#pragma once

#include "ecml_mac.hpp"

namespace ecml::protocol {

enum class EthernetSerializeStatus : uint8_t { Success, BufferTooSmall };
struct EthernetDeserializeResult;

struct EthernetHeader {
  static constexpr std::size_t size{14};
  static constexpr std::size_t ether_type{0x88A4};
  MacAddress dst;
  MacAddress src;

  // writes 14 bytes in destination
  // first 6 bytes is destination MAC address then 6 bytes of source MAC address
  // then 2 bytes of ether_type
  // (ether_type is most likely to be 0x88A4 but for now it can chossed freely)
  // return Success if all bytes were successgully written
  // if destination.size() < 14 returns BufferTooSmall
  [[nodiscard]] EthernetSerializeStatus
  serialize(std::span<std::byte> destination) const noexcept;

  // construct ethernet header from first 14 bytes of source
  [[nodiscard]] static EthernetDeserializeResult
  deserialize(std::span<const std::byte> source) noexcept;
};

struct EthernetDeserializeResult {
  enum class Status : uint8_t { Success, BufferTooSmall, UnsupportedEtherType };
  Status status{Status::Success};
  std::optional<EthernetHeader> header{std::nullopt};
  [[nodiscard]] bool ok() const noexcept { return status == Status::Success; }
};

} // namespace ecml::protocol
