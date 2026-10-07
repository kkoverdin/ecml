#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace ecml::protocol {

struct MacDeserializeResult;

enum class MacSerializeResult : uint8_t { Success, BufferTooSmall };

struct MacAddress {
  static constexpr std::size_t size{6};
  std::array<std::byte, size> bytes{};
  auto operator<=>(const MacAddress &other) const = default;

  // writes 6 bytes of mac address in destination
  // return Success if all bytes were successfully written
  // if destination.size() < 6 return BufferTooSmall
  [[nodiscard]] MacSerializeResult
  serialize(std::span<std::byte> destination) const noexcept;

  // construct mac address from first 6 bytes of source
  [[nodiscard]] static MacDeserializeResult
  deserialize(std::span<const std::byte> source) noexcept;
};

struct MacDeserializeResult {
  enum class Status : uint8_t { Success, BufferTooSmall };
  Status status{Status::Success};
  std::optional<MacAddress> address{std::nullopt};
  [[nodiscard]] bool ok() const noexcept { return status == Status::Success; }
};

} // namespace ecml::protocol
