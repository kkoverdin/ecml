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
  uint16_t actual_wkc{};
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
  std::chrono::nanoseconds roundtrip_time{0};
  [[nodiscard]] bool ok() const noexcept { return status == Status::Success; }
};

template <network::PacketTransport Transport> class TransactionManager {
public:
  TransactionManager() = delete;
  ~TransactionManager() = default;

  TransactionManager(const TransactionManager &other) = delete;
  TransactionManager &operator=(const TransactionManager &other) = delete;

  TransactionManager(TransactionManager &&other) = default;
  TransactionManager &operator=(TransactionManager &&other) = default;

  using Clock = std::chrono::steady_clock;
  using Deadline = Clock::time_point;
  using Duration = std::chrono::nanoseconds;

  [[nodiscard]] TransactionResult execute(std::span<DatagramRequest> requests,
                                          Deadline deadline) noexcept;
  [[nodiscard]] TransactionResult execute(std::span<DatagramRequest> requests,
                                          Duration timeout) noexcept;
  [[nodiscard]] TransactionResult execute(DatagramRequest &request,
                                          Deadline deadline) noexcept;
  [[nodiscard]] TransactionResult execute(DatagramRequest &request,
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
  receiveAndMatchFrame(std::span<DatagramRequest> requests, uint8_t base_idx,
                       Deadline deadline) const noexcept;
  [[nodiscard]] std::optional<TransactionResult::Status>
  verifyAndExtractResponse(std::span<const std::byte> rx_frame,
                           std::span<DatagramRequest> requests,
                           uint8_t base_idx) const noexcept;
};

} // namespace ecml::master
