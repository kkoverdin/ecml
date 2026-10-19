#include "ecml/ecml_frame.hpp"

namespace {

constexpr size_t min_ethernet_frame_size = 60;
constexpr uint8_t max_command_type = 0x0E;
constexpr size_t max_datagrams_total_bytes = 1498;
constexpr uint8_t lower_byte_mask = 0xFF;
constexpr uint8_t byte_shift = 8;
constexpr uint8_t seventh_bit_mask = 0x80;
constexpr uint8_t more_datagrams_flag_byte_offset = 7;
constexpr size_t min_datagram_size = ecml::protocol::DatagramHeader::size + 2;

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

FrameParser::FrameParser(std::span<std::byte> bytes, EthernetHeader eth,
                         EthercatHeader etc) noexcept
    : bytes_{bytes}, eth_header_{eth}, ec_header_{etc},
      more_datagrams_{etc.length > 0} {}

FrameParserCreateResult
FrameParser::create(std::span<std::byte> bytes) noexcept {
  if (bytes.size() < FrameConstants::total_header_size) {
    return {FrameParserCreateResult::Status::BufferTooSmall};
  }
  const auto eth_res =
      EthernetHeader::deserialize(bytes.first(EthernetHeader::size));
  if (!eth_res.header.has_value()) {
    return {FrameParserCreateResult::Status::BufferTooSmall};
  }
  if (eth_res.header->ether_type != ethercat_ether_type) {
    return {FrameParserCreateResult::Status::UnsupportedEtherType};
  }
  const auto ec_res = EthercatHeader::deserialize(
      bytes.subspan(EthernetHeader::size, EthercatHeader::size));
  if (!ec_res.header.has_value()) {
    if (ec_res.status ==
        EthercatHeaderDeserializeResult::Status::ReservedBitMismatch) {
      return {FrameParserCreateResult::Status::ReservedBitMismatch};
    }
    return {FrameParserCreateResult::Status::InvalidProtocolType};
  }
  const size_t total_valid_size =
      FrameConstants::total_header_size + ec_res.header->length;
  if (bytes.size() < total_valid_size) {
    return {FrameParserCreateResult::Status::PayloadTruncated};
  }
  return {FrameParserCreateResult::Status::Success,
          FrameParser{bytes.first(total_valid_size), *eth_res.header,
                      *ec_res.header}};
}

FrameParserNextDatagramResult FrameParser::nextDatagram() noexcept {
  if (!more_datagrams_) {
    return {FrameParserNextDatagramResult::Status::EndOfStream};
  }
  const auto remaining = bytes_.subspan(read_cursor_);
  if (remaining.empty()) {
    more_datagrams_ = false;
    return {FrameParserNextDatagramResult::Status::PrematureChainTermination};
  }
  if (remaining.size() < min_datagram_size) {
    more_datagrams_ = false;
    return {FrameParserNextDatagramResult::Status::UnexpectedEndOfPayload};
  }
  const auto hdr_res = DatagramHeader::deserialize(remaining);
  if (!hdr_res.header.has_value()) {
    more_datagrams_ = false;
    switch (hdr_res.status) {
    case DatagramHeaderDeserializeResult::Status::UnknownCommand:
      return {FrameParserNextDatagramResult::Status::UnknownCommand};
    case DatagramHeaderDeserializeResult::Status::ReservedBitsMismatch:
      return {FrameParserNextDatagramResult::Status::ReservedBitsMismatch};
    case DatagramHeaderDeserializeResult::Status::LengthOutOfRange:
      return {FrameParserNextDatagramResult::Status::InvalidDatagramLength};
    default:
      return {FrameParserNextDatagramResult::Status::UnexpectedEndOfPayload};
    }
  }
  const auto dg_res = Datagram::deserialize(remaining);
  if (!dg_res.datagram.has_value()) {
    more_datagrams_ = false;
    return {FrameParserNextDatagramResult::Status::UnexpectedEndOfPayload};
  }
  const size_t next_cursor = read_cursor_ + dg_res.bytes_consumed;
  const size_t bytes_left = bytes_.size() - next_cursor;
  const bool has_more = dg_res.datagram->header().more_datagrams;
  if (has_more && bytes_left == 0) {
    more_datagrams_ = false;
    return {FrameParserNextDatagramResult::Status::PrematureChainTermination};
  }
  if (!has_more && bytes_left > 0) {
    more_datagrams_ = false;
    return {FrameParserNextDatagramResult::Status::DanglingBytesAfterChain};
  }
  read_cursor_ = next_cursor;
  more_datagrams_ = has_more;
  return {FrameParserNextDatagramResult::Status::Success, *dg_res.datagram};
}

} // namespace ecml::protocol
