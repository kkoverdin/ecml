#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <sys/types.h>

namespace ecml::network {

struct SendResult {
  enum class Status : uint8_t {
    Success,       // socket sended exactly frame.size() bytes
    WouldBlock,    // requested operation would block
    Interrupted,   // kernel interrupted send
    SystemError,   // system error
    InvalidSocket, // calling send on closed socket
    EmptyBuffer,   // frame.size() == 0
    BadSend        // send syscall return < frame.size()
  };
  Status status{Status::Success};
  size_t bytes_sent{0};
  int system_errno{0}; // only for Status::SystemError
  [[nodiscard]] bool ok() const noexcept { return status == Status::Success; }
  [[nodiscard]] bool wouldBlock() const noexcept {
    return status == Status::WouldBlock;
  }
  [[nodiscard]] bool interrupted() const noexcept {
    return status == Status::Interrupted;
  }
};

struct ReceiveResult {
  enum class Status : uint8_t {
    Success,       // frame received and fit in destination EmptyBuffer
    Truncated,     // received frame size > destination.size()
    WouldBlock,    // no incoming frames
    Interrupted,   // interrupted by kernel
    SystemError,   // system error
    InvalidSocket, // calling receive on closed socket
    EmptyBuffer    // destination.size() == 0
  };
  Status status{Status::Success};
  size_t bytes_stored{0};
  size_t wire_length{0};
  int system_errno{0};
  [[nodiscard]] bool ok() const noexcept { return status == Status::Success; }
  [[nodiscard]] bool truncated() const noexcept {
    return status == Status::Truncated;
  }
  [[nodiscard]] bool wouldBlock() const noexcept {
    return status == Status::WouldBlock;
  }
  [[nodiscard]] bool interrupted() const noexcept {
    return status == Status::Interrupted;
  }
};

struct OpenResult;

// RAII socket wrapper
// needed since EtherCat works on data link layer (OSI)
class RawSocket {
public:
  static constexpr uint16_t default_ether_type{0x88A4};
  // to prevent logical error like calling send or receive before binding
  // default ctor was deleted in favor of static member function open
  RawSocket() = delete;
  ~RawSocket() noexcept;
  RawSocket(const RawSocket &other) = delete;
  RawSocket &operator=(const RawSocket &other) = delete;

  RawSocket(RawSocket &&other) noexcept;
  RawSocket &operator=(RawSocket &&other) noexcept;

  // optional has value if socket created successfully
  // return type is std::optional for simplicity
  // later can be change in favor of error wrapper struct
  // 0x88A4 is a standard EtherType for EtherCat
  [[nodiscard]] static OpenResult
  open(std::string_view ifname,
       uint16_t ether_type = default_ether_type) noexcept;
  // return false only if object was moved-from
  [[nodiscard]] bool isOpen() const noexcept;
  [[nodiscard]] SendResult
  send(std::span<const std::byte> frame) const noexcept;
  [[nodiscard]] ReceiveResult
  receive(std::span<std::byte> destination) const noexcept;

private:
  RawSocket(int file_descriptor) noexcept;
  void close() noexcept;
  int fd_{-1};
};

struct OpenResult {
  enum class Status : uint8_t {
    Success,                // socket created binded
    InvalidInterfaceName,   // ifname is empty or ifname.size() >= IFNAMSIZ
    SocketCreationFailed,   // ::socket returned -1
    InterfaceIndexNotFound, // ::ioctl returned -1
    BindFailed              // ::bind returned -1
  };
  Status status{Status::Success};
  std::optional<RawSocket> socket;
  int system_errno{0};
  [[nodiscard]] bool ok() const noexcept { return status == Status::Success; }
};

} // namespace ecml::network
