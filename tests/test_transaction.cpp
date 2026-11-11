#include <array>
#include <cstddef>
#include <cstring>
#include <gtest/gtest.h>
#include <span>

#include "ecml/ecml_transaction.hpp"

struct MockTransport {
  mutable std::array<std::byte, 1514> wire_buffer{};
  mutable size_t wire_size{};
  std::function<bool(std::span<std::byte>)> handler;
  mutable size_t send_would_block_count{};
  mutable size_t recv_would_block_count{};
  ecml::network::SendResult send(std::span<const std::byte> tx_buffer) const {
    if (send_would_block_count > 0) {
      --send_would_block_count;
      return {ecml::network::SendResult::Status::WouldBlock};
    }
    std::memcpy(wire_buffer.data(), tx_buffer.data(), tx_buffer.size());
    wire_size = tx_buffer.size();
    return {ecml::network::SendResult::Status::Success, tx_buffer.size()};
  }
  ecml::network::ReceiveResult receive(std::span<std::byte> rx_buffer) const {
    if (recv_would_block_count > 0) {
      --recv_would_block_count;
      return {ecml::network::ReceiveResult::Status::WouldBlock};
    }
    std::memcpy(rx_buffer.data(), wire_buffer.data(), wire_size);
    return {ecml::network::ReceiveResult::Status::Success, wire_size,
            wire_size};
  }
};

static_assert(ecml::network::PacketTransport<MockTransport>);
