#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <sys/types.h>

namespace ecml::network {
// RAII socket wrapper
// needed since EtherCat works on data link layer (OSI)
class RawSocket {
public:
  // to prevent logical error like calling send or receive before binding
  // default ctor was deleted in favor of static member function open
  RawSocket() = delete;
  ~RawSocket();
  RawSocket(const RawSocket &other) = delete;
  RawSocket &operator=(const RawSocket &other) = delete;

  RawSocket(RawSocket &&other) noexcept;
  RawSocket &operator=(RawSocket &&other) noexcept;

  // optional has value if socket created successfully
  // return type is std::optional for simplicity
  // later can be change in favor of error wrapper struct
  // 0x88A4 is a standard EtherType for EtherCat
  [[nodiscard]] static std::optional<RawSocket>
  open(std::string_view, uint16_t ether_type = 0x88A4) noexcept;
  // return false only if object was moved-from
  [[nodiscard]] bool isOpen() const noexcept;
  [[nodiscard]] ssize_t send(std::span<const std::byte> frame) noexcept;
  [[nodiscard]] ssize_t receive(std::span<std::byte> destination) noexcept;

private:
  RawSocket(int fd) noexcept;
  void close();
  int fd_{-1};
};

} // namespace ecml::network
