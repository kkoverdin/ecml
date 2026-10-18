#include <gtest/gtest.h>

#include "ecml/ecml_ethercat.hpp"

namespace {

constexpr uint16_t lower_byte_mask = 0x00FF;
constexpr uint8_t byte_shift = 8;
constexpr uint16_t length_mask = 0x07FF;
constexpr uint8_t type_shift = 12;
constexpr uint16_t sample_raw_word = 0x15A3; // length = 1443 type = 1
constexpr uint16_t reserved_bit_mismatch_sample_raw_word = 0x182C;
constexpr uint16_t unsupported_type_sample_raw_word = 0x202C;

} // namespace

TEST(EthercatHeaderUnit, SerializeSuccessfully) {
  const ecml::protocol::EthercatHeader header{
      .length = 1000, .type = ecml::protocol::EthercatHeader::ethercat_type};
  std::array<std::byte, ecml::protocol::EthercatHeader::size> buffer{};
  const auto status = header.serialize(buffer);
  EXPECT_EQ(status, ecml::protocol::EthercatHeaderSerializeStatus::Success);
  const auto lsb = std::to_integer<uint16_t>(buffer[0]);
  const auto msb = std::to_integer<uint16_t>(buffer[1]);
  const auto raw_word = static_cast<uint16_t>(lsb | (msb << byte_shift));
  const uint16_t expected_raw_word =
      (header.length & length_mask) | (header.type << type_shift);
  EXPECT_EQ(raw_word, expected_raw_word);
}

TEST(EthercatHeaderUnit, SerializeBufferTooSmall) {
  const ecml::protocol::EthercatHeader header{
      .length = 1000, .type = ecml::protocol::EthercatHeader::ethercat_type};
  std::array<std::byte, ecml::protocol::EthercatHeader::size - 1>
      small_buffer{};
  const auto status = header.serialize(small_buffer);
  EXPECT_EQ(status,
            ecml::protocol::EthercatHeaderSerializeStatus::BufferTooSmall);
}

TEST(EthercatHeaderUnit, SerializeLengthOutOfRange) {
  const ecml::protocol::EthercatHeader header{
      .length = 1500, .type = ecml::protocol::EthercatHeader::ethercat_type};
  std::array<std::byte, ecml::protocol::EthercatHeader::size> buffer{};
  const auto status = header.serialize(buffer);
  EXPECT_EQ(status,
            ecml::protocol::EthercatHeaderSerializeStatus::LengthOutOfRange);
}

TEST(EthercatHeaderUnit, SerializeUnsupportedProtocolType) {
  const ecml::protocol::EthercatHeader header{.length = 1000, .type = 0x02};
  std::array<std::byte, ecml::protocol::EthercatHeader::size> buffer{};
  const auto status = header.serialize(buffer);
  EXPECT_EQ(
      status,
      ecml::protocol::EthercatHeaderSerializeStatus::UnsupportedProtocolType);
}

TEST(EthercatHeaderUnit, DeserializeSuccessfully) {
  std::array<std::byte, ecml::protocol::EthercatHeader::size> bytes{};
  bytes[0] = static_cast<std::byte>(sample_raw_word & lower_byte_mask);
  bytes[1] =
      static_cast<std::byte>((sample_raw_word >> byte_shift) & lower_byte_mask);
  const auto res = ecml::protocol::EthercatHeader::deserialize(bytes);
  ASSERT_TRUE(res.ok());
  ASSERT_TRUE(res.header.has_value());
  EXPECT_EQ(res.header->length, 1443);
  EXPECT_EQ(res.header->type, 0x01);
  EXPECT_EQ(res.status,
            ecml::protocol::EthercatHeaderDeserializeResult::Status::Success);
}

TEST(EthercatHeaderUnit, DeserializeBufferTooSmall) {
  std::array<std::byte, ecml::protocol::EthercatHeader::size - 1>
      small_buffer{};
  const auto res = ecml::protocol::EthercatHeader::deserialize(small_buffer);
  EXPECT_FALSE(res.ok());
  EXPECT_FALSE(res.header.has_value());
  EXPECT_EQ(
      res.status,
      ecml::protocol::EthercatHeaderDeserializeResult::Status::BufferTooSmall);
}

TEST(EthercatHeaderUnit, DeserializeUnsupportedProtocolType) {
  std::array<std::byte, ecml::protocol::EthercatHeader::size> bytes{};
  bytes[0] = static_cast<std::byte>(unsupported_type_sample_raw_word &
                                    lower_byte_mask);
  bytes[1] = static_cast<std::byte>(
      (unsupported_type_sample_raw_word >> byte_shift) & lower_byte_mask);
  const auto res = ecml::protocol::EthercatHeader::deserialize(bytes);
  EXPECT_EQ(res.status, ecml::protocol::EthercatHeaderDeserializeResult::
                            Status::UnsupportedProtocolType);
  EXPECT_FALSE(res.ok());
  EXPECT_FALSE(res.header.has_value());
}

TEST(EthercatHeaderUnit, DeserializeReservedBitMismatch) {
  std::array<std::byte, ecml::protocol::EthercatHeader::size> bytes{};
  bytes[0] = static_cast<std::byte>(reserved_bit_mismatch_sample_raw_word &
                                    lower_byte_mask);
  bytes[1] = static_cast<std::byte>(
      (reserved_bit_mismatch_sample_raw_word >> byte_shift) & lower_byte_mask);
  const auto res = ecml::protocol::EthercatHeader::deserialize(bytes);
  EXPECT_EQ(res.status, ecml::protocol::EthercatHeaderDeserializeResult::
                            Status::ReservedBitMismatch);
  EXPECT_FALSE(res.ok());
  EXPECT_FALSE(res.header.has_value());
}

TEST(EthercatHeaderUnit, SerializeDeserializeEquality) {
  const ecml::protocol::EthercatHeader original_header{.length = 1000,
                                                       .type = 0x01};
  std::array<std::byte, ecml::protocol::EthercatHeader::size> buffer{};
  EXPECT_EQ(original_header.serialize(buffer),
            ecml::protocol::EthercatHeaderSerializeStatus::Success);
  const auto res = ecml::protocol::EthercatHeader::deserialize(buffer);
  EXPECT_TRUE(res.ok());
  ASSERT_TRUE(res.header.has_value());
  EXPECT_EQ(res.header->length, original_header.length);
  EXPECT_EQ(res.header->type, original_header.type);
}
