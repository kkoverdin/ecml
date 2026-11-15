#include <algorithm>

#include "ecml/ecml_mac.hpp"

namespace ecml::protocol {

MacSerializeStatus
MacAddress::serialize(std::span<std::byte> destination) const noexcept {
  if (destination.size() < MacAddress::size) {
    return MacSerializeStatus::BufferTooSmall;
  }
  std::copy(bytes.begin(), bytes.end(), destination.begin());
  return MacSerializeStatus::Success;
}

MacDeserializeResult
MacAddress::deserialize(std::span<const std::byte> source) noexcept {
  if (source.size() < MacAddress::size) {
    return {MacDeserializeResult::Status::BufferTooSmall, std::nullopt};
  }
  MacAddress res;
  std::copy_n(source.begin(), MacAddress::size, res.bytes.begin());
  return {MacDeserializeResult::Status::Success, res};
}

} // namespace ecml::protocol
