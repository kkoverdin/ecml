#include "ecml/ecml_rawsocket.hpp"

#include <algorithm>
#include <arpa/inet.h>
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
ssize_t RawSocket::send(std::span<const std::byte> frame) noexcept {
  return ::send(fd_, frame.data(), frame.size(), 0);
}

ssize_t RawSocket::receive(std::span<std::byte> destination) noexcept {
  return ::recv(fd_, destination.data(), destination.size(), MSG_DONTWAIT);
}

RawSocket::RawSocket(int fd) noexcept : fd_{fd} {}

std::optional<RawSocket> RawSocket::open(std::string_view ifname,
                                         uint16_t ether_type) noexcept {
  if (ifname.empty() || ifname.size() >= IFNAMSIZ) {
    return std::nullopt;
  }
  int fd = ::socket(PF_PACKET, SOCK_RAW, htons(ether_type));
  if (fd < 0) {
    return std::nullopt;
  }
  struct ifreq ifr {};
  std::copy(ifname.data(), ifname.data() + ifname.size(), ifr.ifr_name);
  if (::ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
    ::close(fd);
    return std::nullopt;
  }
  struct sockaddr_ll sll {};
  sll.sll_family = AF_PACKET;
  sll.sll_ifindex = ifr.ifr_ifindex;
  sll.sll_protocol = htons(ether_type);
  if (::bind(fd, reinterpret_cast<struct sockaddr *>(&sll), sizeof(sll)) < 0) {
    ::close(fd);
    return std::nullopt;
  }
  return RawSocket{fd};
}

} // namespace ecml::network
