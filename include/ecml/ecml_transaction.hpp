#pragma once

#include <algorithm>
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

template <typename T>
concept PacketTransport =
    requires(T transport, std::span<const std::byte> tx_buffer,
             std::span<std::byte> rx_buffer) {
      { transport.send(tx_buffer) } -> std::same_as<SendResult>;
      { transport.receive(rx_buffer) } -> std::same_as<ReceiveResult>;
    };

} // namespace ecml::network

namespace ecml::master {

struct DatagramRequest {
  ecml::protocol::CommandType command{0x00};
  std::span<std::byte> buffer;
  uint32_t address{};
  uint16_t expected_wkc{};
};

struct SendResult {
  enum class Status : uint8_t {
    Success,
    SendError,
    Busy,
    InvalidCommand,
    PayloadTooLarge,
    FrameOverflow,
  };
  Status status{Status::Success};
  size_t failed_index{};
  int system_errno{};
  [[nodiscard]] bool ok() const noexcept { return status == Status::Success; }
};

struct PollResult {
  enum class Status : uint8_t {
    Success,
    Timeout,
    ReceiveError,
    CorruptedFrame,
    ResponseMismatch,
    WkcMismatch,
    Pending,
    Idle,
    SendError
  };
  Status status{Status::Success};
  uint16_t actual_wkc{};
  int system_errno{};
  size_t failed_index{};
  std::chrono::nanoseconds roundtrip_time{};
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

  SendResult send(const DatagramRequest &&, Deadline deadline) = delete;
  // if requests lifetime is ended before poll behaviour is undefined
  [[nodiscard]] SendResult send(std::span<const DatagramRequest> requests,
                                Deadline deadline) noexcept;
  [[nodiscard]] PollResult poll() noexcept;

private:
  Transport transport_;
  static constexpr size_t mac_address_size = 6;
  protocol::MacAddress source_;
  protocol::MacAddress destination_;
  uint8_t next_idx_{0};
  static constexpr size_t max_frame_size = 1514;
  std::array<std::byte, max_frame_size> tx_buffer_{};
  std::array<std::byte, max_frame_size> rx_buffer_{};
  enum class State : uint8_t { Idle, PendingSend, PendingReceive };
  State state_{State::Idle};
  size_t active_tx_size_{};
  size_t active_base_idx_{};
  Deadline active_deadline_;
  Deadline active_start_time_;
  std::span<const DatagramRequest> requests_;
  struct BuildResult {
    SendResult::Status status{SendResult::Status::Success};
    std::span<const std::byte> buffer;
    size_t failed_index{};
  };
  [[nodiscard]] BuildResult
  buildTxFrame(std::span<const DatagramRequest> requests,
               uint8_t base_idx) noexcept;
  // returns std::nullopt if received frame is not ethercat type
  [[nodiscard]] std::optional<PollResult>
  verifyAndExtractResponse(std::span<const std::byte> rx_frame,
                           std::span<const DatagramRequest> requests,
                           uint8_t base_idx) const noexcept;
};

template <network::PacketTransport transport>
SendResult
TransactionManager<transport>::send(std::span<const DatagramRequest> requests,
                                    Deadline deadline) noexcept {
  if (state_ != State::Idle) {
    return {SendResult::Status::Busy};
  }
  const auto build_res = buildTxFrame(requests, next_idx_);
  if (build_res.status != SendResult::Status::Success) {
    return build_res;
  }
  const auto send_res = transport_.send(build_res.buffer);
  if (send_res.status == network::SendResult::Status::SystemError ||
      send_res.status == network::SendResult::Status::BadSend) {
    return {SendResult::Status::SendError, 0, send_res.system_errno};
  }
  active_tx_size_ = build_res.buffer.size();
  active_deadline_ = deadline;
  active_base_idx_ = next_idx_;
  next_idx_ += requests.size();
  requests_ = requests;
  if (send_res.ok()) {
    active_start_time_ = Clock::now();
    state_ = State::PendingReceive;
  } else {
    state_ = State::PendingSend;
  }
  return {SendResult::Status::Success};
}

template <network::PacketTransport transport>
PollResult TransactionManager<transport>::poll() noexcept {
  switch (state_) {
  case State::Idle:
    return {PollResult::Status::Idle};
  case State::PendingSend: {
    if (Clock::now() >= active_deadline_) {
      state_ = State::Idle;
      requests_ = std::span<const DatagramRequest>{};
      return {PollResult::Status::Timeout};
    }
    const auto send_res = transport_.send(
        std::span<const std::byte>{tx_buffer_.data(), active_tx_size_});
    if (send_res.status == network::SendResult::Status::SystemError ||
        send_res.status == network::SendResult::Status::BadSend) {
      state_ = State::Idle;
      requests_ = std::span<const DatagramRequest>{};
      return {PollResult::Status::SendError, 0, send_res.system_errno};
    }
    if (send_res.ok()) {
      active_start_time_ = Clock::now();
      state_ = State::PendingReceive;
    }
    return {PollResult::Status::Pending};
  }
  case State::PendingReceive: {
    if (Clock::now() >= active_deadline_) {
      state_ = State::Idle;
      requests_ = std::span<const DatagramRequest>{};
      return {PollResult::Status::Timeout};
    }
    const auto recv_res = transport_.receive(rx_buffer_);
    if (recv_res.status == network::ReceiveResult::Status::Truncated ||
        recv_res.status == network::ReceiveResult::Status::SystemError) {
      state_ = State::Idle;
      requests_ = std::span<const DatagramRequest>{};
      return {PollResult::Status::ReceiveError, 0, recv_res.system_errno};
    }
    if (!recv_res.ok()) {
      return {PollResult::Status::Pending};
    }
    const auto response = verifyAndExtractResponse(
        std::span<std::byte>{rx_buffer_.data(), recv_res.bytes_stored},
        requests_, active_base_idx_);
    if (!response.has_value()) {
      return {PollResult::Status::Pending};
    }
    state_ = State::Idle;
    requests_ = std::span<const DatagramRequest>{};
    if (response->status != PollResult::Status::Success) {
      return response.value();
    }
    return {PollResult::Status::Success, 0, 0, 0,
            Clock::now() - active_start_time_};
  }
  }
}

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
    if (requests[i].buffer.size() > protocol::Datagram::max_payload_size) {
      return {SendResult::Status::PayloadTooLarge, std::span<const std::byte>{},
              i};
    }
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
      return {SendResult::Status::InvalidCommand, std::span<const std::byte>{},
              i};
    }
    if (add_status == protocol::FrameBuilderAddDatagramStatus::MtuExceeded) {
      return {SendResult::Status::FrameOverflow, std::span<const std::byte>{},
              i};
    }
  }
  const auto fin_res = builder.finalize();
  return {SendResult::Status::Success, *fin_res.frame};
}

template <network::PacketTransport transport>
std::optional<PollResult>
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
    return PollResult{.status = PollResult::Status::CorruptedFrame};
  }
  auto &parser = *create_res.parser;
  std::array<std::span<const std::byte>, max_datagrams_per_frame>
      extracted_payloads{};
  for (size_t i{}; i != requests.size(); ++i) {
    auto next_res = parser.nextDatagram();
    if (next_res.done()) {
      return PollResult{.status = PollResult::Status::ResponseMismatch};
    }
    if (!next_res.ok()) {
      return PollResult{.status = PollResult::Status::CorruptedFrame,
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
      return PollResult{.status = PollResult::Status::ResponseMismatch,
                        .failed_index = i};
    }
    if (datagram.wkc() != requests[i].expected_wkc) {
      return PollResult{.status = PollResult::Status::WkcMismatch,
                        .actual_wkc = datagram.wkc(),
                        .failed_index = i};
    }
    extracted_payloads[i] = datagram.data();
  }
  const auto next_res = parser.nextDatagram();
  if (!next_res.done()) {
    return PollResult{.status = PollResult::Status::ResponseMismatch};
  }
  for (size_t i{}; i != requests.size(); ++i) {
    std::ranges::copy(extracted_payloads[i], requests[i].buffer.begin());
  }
  return PollResult{.status = PollResult::Status::Success};
}

} // namespace ecml::master
