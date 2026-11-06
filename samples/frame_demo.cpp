#include "ecml/ecml_frame.hpp"

#include <iomanip>
#include <iostream>
#include <string_view>

namespace {

constexpr size_t two_bytes = 16;
constexpr uint8_t byte_shift = 8;
constexpr uint16_t al_status_reg = 0x0130;
constexpr uint16_t station_alias_reg = 0x0012;

void printHexDump(std::string_view title,
                  std::span<const std::byte> bytes) noexcept {
  std::cout << "=== " << title << " (" << bytes.size() << " bytes) ===\n";
  const auto old_flags = std::cout.flags();
  const auto old_fill = std::cout.fill('0');
  for (size_t i{}; i != bytes.size(); ++i) {
    if (i % two_bytes == 0) {
      std::cout << " [" << std::setw(4) << std::dec << i << "] ";
    }
    std::cout << std::hex << std::setw(2) << std::to_integer<unsigned>(bytes[i])
              << ' ';
    if ((i + 1) % two_bytes == 0 || i + 1 == bytes.size()) {
      std::cout << '\n';
    }
  }
  std::cout.fill(old_fill);
  std::cout.flags(old_flags);
  std::cout << '\n';
}

bool simulateSlave(std::span<std::byte> wire_frame) noexcept {
  auto parser_res = ecml::protocol::FrameParser::create(wire_frame);
  if (!parser_res.ok()) {
    return false;
  }
  auto &parser = *parser_res.parser;
  size_t current_offset = ecml::protocol::FrameConstants::total_header_size;
  for (;;) {
    auto dg_res = parser.nextDatagram();
    if (dg_res.done()) {
      break;
    }
    if (!dg_res.ok()) {
      return false;
    }
    auto &datagram = *dg_res.datagram;
    auto payload = datagram.data();
    const auto reg_offset =
        static_cast<uint16_t>(datagram.header().address >> two_bytes);

    if (reg_offset == 0x0000 && payload.size() == 4) {
      constexpr std::byte type{0x11};
      payload[0] = std::byte{type};
      payload[1] = std::byte{0x02};
      payload[2] = std::byte{0x01};
      payload[3] = std::byte{0x04};
    } else if (reg_offset == al_status_reg && payload.size() == 2) {
      payload[0] = std::byte{0x01};
      payload[1] = std::byte{0x00};
    } else if (reg_offset == station_alias_reg && payload.size() == 2) {
      constexpr std::byte node{0x05};
      payload[0] = std::byte{node};
      payload[1] = std::byte{0x00};
    }
    const size_t wkc_offset =
        current_offset + ecml::protocol::DatagramHeader::size + payload.size();
    auto wkc_span = wire_frame.subspan(wkc_offset, 2);
    wkc_span[0] = std::byte{0x01};
    wkc_span[1] = std::byte{0x00};

    current_offset += datagram.size();
  }
  return true;
}

uint16_t readUint16LE(std::span<const std::byte> data) noexcept {
  return static_cast<uint16_t>(
      std::to_integer<uint16_t>(data[0]) |
      (std::to_integer<uint16_t>(data[1]) << byte_shift));
}

} // namespace

int main() {
  std::array<std::byte, ecml::protocol::FrameConstants::max_frame_size>
      raw_buffer{};

  const ecml::protocol::EthernetHeader eth_hdr{
      .ether_type = ecml::protocol::ethercat_ether_type,
      .dst = ecml::protocol::MacAddress{{std::byte{0xFF}, std::byte{0xFF},
                                         std::byte{0xFF}, std::byte{0xFF},
                                         std::byte{0xFF}, std::byte{0xFF}}},
      .src = ecml::protocol::MacAddress{{std::byte{0x00}, std::byte{0x1A},
                                         std::byte{0x2B}, std::byte{0x3C},
                                         std::byte{0x4D}, std::byte{0x5E}}},
  };

  auto builder_res = ecml::protocol::FrameBuilder::create(eth_hdr, raw_buffer);
  if (!builder_res.ok()) {
    std::cerr << "Failed to create FrameBuilder\n";
    return 1;
  }
  auto &builder = *builder_res.builder;

  std::array<std::byte, 4> empty4{};
  std::array<std::byte, 2> empty2{};

  const ecml::protocol::DatagramHeader dg_esc_info{
      .cmd = ecml::protocol::CommandType::APRD,
      .idx = 0x01,
      .address = 0x0000'0000U};

  const ecml::protocol::DatagramHeader dg_al_status{
      .cmd = ecml::protocol::CommandType::APRD,
      .idx = 0x02,
      .address = 0x0130'0000U};

  const ecml::protocol::DatagramHeader dg_alias{
      .cmd = ecml::protocol::CommandType::APRD,
      .idx = 0x03,
      .address = 0x0012'0000U};

  if (builder.addDatagram(dg_esc_info, empty4, 0) !=
          ecml::protocol::FrameBuilderAddDatagramStatus::Success ||
      builder.addDatagram(dg_al_status, empty2, 0) !=
          ecml::protocol::FrameBuilderAddDatagramStatus::Success ||
      builder.addDatagram(dg_alias, empty2, 0) !=
          ecml::protocol::FrameBuilderAddDatagramStatus::Success) {
    std::cerr << "Failed to add datagrams to frame\n";
    return 1;
  }

  const auto fin_res = builder.finalize();
  if (!fin_res.ok() || !fin_res.frame.has_value()) {
    std::cerr << "Failed to finalize frame\n";
    return 1;
  }

  const auto tx_frame = *fin_res.frame;
  printHexDump("Master TX Frame (Before Slave)", tx_frame);

  auto mutable_wire_frame =
      std::span<std::byte>(raw_buffer).subspan(0, tx_frame.size());
  if (!simulateSlave(mutable_wire_frame)) {
    std::cerr << "Slave simulation failed\n";
    return 1;
  }

  printHexDump("Master RX Frame (After Slave)", mutable_wire_frame);

  auto rx_parser_res = ecml::protocol::FrameParser::create(mutable_wire_frame);
  if (!rx_parser_res.ok()) {
    std::cerr << "Failed to parse RX frame\n";
    return 1;
  }
  auto &rx_parser = *rx_parser_res.parser;

  std::cout << "=== Parsed Registers ===\n";
  while (true) {
    const auto dg_res = rx_parser.nextDatagram();
    if (!dg_res.ok()) {
      std::cerr << "Error while iterating datagrams\n";
      return 1;
    }
    if (dg_res.done()) {
      break;
    }

    const auto &datagram = *dg_res.datagram;
    const auto reg_offset =
        static_cast<uint16_t>(datagram.header().address >> 16U);

    std::cout << "Datagram [idx="
              << static_cast<unsigned>(datagram.header().idx) << ", Reg=0x"
              << std::hex << std::setw(4) << std::setfill('0') << reg_offset
              << std::dec << "]" << " | Len=" << datagram.header().len
              << " | WKC=" << datagram.wkc() << " | More="
              << (datagram.header().more_datagrams ? "true" : "false")
              << " -> ";

    if (reg_offset == 0x0000) {
      std::cout << "ESC Type: 0x" << std::hex
                << std::to_integer<unsigned>(datagram.data()[0]) << ", Rev: 0x"
                << std::to_integer<unsigned>(datagram.data()[1]) << std::dec
                << '\n';
    } else if (reg_offset == al_status_reg) {
      std::cout << "AL Status: 0x" << std::hex << readUint16LE(datagram.data())
                << " (INIT)\n"
                << std::dec;
    } else if (reg_offset == station_alias_reg) {
      std::cout << "Station Alias: " << readUint16LE(datagram.data()) << '\n';
    }
  }

  return 0;
}
