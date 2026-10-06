#include "ecml/ecml_rawsocket.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

namespace ecml::network {

void RawSocket::close() noexcept {
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
}

RawSocket::~RawSocket() noexcept { close(); }

RawSocket::RawSocket(RawSocket &&other) noexcept : fd_{other.fd_} {
  other.fd_ = -1;
}

RawSocket &RawSocket::operator=(RawSocket &&other) noexcept {
  if (this != &other) {
    close();
    fd_ = other.fd_;
    other.fd_ = -1;
  }
  return *this;
}

bool RawSocket::isOpen() const noexcept { return fd_ >= 0; }
SendResult RawSocket::send(std::span<const std::byte> frame) const noexcept {
  if (!isOpen()) {
    return {SendResult::Status::InvalidSocket};
  }
  if (frame.empty()) {
    return {SendResult::Status::EmptyBuffer};
  }
  ssize_t res = ::send(fd_, frame.data(), frame.size(), MSG_DONTWAIT);
  if (res > 0) {
    if (static_cast<size_t>(res) == frame.size()) {
      return {SendResult::Status::Success, static_cast<size_t>(res)};
    }
    return {SendResult::Status::BadSend, static_cast<size_t>(res)};
  }
  if (res == 0) {
    return {SendResult::Status::BadSend};
  }
  int saved_errno{errno};
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
  if ((saved_errno == EAGAIN) || (saved_errno == EWOULDBLOCK)) {
#else
  if (saved_errno == EAGAIN) {
#endif
    return {SendResult::Status::WouldBlock};
  }
  if (saved_errno == EINTR) {
    return {SendResult::Status::Interrupted};
  }
  return {SendResult::Status::SystemError, 0, saved_errno};
}

ReceiveResult
RawSocket::receive(std::span<std::byte> destination) const noexcept {
  if (!isOpen()) {
    return {ReceiveResult::Status::InvalidSocket};
  }
  if (destination.empty()) {
    return {ReceiveResult::Status::EmptyBuffer};
  }
  ssize_t ret = ::recv(fd_, destination.data(), destination.size(),
                       MSG_DONTWAIT | MSG_TRUNC);
  if (ret >= 0) {
    auto total_wire_bytes = static_cast<size_t>(ret);
    if (total_wire_bytes <= destination.size()) {
      return {ReceiveResult::Status::Success, total_wire_bytes,
              total_wire_bytes};
    }
    return {ReceiveResult::Status::Truncated, destination.size(),
            total_wire_bytes};
  }
  int saved_errno = errno;
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
  if ((saved_errno == EAGAIN) || (saved_errno == EWOULDBLOCK)) {
#else
  if (saved_errno == EAGAIN) {
#endif
    return {ReceiveResult::Status::WouldBlock};
  }
  if (saved_errno == EINTR) {
    return {ReceiveResult::Status::Interrupted};
  }
  return {ReceiveResult::Status::SystemError, 0, 0, saved_errno};
}

RawSocket::RawSocket(int file_descriptor) noexcept : fd_{file_descriptor} {}

OpenResult RawSocket::open(std::string_view ifname,
                           uint16_t ether_type) noexcept {
  if (ifname.empty() || ifname.size() >= IFNAMSIZ) {
    return {OpenResult::Status::InvalidInterfaceName, std::nullopt};
  }
  int file_descriptor = ::socket(
      PF_PACKET, SOCK_RAW | SOCK_NONBLOCK | SOCK_CLOEXEC, htons(ether_type));
  if (file_descriptor < 0) {
    int saved_errno = errno;
    return {OpenResult::Status::SocketCreationFailed, std::nullopt,
            saved_errno};
  }
  struct ifreq ifr {};
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
  std::copy(ifname.begin(), ifname.end(), ifr.ifr_name);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
  if (::ioctl(file_descriptor, SIOCGIFINDEX, &ifr) < 0) {
    int saved_errno = errno;
    ::close(file_descriptor);
    return {OpenResult::Status::InterfaceIndexNotFound, std::nullopt,
            saved_errno};
  }
  struct sockaddr_ll sll {};
  sll.sll_family = AF_PACKET;
  sll.sll_ifindex = ifr.ifr_ifindex;
  sll.sll_protocol = htons(ether_type);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  if (::bind(file_descriptor, reinterpret_cast<struct sockaddr *>(&sll),
             sizeof(sll)) < 0) {
    int saved_errno = errno;
    ::close(file_descriptor);
    return {OpenResult::Status::BindFailed, std::nullopt, saved_errno};
  }
  return {OpenResult::Status::Success, RawSocket{file_descriptor}};
}

} // namespace ecml::network
