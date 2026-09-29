#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace ecml::protocol {

struct MacAddress {
  std::array<std::byte, 6> bytes{};
  auto operator<=>(const MacAddress &other) const = default;

  // writes 6 bytes of mac address in destination
  // return true if all bytes were successfully written
  // if destination.size() < 6 return false
  bool serialize(std::span<std::byte> destination) const noexcept;

  // construct mac address from first 6 bytes of source
  // if source.size() < 6 returns std::nullopt_t
  [[nodiscard]] static std::optional<MacAddress>
  deserialize(std::span<const std::byte> source) noexcept;
};

struct EthernetHeader {
  static constexpr std::size_t kHeaderSize{14};

  MacAddress dst;
  MacAddress src;
  uint16_t ether_type{0x88A4};

  // writes 14 bytes in destination
  // first 6 bytes is destination MAC address then 6 bytes of source MAC address
  // then 2 bytes of ether_type
  // (ether_type is most likely to be 0x88A4 but for now it can chossed freely)
  // return true if all bytes were successgully written
  // if destination.size() < 14 returns false
  bool serialize(std::span<std::byte> destination) const noexcept;

  // construct ethernet header from first 14 bytes of source
  // if source.size() < 14 returns std::nullopt_t
  [[nodiscard]] static std::optional<EthernetHeader>
  deserialize(std::span<const std::byte> source) noexcept;
};

struct EthercatHeader {
  static constexpr std::size_t kHeaderSize{2};
  static constexpr uint8_t kEthercatType{0x01};

  uint16_t length{0}; // 0...2047
  uint8_t type{kEthercatType};

  // writes two bytes in destination
  // first 11 bits is length then 1 reserved bit then 4 bits for type
  // return true if all bytes were successfully written
  // if destination.size() < 2 returns false
  bool serialize(std::span<std::byte> destination) const noexcept;

  // constructs ethercat header from first two bytes of source
  // if source.size() < 2 returns std::nullopt_t
  [[nodiscard]] static std::optional<EthercatHeader>
  deserialize(std::span<const std::byte> source) noexcept;
};

enum class CommandType : uint8_t {
  NOP = 0x00,  // No Operation
  APRD = 0x01, // Auto Increment Read
  APWR = 0x02, // Auto Increment Write
  APRW = 0x03, // Auto Increment Read Write
  FPRD = 0x04, // Configured Address Read
  FPWR = 0x05, // Configured Address Write
  FPRW = 0x06, // Configured Address Read Write
  BRD = 0x07,  // Broadcast Read
  BWR = 0x08,  // Broadcase Write
  BRW = 0x09,  // Broadcast Read Write
  LRD = 0x0A,  // Logical Memory Read
  LWR = 0x0B,  // Logical Memory Write
  LRW = 0x0C,  // Logical Memory Read Write
  ARMW = 0x0D, // Auto Increment Read Multiple Write
  FRMW = 0x0E  // Configured Read Multiple Write
};

struct DatagramHeader {
  static constexpr std::size_t kHeaderSize{10};

  CommandType cmd{CommandType::NOP};
  uint8_t idx{0};
  uint32_t address{0};
  uint16_t len{0}; // 0...2047
  bool more_datagrams{false};
  bool circulating{false};
  uint16_t irq{0};

  // writes ten bytes in destination
  // first 8 bits cmd then 8 bits of idx
  // then 32 bit address depending on addressing model
  // either 16 bit position/addres + 16 bit offset
  // or 32 bit logical address
  // return true if all bytes were successfully written
  // if destination.size() < 10 returns false
  bool serialize(std::span<std::byte> destination) const noexcept;

  // constructs datagram header from first ten bytes of source
  // if source.size() < 10 returns std::nullopt_t
  [[nodiscard]] static std::optional<DatagramHeader>
  deserialize(std::span<const std::byte> source) noexcept;
};

class Datagram {
public:
  static constexpr size_t kMaxPayloadSize{1486};

  Datagram() = delete;

  // construct datagram with given header data and wkc
  // if data.size() > 1486 returns std::nullopt_t
  // len field of given header is ignored
  // in returned object header_.len = static_cast<uint16_t>(data.size())
  [[nodiscard]] static std::optional<Datagram>
  create(DatagramHeader header, std::span<std::byte> data, uint16_t wkc = 0);

  const DatagramHeader &header() const noexcept;
  std::span<std::byte> data() noexcept;
  std::span<const std::byte> data() const noexcept;
  uint16_t wkc() const noexcept;
  // 10 + data.size() + 2;
  size_t size() const noexcept;

  // wrties this->size() bytes in destination
  // uses serialize method of DatagramHeader
  // return true if all bytes were successfully written
  // if destination.size() < this->size() returns false
  bool serialize(std::span<std::byte> destination) const noexcept;

  // construct datagram from first 10 + header.len + 2 bytes
  // supposing first 10 bytes is datagram header
  // last 2 bytes is working counter
  // and header.len() bytes in between is data
  // if source.size() < 12 return std::nullopt_t
  // uses deserialize method if DatagramHeader
  // if DatagramHeader::deserialize returns std::nullopt_t
  // this method returns std::nullopt_t
  // otherwise if source.size() < 10 + header.len + 2 returns std::nullopt_t
  [[nodiscard]] static std::optional<Datagram>
  deserialize(std::span<std::byte> source) noexcept;

private:
  Datagram(DatagramHeader header, std::span<std::byte> data, uint16_t wkc = 0);
  DatagramHeader header_;
  std::span<std::byte> data_; // Datagram doesnt owns data
  uint16_t wkc_{};
};

} // namespace ecml::protocol
