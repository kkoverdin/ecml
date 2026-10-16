#include "ecml/ecml_datagram.hpp"

namespace {

constexpr uint8_t lower_byte_mask = 0xFF;
constexpr uint8_t lower_three_bits_mask = 0x07;
constexpr uint8_t byte_shift = 8;
constexpr uint8_t sixth_bit_mask = 0x40;
constexpr uint8_t seventh_bit_mask = 0x80;
constexpr uint8_t max_command_type = 0x0E;
constexpr uint8_t reserved_bits_mask = 0x38;

} // namespace

namespace ecml::protocol {

DatagramHeaderSerializeStatus
DatagramHeader::serialize(std::span<std::byte> destination) const noexcept {
  if (len > datagram_max_len) {
    return DatagramHeaderSerializeStatus::LengthOutOfRange;
  }
  if (destination.size() < DatagramHeader::size) {
    return DatagramHeaderSerializeStatus::BufferTooSmall;
  }
  destination[0] = static_cast<std::byte>(static_cast<std::uint8_t>(cmd));
  destination[1] = static_cast<std::byte>(idx);
  destination[2] = static_cast<std::byte>(address & lower_byte_mask);
  destination[3] =
      static_cast<std::byte>((address >> byte_shift) & lower_byte_mask);
  destination[4] =
      static_cast<std::byte>((address >> (2 * byte_shift)) & lower_byte_mask);
  // NOLINTNEXTLINE
  destination[5] =
      static_cast<std::byte>((address >> (3 * byte_shift)) & lower_byte_mask);
  // NOLINTNEXTLINE
  destination[6] = static_cast<std::byte>(len & lower_byte_mask);
  // NOLINTNEXTLINE
  destination[7] =
      static_cast<std::byte>(((len >> byte_shift) & lower_three_bits_mask) |
                             (circulating ? sixth_bit_mask : 0x00) |
                             (more_datagrams ? seventh_bit_mask : 0x00));
  // NOLINTNEXTLINE
  destination[8] = static_cast<std::byte>(irq & lower_byte_mask);
  // NOLINTNEXTLINE
  destination[9] =
      static_cast<std::byte>((irq >> byte_shift) & lower_byte_mask);
  return DatagramHeaderSerializeStatus::Success;
}

DatagramHeaderDeserializeResult
DatagramHeader::deserialize(std::span<const std::byte> source) noexcept {
  if (source.size() < DatagramHeader::size) {
    return {DatagramHeaderDeserializeResult::Status::BufferTooSmall};
  }
  DatagramHeader header;
  if (auto command_byte = static_cast<uint8_t>(source[0]);
      command_byte > max_command_type) {
    return {DatagramHeaderDeserializeResult::Status::UnknownCommand};
  }
  header.cmd = static_cast<CommandType>(source[0]);
  header.idx = static_cast<uint8_t>(source[1]);
  header.address = static_cast<uint32_t>(source[2]) |
                   (static_cast<uint32_t>(source[3]) << byte_shift) |
                   (static_cast<uint32_t>(source[4]) << 2 * byte_shift) |
                   // NOLINTNEXTLINE
                   (static_cast<uint32_t>(source[5]) << 3 * byte_shift);
  // NOLINTNEXTLINE
  if ((static_cast<uint8_t>(source[7]) & reserved_bits_mask) != 0) {
    return {DatagramHeaderDeserializeResult::Status::ReservedBitsMismatch};
  }
  // NOLINTNEXTLINE
  uint16_t len = static_cast<uint16_t>(source[6]) |
                 // NOLINTNEXTLINE
                 ((static_cast<uint16_t>(source[7]) & lower_three_bits_mask)
                  << byte_shift);
  if (len > datagram_max_len) {
    return {DatagramHeaderDeserializeResult::Status::LengthOutOfRange};
  }
  header.len = len;
  // NOLINTNEXTLINE
  header.circulating = (static_cast<uint8_t>(source[7]) & sixth_bit_mask) != 0;
  header.more_datagrams =
      // NOLINTNEXTLINE
      (static_cast<uint8_t>(source[7]) & seventh_bit_mask) != 0;
  // NOLINTNEXTLINE
  header.irq = static_cast<uint16_t>(source[8]) |
               // NOLINTNEXTLINE
               (static_cast<uint16_t>(source[9]) << byte_shift);
  return {DatagramHeaderDeserializeResult::Status::Success, header};
}

} // namespace ecml::protocol
