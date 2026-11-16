#pragma once

#include <array>
#include <cassert>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "ecml/ecml_frame.hpp"
#include "ecml/ecml_rawsocket.hpp"

namespace ecml::network {

struct SendResult;
struct ReceiveResult;

template <typename T>
concept PacketTransport =
    requires(const T transport, std::span<const std::byte> tx_buffer,
             std::span<std::byte> rx_buffer) {
      { transport.send(tx_buffer) } -> std::same_as<SendResult>;
      { transport.receive(rx_buffer) } -> std::same_as<ReceiveResult>;
    };

} // namespace ecml::network

namespace ecml::protocol {

enum class CommandType : uint8_t;

} // namespace ecml::protocol

namespace ecml::master {

struct DatagramRequest {
  ecml::protocol::CommandType command{0x00};
  std::span<std::byte> buffer;
  uint32_t address{};
  uint16_t expected_wkc{};
};

struct TransactionResult {
  enum class Status : uint8_t {
    Success,
    EmptyTransaction,
    InvalidCommand,
    PayloadTooLarge,
    FrameOverflow,
    SendError,
    ReceiveError,
    Timeout,
    CorruptedFrame,
    ResponseMismatch,
    WkcMismatch
  };
  Status status{Status::Success};
  int system_errno{};
  uint16_t actual_wkc{0};
  size_t failed_index{0};
  std::chrono::nanoseconds roundtrip_time{0};
  [[nodiscard]] bool ok() const noexcept { return status == Status::Success; }
};

template <network::PacketTransport Transport> class TransactionManager {
public:
  static constexpr size_t max_datagrams_per_frame = 124;
  TransactionManager() = delete;
  TransactionManager(Transport transport, protocol::MacAddress source,
                     protocol::MacAddress destination) noexcept;
  ~TransactionManager() = default;

  TransactionManager(const TransactionManager &other) = delete;
  TransactionManager &operator=(const TransactionManager &other) = delete;

  TransactionManager(TransactionManager &&other) = default;
  TransactionManager &operator=(TransactionManager &&other) = default;

  using Clock = std::chrono::steady_clock;
  using Deadline = Clock::time_point;
  using Duration = std::chrono::nanoseconds;

  [[nodiscard]] TransactionResult
  execute(std::span<const DatagramRequest> requests,
          Deadline deadline) noexcept;
  [[nodiscard]] TransactionResult
  execute(std::span<const DatagramRequest> requests, Duration timeout) noexcept;
  [[nodiscard]] TransactionResult execute(const DatagramRequest &request,
                                          Deadline deadline) noexcept;
  [[nodiscard]] TransactionResult execute(const DatagramRequest &request,
                                          Duration timeout) noexcept;

private:
  Transport transport_;
  static constexpr size_t mac_address_size = 6;
  // std::array instead of protocol::MacAddress until ec-frame is merged into
  // main
  protocol::MacAddress source_;
  protocol::MacAddress destination_;
  uint8_t next_idx_{0};
  static constexpr size_t max_frame_size = 1514;
  std::array<std::byte, max_frame_size> tx_buffer_{};
  std::array<std::byte, max_frame_size> rx_buffer_{};

  struct BuildResult {
    TransactionResult::Status status{TransactionResult::Status::Success};
    std::span<const std::byte> buffer;
  };
  [[nodiscard]] BuildResult
  buildTxFrame(std::span<const DatagramRequest> requests,
               uint8_t base_idx) noexcept;
  [[nodiscard]] TransactionResult sendFrame(std::span<const std::byte> frame,
                                            Deadline deadline) const noexcept;
  [[nodiscard]] TransactionResult
  receiveAndMatchFrame(std::span<const DatagramRequest> requests,
                       uint8_t base_idx, Deadline deadline) noexcept;
  [[nodiscard]] std::optional<TransactionResult>
  verifyAndExtractResponse(std::span<const std::byte> rx_frame,
                           std::span<const DatagramRequest> requests,
                           uint8_t base_idx) const noexcept;
};

template <network::PacketTransport transport>
TransactionManager<transport>::BuildResult
TransactionManager<transport>::buildTxFrame(
    std::span<const DatagramRequest> requests, uint8_t base_idx) noexcept {
  assert(!requests.empty());
  const protocol::EthernetHeader eth_header{.ether_type =
                                                protocol::ethercat_ether_type,
                                            .dst = destination_,
                                            .src = source_};
  auto create_res = protocol::FrameBuilder ::create(eth_header, tx_buffer_);
  assert(create_res.ok());
  auto &builder = *create_res.builder;
  for (size_t i{}; i != requests.size(); ++i) {
    const protocol::DatagramHeader dtg_header{
        .cmd = requests[i].command,
        .idx = static_cast<uint8_t>(base_idx + i),
        .address = requests[i].address,
        .len = static_cast<uint16_t>(requests[i].buffer.size()),
        .more_datagrams = (i + 1 < requests.size()),
        .irq = 0};
    const auto add_status =
        builder.addDatagram(dtg_header, requests[i].buffer, 0);
    if (add_status == protocol::FrameBuilderAddDatagramStatus::InvalidCommand) {
      return {TransactionResult::Status::InvalidCommand,
              std::span<const std::byte>{}};
    }
    if (add_status ==
        protocol::FrameBuilderAddDatagramStatus::PayloadTooLarge) {
      return {TransactionResult::Status::PayloadTooLarge,
              std::span<const std::byte>{}};
    }
    if (add_status == protocol::FrameBuilderAddDatagramStatus::MtuExceeded) {
      return {TransactionResult::Status::FrameOverflow,
              std::span<const std::byte>{}};
    }
  }
  const auto fin_res = builder.finalize();
  return {TransactionResult::Status::Success, fin_res.frame};
}

template <network::PacketTransport transport>
TransactionResult TransactionManager<transport>::sendFrame(
    std::span<const std::byte> frame,
    TransactionManager::Deadline deadline) const noexcept {
  const auto res = transport_.send(frame);
  while (!res.ok()) {
    if (res.wouldBlock() || res.interrupted()) {
      if (Clock::now() >= deadline) {
        return {TransactionResult::Status::Timeout};
      }
    } else {
      return {TransactionResult::Status::SendError, res.system_errno};
    }
  }
  return {TransactionResult::Status::Success};
}

template <network::PacketTransport transport>
std::optional<TransactionResult>
TransactionManager<transport>::verifyAndExtractResponse(
    std::span<const std::byte> rx_frame,
    std::span<const DatagramRequest> requests,
    uint8_t base_idx) const noexcept {
  auto create_res = protocol::FrameParser::create(rx_frame);
  if (!create_res.ok()) {
    if (create_res.status ==
        protocol::FrameParserCreateResult::Status::UnsupportedEtherType) {
      return std::nullopt;
    }
    return TransactionResult{.status =
                                 TransactionResult::Status::CorruptedFrame};
  }
  auto &parser = *create_res.parser;
  std::array<std::span<const std::byte>, max_datagrams_per_frame>
      extracted_payloads{};
  for (size_t i{}; i != requests.size(); ++i) {
    auto next_res = parser.nextDatagram();
    if (next_res.done()) {
      return TransactionResult{.status =
                                   TransactionResult::Status::ResponseMismatch};
    }
    if (!next_res.ok()) {
      return TransactionResult{.status =
                                   TransactionResult::Status::CorruptedFrame,
                               .failed_index = i};
    }
    const auto &datagram = *next_res.datagram;
    const auto &header = datagram.header();
    if (i == 0 && header.idx != base_idx) {
      return std::nullopt;
    }
    if (header.idx != static_cast<uint8_t>(base_idx + i) ||
        header.cmd != requests[i].command ||
        header.address != requests[i].address ||
        datagram.data().size() != requests[i].buffer.size()) {
      return TransactionResult{.status =
                                   TransactionResult::Status::ResponseMismatch,
                               .failed_index = i};
    }
    if (datagram.wkc() != requests[i].expected_wkc) {
      return TransactionResult{.status = TransactionResult::Status::WkcMismatch,
                               .actual_wkc = datagram.wkc(),
                               .failed_index = i};
    }
    extracted_payloads[i] = datagram.data();
  }
  const auto next_res = parser.nextDatagram();
  if (!next_res.done()) {
    return TransactionResult{.status =
                                 TransactionResult::Status::ResponseMismatch};
  }
  for (size_t i{}; i != requests.size(); ++i) {
    std::ranges::copy(extracted_payloads[i], requests[i].buffer.begin());
  }
  return TransactionResult{.status = TransactionResult::Status::Success};
}

template <network::PacketTransport transport>
TransactionResult TransactionManager<transport>::receiveAndMatchFrame(
    std::span<const DatagramRequest> requests, uint8_t base_idx,
    Deadline deadline) noexcept {
  while (Clock::now() < deadline) {
    const auto recv_res = transport_.receive(rx_buffer_);
    if (recv_res.wouldBlock() || recv_res.interrupted()) {
      continue;
    }
    if (recv_res.truncated()) {
      return {.status = TransactionResult::Status::CorruptedFrame};
    }
    if (recv_res.SystemError()) {
      return {.status = TransactionResult::Status::ReceiveError,
              .system_errno = recv_res.system_errno};
    }
    if (recv_res.ok()) {
      auto response = verifyAndExtractResponse(
          std::span<const std::byte>(rx_buffer_).first(recv_res.bytes_stored));
      if (response.has_value()) {
        return *response;
      }
    }
  }
  return {.status = TransactionResult::Status::Timeout};
}

} // namespace ecml::master
