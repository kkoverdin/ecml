#pragma once

#include <cstddef>
#include <span>

#include "ecml_datagram.hpp"
#include "ecml_ethercat.hpp"
#include "ecml_ethernet.hpp"

namespace ecml::protocol {

namespace FrameConstants {

static constexpr size_t min_frame_size{60};
static constexpr size_t max_frame_size{1514};
static constexpr size_t ethernet_header_size{14};
static constexpr size_t ethercat_header_size{2};
static constexpr size_t total_header_size{ethernet_header_size +
                                          ethercat_header_size};
static constexpr size_t max_datagram_payload{max_frame_size -
                                             total_header_size};
}; // namespace FrameConstants

struct FrameBuilderCreateResult;

enum class FrameBuilderAddDatagramStatus : uint8_t {
  Success,
  FrameAlreadyFinalized, // calling addDatagram after finalize
  PayloadTooLarge,       // payload > 1486
  MtuExceeded,           // datagram cant fit inside remaining buffer
  LengthLimitExceeded,   // len > 2047
  InvalidCommandCode
};

struct FrameBuilderFinalizeResult;

class FrameBuilder {
public:
  FrameBuilder() = delete;
  ~FrameBuilder() = default;

  FrameBuilder(const FrameBuilder &other) = delete;
  FrameBuilder &operator=(const FrameBuilder &other) = delete;

  FrameBuilder(FrameBuilder &&other) noexcept = default;
  FrameBuilder &operator=(FrameBuilder &&other) noexcept = default;

  // constructs FrameBuilder from given ethernet header and buffer
  // if buffer.size() < 60 returns BufferTooSmall
  // if EtherType is not 0x88A4 returns InvalidEtherType
  // otherwise returns Success
  [[nodiscard]] static FrameBuilderCreateResult
  create(const EthernetHeader &header, std::span<std::byte> buffer) noexcept;

  [[nodiscard]] FrameBuilderAddDatagramStatus
  addDatagram(const DatagramHeader &header, std::span<const std::byte> payload,
              uint16_t wkc = 0) noexcept;

  [[nodiscard]] FrameBuilderFinalizeResult finalize() noexcept;

private:
  explicit FrameBuilder(std::span<std::byte> buffer) noexcept;
  std::span<std::byte> raw_frame_;
  size_t write_cursor_{FrameConstants::total_header_size};
  size_t datagram_offset_{}; // index of last added datagram
  size_t datagram_count_{};
  bool finalized_{false};
};

struct FrameBuilderCreateResult {
  enum class Status : uint8_t { Success, BufferTooSmall, InvalidEtherType };
  Status status{Status::Success};
  std::optional<FrameBuilder> builder{std::nullopt};
  [[nodiscard]] bool ok() const noexcept { return status == Status::Success; }
};

struct FrameBuilderFinalizeResult {
  enum class Status : uint8_t { Success, EmptyFrame, FrameAlreadyFinalized };
  Status status{Status::Success};
  std::optional<std::span<const std::byte>> frame{std::nullopt};
  [[nodiscard]] bool ok() const noexcept { return status == Status::Success; }
};

struct FrameParserCreateResult;
struct FrameParserNextDatagramResult;

class FrameParser {
public:
  FrameParser() = delete;
  ~FrameParser() = default;

  FrameParser(const FrameParser &other) = delete;
  FrameParser &operator=(const FrameParser &other) = delete;

  FrameParser(FrameParser &&other) noexcept = default;
  FrameParser &operator=(FrameParser &&other) noexcept = default;

  [[nodiscard]] static FrameParserCreateResult
  create(std::span<std::byte> bytes);

  [[nodiscard]] FrameParserNextDatagramResult nextDatagram() noexcept;

  void reset() noexcept {
    read_cursor_ = FrameConstants::total_header_size;
    more_datagrams_ = true;
  }

private:
  FrameParser(std::span<std::byte> bytes, EthernetHeader eth,
              EthercatHeader etc) noexcept;
  std::span<std::byte> bytes_;
  EthernetHeader eth_header_;
  EthercatHeader ec_header_;
  size_t read_cursor_{FrameConstants::total_header_size};
  bool more_datagrams_{true};
};

struct FrameParserCreateResult {
  enum class Status : uint8_t {
    Success,
    BufferTooSmall,
    UnsupportedEtherType,
    InvalidProtocolType,
    ReservedBitMismatch,
    PayloadTruncated // ec_header_.len > bytes.size()
  };
  Status status{Status::Success};
  std::optional<FrameParser> parser;
  [[nodiscard]] bool ok() const noexcept { return status == Status::Success; }
};

struct FrameParserNextDatagramResult {
  enum class Status : uint8_t {
    Success,
    EndOfStream,
    UnexpectedEndOfPayload,
    UnknownCommand,
    InvalidDatagramLength,
    ReservedBitsMismatch,
    PrematureChainTermination,
    DanglingBytesAfterChain
  };
  Status status{Status::Success};
  std::optional<Datagram> datagram;
  [[nodiscard]] bool ok() const noexcept {
    return status == Status::Success || status == Status::EndOfStream;
  }
  [[nodiscard]] bool last() const noexcept {
    return status == Status::EndOfStream;
  }
};

} // namespace ecml::protocol
