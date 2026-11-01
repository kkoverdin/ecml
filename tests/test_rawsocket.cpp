#include "ecml/ecml_rawsocket.hpp"

#include <array>
#include <gtest/gtest.h>

TEST(RawSocketUnit, EmptyInterfaceName) {
  // Arrange
  std::string_view empty{};
  // Act
  auto rs = ecml::network::RawSocket::open(empty);
  // Assert
  EXPECT_FALSE(rs.has_value());
}

TEST(RawSocketUnit, TooLongInterfaceName) {
  // Arrange
  std::string_view long_name{"too long interface name"};
  // Act
  auto rs = ecml::network::RawSocket::open(long_name);
  // Assert
  EXPECT_FALSE(rs.has_value());
}

TEST(RawSocketUnit, NonExistingInterfaceName) {
  // Arrange
  std::string_view name{"invalid_dev_42"};
  // Act
  auto rs = ecml::network::RawSocket::open(name);
  // Assert
  EXPECT_FALSE(rs.has_value());
}

TEST(RawSocketUnit, MovingCtor) {
  // Arrange
  std::string_view name{"veth0"};
  auto rs = ecml::network::RawSocket::open(name);
  if (!rs.has_value()) {
    GTEST_SKIP() << "Skipping: requires root permissions";
  }
  // Act
  auto moved_rs = ecml::network::RawSocket(std::move(rs.value()));
  // Assert
  EXPECT_FALSE(rs.value().isOpen());
  EXPECT_TRUE(moved_rs.isOpen());
}

TEST(RawSocketUnit, MovingAssignOperator) {
  // Arrange
  std::string_view name1{"veth0"};
  std::string_view name2{"veth1"};
  auto sock1 = ecml::network::RawSocket::open(name1);
  auto sock2 = ecml::network::RawSocket::open(name2);
  if (!sock1.has_value() || !sock2.has_value()) {
    GTEST_SKIP() << "Skipping: requires root permissions";
  }
  // Act
  auto &s1 = sock1.value();
  auto &s2 = sock2.value();
  s2 = std::move(s1);
  // Assert
  EXPECT_TRUE(s2.isOpen());
  EXPECT_FALSE(s1.isOpen());
}

TEST(RawSocketIntegration, SendAndReceiveEthernetFrame) {
  // Arrange
  auto tx_sock = ecml::network::RawSocket::open("veth0");
  auto rx_sock = ecml::network::RawSocket::open("veth1");
  if (!tx_sock.has_value() || !rx_sock.has_value()) {
    GTEST_SKIP() << "Skipping: requires root permissions";
  }
  std::array<std::byte, 60> frame{std::byte{0xAA}};
  frame[0] = frame[1] = frame[2] = frame[3] = frame[4] = frame[5] =
      std::byte{0xFF};
  frame[6] = std::byte{0x02};
  frame[7] = frame[8] = frame[9] = frame[10] = std::byte{0x00};
  frame[11] = std::byte{0x01};
  frame[12] = std::byte{0x88};
  frame[13] = std::byte{0xA4};
  std::array<std::byte, 1514> rx_buffer{};
  // Act
  auto send_res = tx_sock.value().send(frame);
  usleep(1000);
  auto rec_res = rx_sock.value().receive(rx_buffer);
  // Assert
  EXPECT_EQ(rec_res, 60);
  EXPECT_TRUE(std::equal(frame.begin(), frame.end(), rx_buffer.begin()));
}
