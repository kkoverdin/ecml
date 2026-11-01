#include <gtest/gtest.h>

#include "ecml/ecml_mac.hpp"

TEST(MacAddressUnit, DeserializeSuccessfully) {
  constexpr std::array<std::byte, 6> bytes{
      static_cast<std::byte>(0xAA), static_cast<std::byte>(0x15),
      static_cast<std::byte>(0x23), static_cast<std::byte>(0xA5),
      static_cast<std::byte>(0x8B), static_cast<std::byte>(0x4A)};

  auto res = ecml::protocol::MacAddress::deserialize(bytes);

  ASSERT_TRUE(res.ok());
  auto mac = *res.address;

  EXPECT_TRUE(std::equal(bytes.begin(), bytes.end(), mac.bytes.begin()));
}
