#pragma once

#include <cstdint>
#include <optional>
#include <span>

namespace ecml::protocol {

enum class EthercatHeaderSerializeStatus : uint8_t {
  Success,
  BufferTooSmall,
  LengthOutOfRange,
  InvalidProtocolType
};

struct EthercatHeaderDeserializeResult;

struct EthercatHeader {
  static constexpr std::size_t size{2};
  static constexpr uint8_t ethercat_type{0x01};

  uint16_t length{0}; // 0...2047
  uint8_t type{ethercat_type};

  // writes two bytes in destination
  // first 11 bits is length then 1 reserved bit then 4 bits for type
  // return Success if all bytes were successfully written
  // if destination.size() < 2 returns BufferTooSmall
  // if length > 2047 return LengthOutOfRange
  [[nodiscard]] EthercatHeaderSerializeStatus
  serialize(std::span<std::byte> destination) const noexcept;

  // constructs ethercat header from first two bytes of source
  // if source.size() < 2 returns std::nullopt_t
  [[nodiscard]] static EthercatHeaderDeserializeResult
  deserialize(std::span<const std::byte> source) noexcept;
};

struct EthercatHeaderDeserializeResult {
  enum class Status : uint8_t {
    Success,
    BufferTooSmall,
    UnsupportedType,
    ReservedBitMismatch
  };
  Status status{Status::Success};
  std::optional<EthercatHeader> header{std::nullopt};
  [[nodiscard]] bool ok() const noexcept { return status == Status::Success; }
};

} // namespace ecml::protocol
