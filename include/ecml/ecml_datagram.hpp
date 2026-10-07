#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace ecml::protocol {

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

enum class DatagramHeaderSerializeStatus : uint8_t {
  Success,
  BufferTooSmall,
  LengthOutOfRange
};

struct DatagramHeaderDeserializeResult;

struct DatagramHeader {
  static constexpr std::size_t size{10};

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
  // return Success if all bytes were successfully written
  // if destination.size() < 10 returns BufferTooSmall
  // if len > 1486 returns LengthOutOfRange
  [[nodiscard]] DatagramHeaderSerializeStatus
  serialize(std::span<std::byte> destination) const noexcept;

  // constructs datagram header from first ten bytes of source
  [[nodiscard]] static DatagramHeaderDeserializeResult
  deserialize(std::span<const std::byte> source) noexcept;
};

struct DatagramHeaderDeserializeResult {
  enum class Status : uint8_t {
    Success,
    BufferTooSmall,
    UnknownCommand,
    LengthOutOfRange,
    ReservedBitsMismatch
  };
  Status status{Status::Success};
  std::optional<DatagramHeader> header{std::nullopt};
  [[nodiscard]] bool ok() const noexcept { return status == Status::Success; }
};

struct DatagramCreateResult;
struct DatagramDeserializeResult;

enum class DatagramSerializeStatus : uint8_t {
  Success,
  BufferTooSmall,
  HeaderSerializationFailed
};

class Datagram {
public:
  static constexpr size_t max_payload_size{1486};

  Datagram() = delete;

  // construct datagram with given header data and wkc
  // if data.size() > 1486 returned status is PayloadTooLarge
  // len field of given header is ignored
  // in returned object header_.len = static_cast<uint16_t>(data.size())
  [[nodiscard]] static DatagramCreateResult
  create(DatagramHeader header, std::span<std::byte> data, uint16_t wkc = 0);

  [[nodiscard]] const DatagramHeader &header() const noexcept;
  std::span<std::byte> data() noexcept;
  [[nodiscard]] std::span<const std::byte> data() const noexcept;
  [[nodiscard]] uint16_t wkc() const noexcept;
  // 10 + data.size() + 2;
  [[nodiscard]] size_t size() const noexcept;

  // wrties this->size() bytes in destination
  // uses serialize method of DatagramHeader
  // return true if all bytes were successfully written
  // if destination.size() < this->size() returns false
  [[nodiscard]] DatagramSerializeStatus
  serialize(std::span<std::byte> destination) const noexcept;

  // construct datagram from first 10 + header.len + 2 bytes
  // supposing first 10 bytes is datagram header
  // last 2 bytes is working counter
  // and header.len() bytes in between is data
  // if source.size() < 12 return BufferTooSmall
  // uses deserialize method if DatagramHeader
  // if DatagramHeader::deserialize returns is not Successfull
  // this method returns InvalidHeader
  // otherwise if source.size() < 10 + header.len + 2 returns BufferTooSmall
  [[nodiscard]] static DatagramDeserializeResult
  deserialize(std::span<std::byte> source) noexcept;

private:
  Datagram(DatagramHeader header, std::span<std::byte> data, uint16_t wkc = 0);
  DatagramHeader header_;
  std::span<std::byte> data_; // Datagram doesnt owns data
  uint16_t wkc_{};
};

struct DatagramCreateResult {
  enum class Status : uint8_t { Success, PayloadTooLarge, InvalidCommand };
  Status status{Status::Success};
  std::optional<Datagram> datagram{std::nullopt};
  [[nodiscard]] bool ok() const noexcept { return status == Status::Success; }
};

struct DatagramDeserializeResult {
  enum class Status : uint8_t {
    Success,
    BufferTooSmall,
    InvalidHeader,
    PayloadTruncated,
    PayloadTooLarge
  };
  Status status{Status::Success};
  std::optional<Datagram> datagram{std::nullopt};
  size_t bytes_consumed{}; // 10 + header.len + 2 if Success 0 otherwise
  [[nodiscard]] bool ok() const noexcept { return status == Status::Success; }
};

} // namespace ecml::protocol
