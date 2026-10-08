#pragma once

#include <cstddef>
#include <span>

#include "ecml_datagram.hpp"
#include "ecml_ethernet.hpp"

namespace ecml::protocol {

struct FrameConstants {
  static constexpr size_t min_frame_size{60};
  static constexpr size_t max_frame_size{1514};
  static constexpr size_t ethernet_header_size{14};
  static constexpr size_t ethercat_header_size{2};
  static constexpr size_t total_header_size{ethernet_header_size +
                                            ethercat_header_size};
  static constexpr size_t max_datagram_payload{max_frame_size -
                                               total_header_size};
};

struct FrameBuilderCreateResult;

enum class FrameBuilderAddDatagramStatus : uint8_t {
  Success,
  FrameAlreadyFinalized, // calling addDatagram after finalize
  PayloadTooLarge,       // payload > 1486
  MtuExceeded,           // datagram cant fit inside reminding buffer
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

  FrameBuilder(FrameBuilder &&other) = delete;
  FrameBuilder &operator=(FrameBuilder &&other) = delete;

  // constructs FrameBuilder from given ethernet header and buffer
  // if buffer.size() < 16 returns BufferTooSmall
  // if EtherType is not 0x88A4 returns InvalidEtherType
  // otherwise returns Success
  [[nodiscard]] static FrameBuilderCreateResult
  create(const EthernetHeader &header, std::span<std::byte> buffer);

  [[nodiscard]] FrameBuilderAddDatagramStatus
  addDatagram(const DatagramHeader &header, std::span<std::byte> payload,
              uint16_t wkc) noexcept;

  [[nodiscard]] FrameBuilderFinalizeResult finalize() const noexcept;

private:
  FrameBuilder(std::span<std::byte> buffer) noexcept;
  std::span<std::byte> raw_frame_;
  size_t write_cursor_{FrameConstants::total_header_size};
  size_t datagram_offset_{}; // index of last added datagram
  size_t datagram_count_{};
  bool finalyzed{false};
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
  std::optional<std::span<const std::byte>> frame;
  [[nodiscard]] bool ok() const noexcept { return status == Status::Success; }
};

} // namespace ecml::protocol
