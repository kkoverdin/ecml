#include <gtest/gtest.h>

#include "ecml/ecml_datagram.hpp"

TEST(DatagramHeaderUnit, SerializeSuccesfully) {
  const ecml::protocol::DatagramHeader header{
      .cmd = ecml::protocol::CommandType::APRD,
      .idx = 0x2A,
      .address = 0x01300000,
      .len = 0x0002,
      .more_datagrams = false,
      .circulating = false,
      .irq = 0x0000};
  const std::array<std::byte, ecml::protocol::DatagramHeader::size>
      expected_buffer{std::byte{0x01}, std::byte{0x2A}, std::byte{0x00},
                      std::byte{0x00}, std::byte{0x30}, std::byte{0x01},
                      std::byte{0x02}, std::byte{0x00}, std::byte{0x00},
                      std::byte{0x00}};
  std::array<std::byte, ecml::protocol::DatagramHeader::size> buffer{};
  const auto status = header.serialize(buffer);
  EXPECT_EQ(status, ecml::protocol::DatagramHeaderSerializeStatus::Success);
  EXPECT_TRUE(
      std::equal(buffer.begin(), buffer.end(), expected_buffer.begin()));
}

TEST(DatagramHeaderUnit, SerializeBufferTooSmall) {
  const ecml::protocol::DatagramHeader header{
      .cmd = ecml::protocol::CommandType::APRD,
      .idx = 0x2A,
      .address = 0x01300000,
      .len = 0x0002,
      .more_datagrams = false,
      .circulating = false,
      .irq = 0x0000};
  std::array<std::byte, ecml::protocol::DatagramHeader::size - 1> buffer{};
  const auto status = header.serialize(buffer);
  EXPECT_EQ(status,
            ecml::protocol::DatagramHeaderSerializeStatus::BufferTooSmall);
}

TEST(DatagramHeaderUnit, SerializeLengthOutOfRange) {
  const ecml::protocol::DatagramHeader header{
      .cmd = ecml::protocol::CommandType::APRD,
      .idx = 0x2A,
      .address = 0x01300000,
      .len = ecml::protocol::datagram_max_len + 1,
      .more_datagrams = false,
      .circulating = false,
      .irq = 0x0000};
  std::array<std::byte, ecml::protocol::DatagramHeader::size> buffer{};
  const auto status = header.serialize(buffer);
  EXPECT_EQ(status,
            ecml::protocol::DatagramHeaderSerializeStatus::LengthOutOfRange);
}

TEST(DatagramHeaderUnit, DeserializeSuccessfully) {
  const std::array<std::byte, ecml::protocol::DatagramHeader::size> bytes{
      std::byte{0x01}, std::byte{0x2A}, std::byte{0x00}, std::byte{0x00},
      std::byte{0x30}, std::byte{0x01}, std::byte{0x02}, std::byte{0x00},
      std::byte{0x00}, std::byte{0x00}};
  const auto res = ecml::protocol::DatagramHeader::deserialize(bytes);
  ASSERT_TRUE(res.ok());
  ASSERT_TRUE(res.header.has_value());
  EXPECT_EQ(res.header->cmd, ecml::protocol::CommandType::APRD);
  EXPECT_EQ(res.header->idx, 0x2A);
  EXPECT_EQ(res.header->address, 0x01300000);
  EXPECT_EQ(res.header->len, 2);
  EXPECT_FALSE(res.header->circulating);
  EXPECT_FALSE(res.header->more_datagrams);
  EXPECT_EQ(res.header->irq, 0x0000);
  EXPECT_EQ(res.status,
            ecml::protocol::DatagramHeaderDeserializeResult::Status::Success);
}

TEST(DatagramHeaderUnit, DeserializeBufferTooSmall) {
  const std::array<std::byte, ecml::protocol::DatagramHeader::size - 1> bytes{
      std::byte{0x01}, std::byte{0x2A}, std::byte{0x00},
      std::byte{0x00}, std::byte{0x30}, std::byte{0x01},
      std::byte{0x02}, std::byte{0x00}, std::byte{0x00}};
  const auto res = ecml::protocol::DatagramHeader::deserialize(bytes);
  EXPECT_FALSE(res.ok());
  ASSERT_FALSE(res.header.has_value());
  EXPECT_EQ(
      res.status,
      ecml::protocol::DatagramHeaderDeserializeResult::Status::BufferTooSmall);
}

TEST(DatagramHeaderUnit, DeserializeUnknownCommand) {
  const std::array<std::byte, ecml::protocol::DatagramHeader::size> bytes{
      std::byte{0x0F}, std::byte{0x01}, std::byte{0x00}, std::byte{0x00},
      std::byte{0x00}, std::byte{0x00}, std::byte{0x04}, std::byte{0x00},
      std::byte{0x00}, std::byte{0x00}};
  const auto res = ecml::protocol::DatagramHeader::deserialize(bytes);
  EXPECT_FALSE(res.ok());
  ASSERT_FALSE(res.header.has_value());
  EXPECT_EQ(
      res.status,
      ecml::protocol::DatagramHeaderDeserializeResult::Status::UnknownCommand);
}

TEST(DatagramHeaderUnit, DeserializeReservedBitsMismatch) {
  const std::array<std::byte, ecml::protocol::DatagramHeader::size> bytes{
      std::byte{0x01}, std::byte{0x01}, std::byte{0x00}, std::byte{0x00},
      std::byte{0x00}, std::byte{0x00}, std::byte{0x02}, std::byte{0x08},
      std::byte{0x00}, std::byte{0x00}};
  const auto res = ecml::protocol::DatagramHeader::deserialize(bytes);
  EXPECT_FALSE(res.ok());
  ASSERT_FALSE(res.header.has_value());
  EXPECT_EQ(res.status, ecml::protocol::DatagramHeaderDeserializeResult::
                            Status::ReservedBitsMismatch);
}

TEST(DatagramHeaderUnit, DeserializeLengthOutOfRange) {
  const std::array<std::byte, ecml::protocol::DatagramHeader::size> bytes{
      std::byte{0x01}, std::byte{0x01}, std::byte{0x00}, std::byte{0x00},
      std::byte{0x00}, std::byte{0x00}, std::byte{0xCF}, std::byte{0x05},
      std::byte{0x00}, std::byte{0x00}};
  const auto res = ecml::protocol::DatagramHeader::deserialize(bytes);
  EXPECT_FALSE(res.ok());
  ASSERT_FALSE(res.header.has_value());
  EXPECT_EQ(res.status, ecml::protocol::DatagramHeaderDeserializeResult::
                            Status::LengthOutOfRange);
}
