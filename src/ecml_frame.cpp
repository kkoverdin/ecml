#include "ecml/ecml_frame.hpp"

namespace {

constexpr size_t min_ethernet_frame_size = 60;
constexpr uint8_t max_command_type = 0x0E;
constexpr size_t max_datagrams_total_bytes = 1498;
constexpr uint8_t lower_byte_mask = 0xFF;
constexpr uint8_t byte_shift = 8;
constexpr uint8_t seventh_bit_mask = 0x80;
constexpr uint8_t more_datagrams_flag_byte_offset = 7;

} // namespace

namespace ecml::protocol {

FrameBuilder::FrameBuilder(std::span<std::byte> buffer) noexcept
    : raw_frame_{buffer} {}

FrameBuilderCreateResult
FrameBuilder::create(const EthernetHeader &header,
                     std::span<std::byte> buffer) noexcept {
  if (buffer.size() < min_ethernet_frame_size) {
    return {FrameBuilderCreateResult::Status::BufferTooSmall};
  }
  if (header.ether_type != ecml::protocol::ethercat_ether_type) {
    return {FrameBuilderCreateResult::Status::InvalidEtherType};
  }
  const auto status = header.serialize(buffer);
  if (status != EthernetSerializeStatus::Success) {
    return {FrameBuilderCreateResult::Status::BufferTooSmall};
  }
  return {FrameBuilderCreateResult::Status::Success, FrameBuilder{buffer}};
}

FrameBuilderAddDatagramStatus
FrameBuilder::addDatagram(const DatagramHeader &header,
                          std::span<const std::byte> payload,
                          uint16_t wkc) noexcept {
  if (finalized_) {
    return FrameBuilderAddDatagramStatus::FrameAlreadyFinalized;
  }
  if (static_cast<uint8_t>(header.cmd) > max_command_type) {
    return FrameBuilderAddDatagramStatus::InvalidCommandCode;
  }
  if (payload.size() > Datagram::max_payload_size) {
    return FrameBuilderAddDatagramStatus::PayloadTooLarge;
  }
  size_t datagram_size = DatagramHeader::size + payload.size() + 2;
  if (write_cursor_ - FrameConstants::total_header_size + datagram_size >
      max_datagrams_total_bytes) {
    return FrameBuilderAddDatagramStatus::LengthLimitExceeded;
  }
  if (write_cursor_ + datagram_size >
      std::min(raw_frame_.size(), FrameConstants::max_frame_size)) {
    return FrameBuilderAddDatagramStatus::MtuExceeded;
  }
  auto local_header = header;
  local_header.len = static_cast<uint16_t>(payload.size());
  local_header.more_datagrams = false;
  const auto status = local_header.serialize(
      raw_frame_.subspan(write_cursor_, DatagramHeader::size));
  if (status != DatagramHeaderSerializeStatus::Success) {
    return FrameBuilderAddDatagramStatus::InvalidCommandCode;
  }
  std::copy(payload.begin(), payload.end(),
            raw_frame_.begin() + static_cast<std::ptrdiff_t>(write_cursor_) +
                DatagramHeader::size);
  size_t wkc_offset = write_cursor_ + DatagramHeader::size + payload.size();
  raw_frame_[wkc_offset] = static_cast<std::byte>(wkc & lower_byte_mask);
  raw_frame_[wkc_offset + 1] =
      static_cast<std::byte>((wkc >> byte_shift) & lower_byte_mask);
  if (datagram_count_ > 0) {
    raw_frame_[datagram_offset_ + more_datagrams_flag_byte_offset] |=
        std::byte{seventh_bit_mask};
  }
  datagram_offset_ = write_cursor_;
  write_cursor_ += datagram_size;
  ++datagram_count_;
  return FrameBuilderAddDatagramStatus::Success;
}

FrameBuilderFinalizeResult FrameBuilder::finalize() noexcept {
  if (finalized_) {
    return {FrameBuilderFinalizeResult::Status::FrameAlreadyFinalized};
  }
  if (datagram_count_ == 0) {
    return {FrameBuilderFinalizeResult::Status::EmptyFrame};
  }
  size_t ecat_payload_len = write_cursor_ - FrameConstants::total_header_size;
  EthercatHeader header{.length = static_cast<uint16_t>(ecat_payload_len),
                        .type = 0x01};
  const auto status =
      header.serialize(raw_frame_.subspan(EthernetHeader::size, 2));
  if (status != EthercatHeaderSerializeStatus::Success) {
    return {FrameBuilderFinalizeResult::Status::EmptyFrame};
  }
  if (write_cursor_ < min_ethernet_frame_size) {
    std::fill(raw_frame_.begin() + static_cast<std::ptrdiff_t>(write_cursor_),
              raw_frame_.begin() + min_ethernet_frame_size, std::byte{0x00});
    write_cursor_ = min_ethernet_frame_size;
  }
  finalized_ = true;
  return {FrameBuilderFinalizeResult::Status::Success,
          raw_frame_.first(write_cursor_)};
}

} // namespace ecml::protocol
