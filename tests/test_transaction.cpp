#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <deque>
#include <functional>
#include <span>

#include <gtest/gtest.h>

#include "ecml/ecml_rawsocket.hpp"
#include "ecml/ecml_transaction.hpp"

template <typename... Args>
constexpr std::array<std::byte, sizeof...(Args)>
makeArrayOfBytes(Args... args) {
  return std::array<std::byte, sizeof...(Args)>{std::byte(args)...};
}

struct MockTransport {
  std::array<std::byte, 1514> wire_buffer{};
  size_t wire_size{};
  std::deque<ecml::network::SendResult> send_queue;
  std::deque<ecml::network::ReceiveResult> recv_queue;
  std::function<bool(std::span<std::byte>)> handler;

  ecml::network::SendResult send(std::span<const std::byte> tx_buffer) {
    if (!send_queue.empty()) {
      auto res = send_queue.front();
      send_queue.pop_front();
      if (res.ok()) {
        const size_t n = std::min(tx_buffer.size(), wire_buffer.size());
        std::memcpy(wire_buffer.data(), tx_buffer.data(), n);
        wire_size = n;
        if (res.bytes_sent == 0) {
          res.bytes_sent = n;
        }
      }
      return res;
    }

    const size_t n = std::min(tx_buffer.size(), wire_buffer.size());
    std::memcpy(wire_buffer.data(), tx_buffer.data(), n);
    wire_size = n;
    return {ecml::network::SendResult::Status::Success, n};
  }

  ecml::network::ReceiveResult receive(std::span<std::byte> rx_buffer) {
    if (!recv_queue.empty()) {
      auto res = recv_queue.front();
      recv_queue.pop_front();
      if (!res.ok()) {
        return res;
      }
    }

    if (wire_size == 0) {
      return {ecml::network::ReceiveResult::Status::WouldBlock};
    }

    if (wire_size > rx_buffer.size()) {
      return {ecml::network::ReceiveResult::Status::Truncated, 0, wire_size};
    }

    auto frame = std::span<std::byte>{wire_buffer.data(), wire_size};
    if (handler && !handler(frame)) {
      return {ecml::network::ReceiveResult::Status::WouldBlock};
    }

    std::memcpy(rx_buffer.data(), frame.data(), frame.size());
    return {ecml::network::ReceiveResult::Status::Success, frame.size(),
            frame.size()};
  }
};

using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

class TransactionManagerTest : public ::testing::Test {
protected:
  ecml::protocol::MacAddress dst_{
      makeArrayOfBytes(0x01, 0x02, 0x03, 0x04, 0x05, 0x06)};
  ecml::protocol::MacAddress src_{
      makeArrayOfBytes(0x11, 0x12, 0x13, 0x14, 0x15, 0x16)};
  MockTransport transport_{};
  ecml::master::TransactionManager<MockTransport> mgr_{transport_, dst_, src_};

  static std::vector<std::byte> makeBytes(std::initializer_list<uint8_t> vals) {
    std::vector<std::byte> out;
    out.reserve(vals.size());
    for (uint8_t v : vals) {
      out.push_back(static_cast<std::byte>(v));
    }
    return out;
  }

  static void setFrameWkc(std::span<std::byte> frame, uint16_t wkc) {
    auto parsed = ecml::protocol::FrameParser::create(frame);
    ASSERT_TRUE(parsed.ok());
    while (true) {
      auto next = parsed.parser->nextDatagram();
      if (next.done() || !next.ok()) {
        break;
      }
      (void)wkc;
    }
  }
};

TEST_F(TransactionManagerTest, SuccessSingleAndBatchRequests) {
  auto rx_buf = makeBytes({0x00, 0x00});
  std::array<ecml::master::DatagramRequest, 1> single_req{{
      ecml::master::DatagramRequest{
          .command = ecml::protocol::CommandType::LRD,
          .buffer = rx_buf,
          .address = 0x1000,
          .expected_wkc = 0,
      },
  }};

  transport_.handler = [](std::span<std::byte> frame) {
    (void)frame;
    return true;
  };

  const auto deadline = Clock::now() + 100ms;
  const auto send_res = mgr_.send(single_req, deadline);
  ASSERT_EQ(send_res.status, ecml::master::SendResult::Status::Success);

  const auto poll_res = mgr_.poll();
  EXPECT_EQ(poll_res.status, ecml::master::PollResult::Status::Success);
  EXPECT_GE(poll_res.roundtrip_time, 0ns);

  auto batch_rx_0 = makeBytes({0xAA});
  auto batch_rx_1 = makeBytes({0xBB, 0xCC});
  std::array<ecml::master::DatagramRequest, 2> batch_req{{
      {.command = ecml::protocol::CommandType::LRD,
       .buffer = batch_rx_0,
       .address = 0x2000,
       .expected_wkc = 0},
      {.command = ecml::protocol::CommandType::LRD,
       .buffer = batch_rx_1,
       .address = 0x2010,
       .expected_wkc = 0},
  }};

  const auto batch_send = mgr_.send(batch_req, Clock::now() + 100ms);
  ASSERT_EQ(batch_send.status, ecml::master::SendResult::Status::Success);
  EXPECT_EQ(mgr_.poll().status, ecml::master::PollResult::Status::Success);
  EXPECT_EQ(batch_rx_0[0], std::byte{0xAA});
  EXPECT_EQ(batch_rx_1[0], std::byte{0xBB});
  EXPECT_EQ(batch_rx_1[1], std::byte{0xCC});
}

TEST_F(TransactionManagerTest, WouldBlockOnSendAndReceive) {
  auto rx_buf = makeBytes({0x42});
  std::array<ecml::master::DatagramRequest, 1> req{{
      {.command = ecml::protocol::CommandType::LRD,
       .buffer = rx_buf,
       .address = 0x1000,
       .expected_wkc = 0},
  }};

  transport_.send_queue.push_back(
      {ecml::network::SendResult::Status::WouldBlock});
  transport_.send_queue.push_back(
      {ecml::network::SendResult::Status::WouldBlock});
  transport_.send_queue.push_back(
      {ecml::network::SendResult::Status::WouldBlock});

  transport_.recv_queue.push_back(
      {ecml::network::ReceiveResult::Status::WouldBlock});
  transport_.recv_queue.push_back(
      {ecml::network::ReceiveResult::Status::WouldBlock});

  const auto send_res = mgr_.send(req, Clock::now() + 200ms);
  ASSERT_EQ(send_res.status, ecml::master::SendResult::Status::Success);

  EXPECT_EQ(mgr_.poll().status, ecml::master::PollResult::Status::Pending);
  EXPECT_EQ(mgr_.poll().status, ecml::master::PollResult::Status::Pending);
  EXPECT_EQ(mgr_.poll().status, ecml::master::PollResult::Status::Pending);

  EXPECT_EQ(mgr_.poll().status, ecml::master::PollResult::Status::Pending);
  EXPECT_EQ(mgr_.poll().status, ecml::master::PollResult::Status::Pending);

  EXPECT_EQ(mgr_.poll().status, ecml::master::PollResult::Status::Success);
  EXPECT_EQ(rx_buf[0], std::byte{0x42});
}

TEST_F(TransactionManagerTest, InterruptedOnSendAndReceiveStaysPending) {
  auto rx_buf = makeBytes({0x77});
  std::array<ecml::master::DatagramRequest, 1> req{{
      {.command = ecml::protocol::CommandType::LRD,
       .buffer = rx_buf,
       .address = 0x1000,
       .expected_wkc = 0},
  }};

  transport_.send_queue.push_back(
      {ecml::network::SendResult::Status::Interrupted});
  transport_.send_queue.push_back(
      {ecml::network::SendResult::Status::Interrupted});
  transport_.send_queue.push_back(
      {ecml::network::SendResult::Status::Success, 0});

  transport_.recv_queue.push_back(
      {ecml::network::ReceiveResult::Status::Interrupted});

  const auto send_res = mgr_.send(req, Clock::now() + 200ms);
  ASSERT_EQ(send_res.status, ecml::master::SendResult::Status::Success);

  EXPECT_EQ(mgr_.poll().status, ecml::master::PollResult::Status::Pending);
  EXPECT_EQ(mgr_.poll().status, ecml::master::PollResult::Status::Pending);
  EXPECT_EQ(mgr_.poll().status, ecml::master::PollResult::Status::Pending);
  EXPECT_EQ(mgr_.poll().status, ecml::master::PollResult::Status::Success);
  EXPECT_EQ(rx_buf[0], std::byte{0x77});
}
