#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace ecml::network {

struct SendResult;
struct ReceiveResult;

template <typename T>
concept PacketTransport =
    requires(T transport, std::span<const std::byte> tx_buffer,
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
  uint16_t expected_wkc;
  uint16_t actual_wkc;
};

} // namespace ecml::master
