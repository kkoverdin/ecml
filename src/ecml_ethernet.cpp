
#include "ecml/ecml_ethernet.hpp"

namespace ecml::protocol {

EthernetSerializeStatus
EthernetHeader::serialize(std::span<std::byte> destination) const noexcept {
  if (destination.size() < EthernetHeader::size) {
    return EthernetSerializeStatus::BufferTooSmall;
  }
  if (dst.serialize(destination) != MacSerializeStatus::Success) {
    return EthernetSerializeStatus::BufferTooSmall;
  }
  if (src.serialize(destination.subspan(MacAddress::size)) !=
      MacSerializeStatus::Success) {
    return EthernetSerializeStatus::BufferTooSmall;
  }
  constexpr size_t ether_type_offset = 2 * MacAddress::size;
  constexpr uint8_t byte_shift = 8;
  constexpr uint8_t lower_byte_mask = 0xFF;
  destination[ether_type_offset] =
      static_cast<std::byte>((ether_type >> byte_shift) & lower_byte_mask);
  destination[ether_type_offset + 1] =
      static_cast<std::byte>(ether_type & lower_byte_mask);
  return EthernetSerializeStatus::Success;
}

EthernetDeserializeResult
EthernetHeader::deserialize(std::span<const std::byte> source) noexcept {
  if (source.size() < EthernetHeader::size) {
    return {EthernetDeserializeResult::Status::BufferTooSmall};
  }
  EthernetHeader res;
  auto dst_mac = MacAddress::deserialize(source);
  if (!dst_mac.address.has_value()) {
    return {EthernetDeserializeResult::Status::BufferTooSmall};
  }
  res.dst = *dst_mac.address;
  auto src_mac = MacAddress::deserialize(source.subspan(MacAddress::size));
  if (!src_mac.address.has_value()) {
    return {EthernetDeserializeResult::Status::BufferTooSmall};
  }
  res.dst = *src_mac.address;
  constexpr size_t ether_type_offset = 2 * MacAddress::size;
  constexpr uint8_t byte_shift = 8;
  const auto msb = std::to_integer<uint16_t>(source[ether_type_offset]);
  const auto lsb = std::to_integer<uint16_t>(source[ether_type_offset + 1]);
  const auto wire_ether_type = static_cast<uint16_t>((msb << byte_shift) | lsb);
  if (wire_ether_type != ethercat_ether_type) {
    return {EthernetDeserializeResult::Status::UnsupportedEtherType};
  }
  res.ether_type = wire_ether_type;
  return {EthernetDeserializeResult::Status::Success, res};
}

} // namespace ecml::protocol
