#include <gtest/gtest.h>

#include "ecml/ecml_mac.hpp"

namespace {

const ecml::protocol::MacAddress sample_dst_mac{
    .bytes = {std::byte{0x01}, std::byte{0x02}, std::byte{0x03},
              std::byte{0x04}, std::byte{0x05}, std::byte{0x06}}};

} // namespace

TEST(MacAddressUnit, SerializeSuccessfully) {
  std::array<std::byte, ecml::protocol::MacAddress::size> destination_buffer{};
  const auto status = sample_dst_mac.serialize(destination_buffer);
  EXPECT_EQ(status, ecml::protocol::MacSerializeStatus::Success);
  EXPECT_TRUE(std::equal(sample_dst_mac.bytes.begin(),
                         sample_dst_mac.bytes.end(),
                         destination_buffer.begin()));
}

TEST(MacAddressUnit, SerializeBufferTooSmall) {
  std::array<std::byte, ecml::protocol::MacAddress::size - 1> small_buffer{};
  const auto status = sample_dst_mac.serialize(small_buffer);
  EXPECT_EQ(status, ecml::protocol::MacSerializeStatus::BufferTooSmall);
}

TEST(MacAddressUnit, DeserializeSuccessfully) {
  constexpr std::array<std::byte, 6> bytes{
      static_cast<std::byte>(0xAA), static_cast<std::byte>(0x15),
      static_cast<std::byte>(0x23), static_cast<std::byte>(0xA5),
      static_cast<std::byte>(0x8B), static_cast<std::byte>(0x4A)};

  auto res = ecml::protocol::MacAddress::deserialize(bytes);

  ASSERT_TRUE(res.ok());
  ASSERT_TRUE(res.address.has_value());
  auto mac = *res.address;

  EXPECT_TRUE(std::equal(bytes.begin(), bytes.end(), mac.bytes.begin()));
}

TEST(MacAddressUnit, DeserializeBufferTooSmall) {
  const std::array<std::byte, ecml::protocol::MacAddress::size - 1>
      truncated_bytes{};
  const auto res = ecml::protocol::MacAddress::deserialize(truncated_bytes);
  EXPECT_FALSE(res.ok());
  EXPECT_FALSE(res.address.has_value());
  EXPECT_EQ(res.status,
            ecml::protocol::MacDeserializeResult::Status::BufferTooSmall);
}

TEST(MacAddressUnit, SerializeDeserializeEquality) {
  std::array<std::byte, ecml::protocol::MacAddress::size> buffer{};

  ASSERT_EQ(sample_dst_mac.serialize(buffer),
            ecml::protocol::MacSerializeStatus::Success);
  const auto res = ecml::protocol::MacAddress::deserialize(buffer);
  ASSERT_TRUE(res.ok());
  ASSERT_TRUE(res.address.has_value());
  EXPECT_EQ(*res.address, sample_dst_mac);
}
