#include "ecml/ecml_rawsocket.hpp"

#include <algorithm>
#include <array>
#include <gtest/gtest.h>
#include <unistd.h>

namespace {

constexpr size_t min_frame_size = 60;
constexpr size_t max_frame_size = 1514;
constexpr useconds_t packet_wait_duration = 1000;
constexpr size_t truncated_buffer_size = 30;

constexpr std::string_view tx_interface = "veth0";
constexpr std::string_view rx_interface = "veth1";
constexpr std::string_view non_existing_interface_name = "invalid_if42";

constexpr std::string_view too_long_interface_name = "0123456789abcdef";

} // namespace

TEST(RawSocketUnit, EmptyInterfaceName) {
  // Arrange
  const std::string_view empty_name{};
  // Act
  const auto open_result = ecml::network::RawSocket::open(empty_name);
  // Assert
  EXPECT_FALSE(open_result.ok());
  EXPECT_FALSE(open_result.socket.has_value());
  EXPECT_EQ(open_result.status,
            ecml::network::OpenResult::Status::InvalidInterfaceName);
}

TEST(RawSocketUnit, TooLongInterfaceName) {
  // Arrange
  const std::string_view long_name{too_long_interface_name};
  // Act
  const auto open_result = ecml::network::RawSocket::open(long_name);
  // Assert
  EXPECT_FALSE(open_result.ok());
  EXPECT_FALSE(open_result.socket.has_value());
  EXPECT_EQ(open_result.status,
            ecml::network::OpenResult::Status::InvalidInterfaceName);
}

TEST(RawSocketUnit, NonExistingInterfaceName) {
  // Arrange
  const std::string_view non_existing_name{non_existing_interface_name};
  // Act
  const auto open_result = ecml::network::RawSocket::open(non_existing_name);
  // Assert
  EXPECT_FALSE(open_result.ok());
  EXPECT_FALSE(open_result.socket.has_value());
  EXPECT_EQ(open_result.status,
            ecml::network::OpenResult::Status::InterfaceIndexNotFound);
  EXPECT_EQ(open_result.system_errno, ENODEV);
}

TEST(RawSocketUnit, EmptyBuffers) {
  // Arrange
  auto open_result = ecml::network::RawSocket::open(tx_interface);
  ASSERT_TRUE(open_result.socket.has_value());
  const auto &socket = *open_result.socket;
  // Act
  const auto send_result = socket.send({});
  std::span<std::byte> empty_dest{};
  const auto recv_result = socket.receive(empty_dest);
  // Assert
  EXPECT_FALSE(send_result.ok());
  EXPECT_EQ(send_result.status, ecml::network::SendResult::Status::EmptyBuffer);
  EXPECT_EQ(send_result.bytes_sent, 0);
  EXPECT_FALSE(recv_result.ok());
  EXPECT_EQ(recv_result.status,
            ecml::network::ReceiveResult::Status::EmptyBuffer);
  EXPECT_EQ(recv_result.bytes_stored, 0);
  EXPECT_EQ(recv_result.wire_length, 0);
}

TEST(RawSocketUnit, MovedFromSocketOperations) {
  // Arrange
  auto open_result = ecml::network::RawSocket::open(tx_interface);
  ASSERT_TRUE(open_result.socket.has_value());
  auto &socket_source = *open_result.socket;
  // Act
  ecml::network::RawSocket target_socket = std::move(socket_source);
  EXPECT_TRUE(target_socket.isOpen());
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_FALSE(socket_source.isOpen());
  std::array<std::byte, min_frame_size> frame{};
  const auto send_result = socket_source.send(frame);
  std::array<std::byte, min_frame_size> rx_buffer{};
  const auto recv_result = socket_source.receive(rx_buffer);
  // Assert
  EXPECT_FALSE(send_result.ok());
  EXPECT_EQ(send_result.status,
            ecml::network::SendResult::Status::InvalidSocket);
  EXPECT_FALSE(recv_result.ok());
  EXPECT_EQ(recv_result.status,
            ecml::network::ReceiveResult::Status::InvalidSocket);
}

TEST(RawSocketUnit, MovingCtor) {
  // Arrange
  auto open_result = ecml::network::RawSocket::open(tx_interface);
  ASSERT_TRUE(open_result.socket.has_value());
  // Act
  auto moved_socket = std::move(*open_result.socket);
  // Assert
  EXPECT_TRUE(moved_socket.isOpen());
  EXPECT_FALSE(open_result.socket->isOpen());
}

TEST(RawSocketUnit, MovingAssignOperator) {
  // Arrange
  auto open_result_source = ecml::network::RawSocket::open(tx_interface);
  auto open_result_dest = ecml::network::RawSocket::open(rx_interface);
  ASSERT_TRUE(open_result_source.socket.has_value());
  ASSERT_TRUE(open_result_dest.socket.has_value());
  auto &socket_source = *open_result_source.socket;
  auto &socket_dest = *open_result_dest.socket;
  // Act
  socket_dest = std::move(socket_source);
  // Assert
  EXPECT_TRUE(socket_dest.isOpen());
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_FALSE(socket_source.isOpen());
}

TEST(RawSocketIntegration, SendAndReceiveEthernetFrame) {
  // Arrange
  auto tx_result = ecml::network::RawSocket::open(tx_interface);
  auto rx_result = ecml::network::RawSocket::open(rx_interface);
  ASSERT_TRUE(tx_result.socket.has_value());
  ASSERT_TRUE(rx_result.socket.has_value());
  auto &tx_socket = *tx_result.socket;
  auto &rx_socket = *rx_result.socket;
  std::array<std::byte, min_frame_size> frame{std::byte{0xAA}};
  frame[0] = frame[1] = frame[2] = frame[3] = frame[4] = frame[5] =
      std::byte{0xFF};
  frame[6] = std::byte{0x02};
  frame[7] = frame[8] = frame[9] = frame[10] = std::byte{0x00};
  frame[11] = std::byte{0x01};
  frame[12] = std::byte{0x88};
  frame[13] = std::byte{0xA4};
  std::array<std::byte, max_frame_size> rx_buffer{};
  // Act
  const auto send_res = tx_socket.send(frame);
  usleep(packet_wait_duration);
  const auto recv_res = rx_socket.receive(rx_buffer);
  // Assert
  EXPECT_TRUE(send_res.ok());
  EXPECT_EQ(send_res.bytes_sent, frame.size());
  EXPECT_TRUE(recv_res.ok());
  EXPECT_EQ(recv_res.bytes_stored, frame.size());
  EXPECT_TRUE(std::equal(frame.begin(), frame.end(), rx_buffer.begin()));
}

TEST(RawSocketIntegration, ReceiveTruncatedFrame) {
  // Arrange
  auto tx_result = ecml::network::RawSocket::open(tx_interface);
  auto rx_result = ecml::network::RawSocket::open(rx_interface);
  ASSERT_TRUE(tx_result.socket.has_value());
  ASSERT_TRUE(rx_result.socket.has_value());
  const auto &tx_socket = *tx_result.socket;
  const auto &rx_socket = *rx_result.socket;
  std::array<std::byte, min_frame_size> frame{std::byte{0xBB}};
  frame[0] = frame[1] = frame[2] = frame[3] = frame[4] = frame[5] =
      std::byte{0xFF};
  frame[12] = std::byte{0x88};
  frame[13] = std::byte{0xA4};
  std::array<std::byte, truncated_buffer_size> small_rx_buffer{};
  // Act
  const auto send_result = tx_socket.send(frame);
  usleep(packet_wait_duration);
  const auto recv_result = rx_socket.receive(small_rx_buffer);
  // Assert
  EXPECT_TRUE(send_result.ok());
  EXPECT_EQ(send_result.bytes_sent, frame.size());
  EXPECT_FALSE(recv_result.ok());
  EXPECT_TRUE(recv_result.truncated());
  EXPECT_EQ(recv_result.status,
            ecml::network::ReceiveResult::Status::Truncated);
  EXPECT_EQ(recv_result.bytes_stored, small_rx_buffer.size());
  EXPECT_EQ(recv_result.wire_length, frame.size());
  EXPECT_TRUE(std::equal(small_rx_buffer.begin(), small_rx_buffer.end(),
                         frame.begin()));
}
