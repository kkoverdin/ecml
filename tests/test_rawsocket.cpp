#include "ecml/ecml_rawsocket.hpp"

#include <algorithm>
#include <array>
#include <gtest/gtest.h>
#include <unistd.h>

namespace {

constexpr size_t min_frame_size = 60;
constexpr size_t max_frame_size = 1514;
constexpr useconds_t packet_wait_duration = 1000;

} // namespace

TEST(RawSocketUnit, EmptyInterfaceName) {
  // Arrange
  const std::string_view empty_name{};
  // Act
  const auto open_result = ecml::network::RawSocket::open(empty_name);
  // Assert
  EXPECT_FALSE(open_result.ok());
  EXPECT_EQ(open_result.status,
            ecml::network::OpenResult::Status::InvalidInterfaceName);
}

TEST(RawSocketUnit, TooLongInterfaceName) {
  // Arrange
  const std::string_view long_name{"very long interface name"};
  // Act
  const auto open_result = ecml::network::RawSocket::open(long_name);
  // Assert
  EXPECT_FALSE(open_result.ok());
  EXPECT_EQ(open_result.status,
            ecml::network::OpenResult::Status::InvalidInterfaceName);
}

TEST(RawSocketUnit, NonExistingInterfaceName) {
  // Arrange
  const std::string_view non_existing_name{"net takogo"};
  // Act
  const auto open_result = ecml::network::RawSocket::open(non_existing_name);
  // Assert
  EXPECT_FALSE(open_result.ok());
  EXPECT_EQ(open_result.status,
            ecml::network::OpenResult::Status::InterfaceIndexNotFound);
}

TEST(RawSocketUnit, MovingCtor) {
  // Arrange
  const std::string_view interface_name{"veth0"};
  auto open_result = ecml::network::RawSocket::open(interface_name);
  ASSERT_TRUE(open_result.socket.has_value());
  // Act
  auto moved_socket = std::move(*open_result.socket);
  // Assert
  EXPECT_TRUE(moved_socket.isOpen());
  EXPECT_FALSE(open_result.socket->isOpen());
}

TEST(RawSocketUnit, MovingAssignOperator) {
  // Arrange
  auto open_result_source = ecml::network::RawSocket::open("veth0");
  auto open_result_dest = ecml::network::RawSocket::open("veth1");
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
  auto tx_result = ecml::network::RawSocket::open("veth0");
  auto rx_result = ecml::network::RawSocket::open("veth1");
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
  EXPECT_TRUE(send_res.ok());
  EXPECT_EQ(send_res.bytes_sent, frame.size());
  usleep(packet_wait_duration);
  const auto recv_res = rx_socket.receive(rx_buffer);
  // Assert
  EXPECT_TRUE(recv_res.ok());
  EXPECT_EQ(recv_res.bytes_stored, frame.size());
  EXPECT_TRUE(std::equal(frame.begin(), frame.end(), rx_buffer.begin()));
}
