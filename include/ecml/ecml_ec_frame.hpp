#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace ecml::protocol {

struct MacAddress {
  std::array<std::byte, 6> bytes{};
  auto operator<=>(const MacAddress &other) const = default;
};

struct EthernetHeader {
  MacAddress src;
  MacAddress dst;
  uint16_t ether_type{0x88A4};
};

struct EthercatHeader {
  static constexpr std::size_t kHeaderSize{2};
  static constexpr uint8_t kEthercatType{0x01};

  uint16_t length{0}; // 0...2047
  uint8_t type{kEthercatType};

  // writes two bytes in destination
  // first 11 bits is length then 1 reserved bit then 4 bits for type
  void serialize(std::span<std::byte> destination) const noexcept;

  // constructs ethercat header from first two bytes of source
  [[nodiscard]] static EthercatHeader
  deserialize(std::span<const std::byte> source) noexcept;
};

} // namespace ecml::protocol
