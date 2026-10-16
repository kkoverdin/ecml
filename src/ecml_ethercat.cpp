

#include "ecml/ecml_ethercat.hpp"

namespace {

constexpr uint16_t max_ethercat_length = 0x05DA; // 1498
constexpr uint16_t length_mask = 0x07FF;
constexpr uint16_t reserved_bit_mask = 0x0800;
constexpr uint8_t ethercat_protocol_type = 0x01;
constexpr uint8_t type_mask = 0x0F;
constexpr uint8_t type_shift = 12;
constexpr uint8_t byte_shift = 8;
constexpr uint16_t lower_byte_mask = 0x00FF;

} // namespace

namespace ecml::protocol {

EthercatHeaderSerializeStatus
EthercatHeader::serialize(std::span<std::byte> destination) const noexcept {
  if (destination.size() < EthercatHeader::size) {
    return EthercatHeaderSerializeStatus::BufferTooSmall;
  }
  if (length > max_ethercat_length) {
    return EthercatHeaderSerializeStatus::LengthOutOfRange;
  }
  if (type > ethercat_protocol_type) {
    return EthercatHeaderSerializeStatus::UnsupportedProtocolType;
  }
  const uint16_t raw_word = (length & length_mask) | (type << type_shift);
  destination[0] = static_cast<std::byte>(raw_word & lower_byte_mask);
  destination[1] =
      static_cast<std::byte>((raw_word >> byte_shift) & lower_byte_mask);
  return EthercatHeaderSerializeStatus::Success;
}

EthercatHeaderDeserializeResult
EthercatHeader::deserialize(std::span<const std::byte> source) noexcept {
  if (source.size() < EthercatHeader::size) {
    return {EthercatHeaderDeserializeResult::Status::BufferTooSmall};
  }
  const auto lsb = std::to_integer<uint16_t>(source[0]);
  const auto msb = std::to_integer<uint16_t>(source[1]);
  const auto raw_word = static_cast<uint16_t>(lsb | (msb << byte_shift));
  if ((raw_word & reserved_bit_mask) != 0) {
    return {EthercatHeaderDeserializeResult::Status::ReservedBitMismatch};
  }
  const auto extracted_type =
      static_cast<uint8_t>((raw_word >> type_shift) & type_mask);
  if (extracted_type != ethercat_type) {
    return {EthercatHeaderDeserializeResult::Status::UnsupportedProtocolType};
  }
  EthercatHeader header;
  header.length = static_cast<uint16_t>(raw_word & length_mask);
  header.type = extracted_type;
  return {EthercatHeaderDeserializeResult::Status::Success, header};
}

} // namespace ecml::protocol
