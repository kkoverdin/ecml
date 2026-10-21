#include "ecml/ecml_frame.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <gtest/gtest.h>
#include <span>
#include <vector>

namespace {

constexpr size_t min_ethernet_frame_size = 60;
constexpr size_t max_ethernet_frame_size = 1514;
constexpr size_t max_payload_size = 1486;

ecml::protocol::EthernetHeader makeValidEthernetHeader() noexcept {
  return ecml::protocol::EthernetHeader{
      .ether_type = ecml::protocol::ethercat_ether_type,
      .dst = ecml::protocol::MacAddress{{std::byte{0xFF}, std::byte{0xFF},
                                         std::byte{0xFF}, std::byte{0xFF},
                                         std::byte{0xFF}, std::byte{0xFF}}},
      .src = ecml::protocol::MacAddress{{std::byte{0x00}, std::byte{0x1A},
                                         std::byte{0x2B}, std::byte{0x3C},
                                         std::byte{0x4D}, std::byte{0x5E}}}};
}

} // namespace

TEST(FrameBuilderUnit, CreateSuccessfully) {
  std::array<std::byte, min_ethernet_frame_size> buffer{};
  auto res =
      ecml::protocol::FrameBuilder::create(makeValidEthernetHeader(), buffer);

  EXPECT_TRUE(res.ok());
  EXPECT_EQ(res.status,
            ecml::protocol::FrameBuilderCreateResult::Status::Success);
  EXPECT_TRUE(res.builder.has_value());
}

TEST(FrameBuilderUnit, CreateBufferTooSmall) {
  std::array<std::byte, min_ethernet_frame_size - 1> small_buffer{};
  auto res = ecml::protocol::FrameBuilder::create(makeValidEthernetHeader(),
                                                  small_buffer);

  EXPECT_FALSE(res.ok());
  EXPECT_EQ(res.status,
            ecml::protocol::FrameBuilderCreateResult::Status::BufferTooSmall);
  EXPECT_FALSE(res.builder.has_value());
}

TEST(FrameBuilderUnit, CreateInvalidEtherType) {
  std::array<std::byte, min_ethernet_frame_size> buffer{};
  auto eth = makeValidEthernetHeader();
  eth.ether_type = 0x0800;

  auto res = ecml::protocol::FrameBuilder::create(eth, buffer);

  EXPECT_FALSE(res.ok());
  EXPECT_EQ(res.status,
            ecml::protocol::FrameBuilderCreateResult::Status::InvalidEtherType);
  EXPECT_FALSE(res.builder.has_value());
}

TEST(FrameBuilderUnit, AddDatagramSuccessfully) {
  std::array<std::byte, 128> buffer{};
  auto builder_res =
      ecml::protocol::FrameBuilder::create(makeValidEthernetHeader(), buffer);
  ASSERT_TRUE(builder_res.ok());

  std::array<std::byte, 4> payload{std::byte{0x01}, std::byte{0x02},
                                   std::byte{0x03}, std::byte{0x04}};
  ecml::protocol::DatagramHeader hdr{.cmd = ecml::protocol::CommandType::APRD,
                                     .idx = 0x10};

  EXPECT_EQ(builder_res.builder->addDatagram(hdr, payload, 0),
            ecml::protocol::FrameBuilderAddDatagramStatus::Success);
}

TEST(FrameBuilderUnit, AddDatagramChainingSuccessfully) {
  std::array<std::byte, 256> buffer{};
  auto builder_res =
      ecml::protocol::FrameBuilder::create(makeValidEthernetHeader(), buffer);
  ASSERT_TRUE(builder_res.ok());
  auto &builder = *builder_res.builder;

  std::array<std::byte, 4> payload4{};
  std::array<std::byte, 2> payload2{};

  ecml::protocol::DatagramHeader hdr1{.cmd = ecml::protocol::CommandType::APRD,
                                      .idx = 1};
  ecml::protocol::DatagramHeader hdr2{.cmd = ecml::protocol::CommandType::APWR,
                                      .idx = 2};

  ASSERT_EQ(builder.addDatagram(hdr1, payload4, 0),
            ecml::protocol::FrameBuilderAddDatagramStatus::Success);
  ASSERT_EQ(builder.addDatagram(hdr2, payload2, 0),
            ecml::protocol::FrameBuilderAddDatagramStatus::Success);

  auto fin_res = builder.finalize();
  ASSERT_TRUE(fin_res.ok());
  const auto frame = *fin_res.frame;

  EXPECT_NE(frame[23] & std::byte{0x80}, std::byte{0x00});
  EXPECT_EQ(frame[39] & std::byte{0x80}, std::byte{0x00});
}

TEST(FrameBuilderUnit, AddDatagramFrameAlreadyFinalized) {
  std::array<std::byte, min_ethernet_frame_size> buffer{};
  auto builder_res =
      ecml::protocol::FrameBuilder::create(makeValidEthernetHeader(), buffer);
  ASSERT_TRUE(builder_res.ok());
  auto &builder = *builder_res.builder;

  std::array<std::byte, 2> payload{};
  ecml::protocol::DatagramHeader hdr{.cmd = ecml::protocol::CommandType::APRD};

  ASSERT_EQ(builder.addDatagram(hdr, payload, 0),
            ecml::protocol::FrameBuilderAddDatagramStatus::Success);
  ASSERT_TRUE(builder.finalize().ok());

  EXPECT_EQ(
      builder.addDatagram(hdr, payload, 0),
      ecml::protocol::FrameBuilderAddDatagramStatus::FrameAlreadyFinalized);
}

TEST(FrameBuilderUnit, AddDatagramInvalidCommandCode) {
  std::array<std::byte, min_ethernet_frame_size> buffer{};
  auto builder_res =
      ecml::protocol::FrameBuilder::create(makeValidEthernetHeader(), buffer);
  ASSERT_TRUE(builder_res.ok());

  std::array<std::byte, 2> payload{};
  ecml::protocol::DatagramHeader hdr{
      // NOLINTNEXTLINE
      .cmd = static_cast<ecml::protocol::CommandType>(0x0F)};

  EXPECT_EQ(builder_res.builder->addDatagram(hdr, payload, 0),
            ecml::protocol::FrameBuilderAddDatagramStatus::InvalidCommandCode);
}

TEST(FrameBuilderUnit, AddDatagramPayloadTooLarge) {
  std::array<std::byte, max_ethernet_frame_size> buffer{};
  auto builder_res =
      ecml::protocol::FrameBuilder::create(makeValidEthernetHeader(), buffer);
  ASSERT_TRUE(builder_res.ok());

  std::vector<std::byte> oversized_payload(max_payload_size + 1,
                                           std::byte{0x00});
  ecml::protocol::DatagramHeader hdr{.cmd = ecml::protocol::CommandType::LWR};

  EXPECT_EQ(builder_res.builder->addDatagram(hdr, oversized_payload, 0),
            ecml::protocol::FrameBuilderAddDatagramStatus::PayloadTooLarge);
}

TEST(FrameBuilderUnit, AddDatagramMtuExceeded) {
  std::array<std::byte, min_ethernet_frame_size> buffer{};
  auto builder_res =
      ecml::protocol::FrameBuilder::create(makeValidEthernetHeader(), buffer);
  ASSERT_TRUE(builder_res.ok());
  auto &builder = *builder_res.builder;

  std::array<std::byte, 28> payload1{};
  ecml::protocol::DatagramHeader hdr{.cmd = ecml::protocol::CommandType::LWR};
  ASSERT_EQ(builder.addDatagram(hdr, payload1, 0),
            ecml::protocol::FrameBuilderAddDatagramStatus::Success);

  std::span<const std::byte> payload2{};
  EXPECT_EQ(builder.addDatagram(hdr, payload2, 0),
            ecml::protocol::FrameBuilderAddDatagramStatus::MtuExceeded);
}

TEST(FrameBuilderUnit, AddDatagramLengthLimitExceeded) {
  std::array<std::byte, 2048> buffer{};
  auto builder_res =
      ecml::protocol::FrameBuilder::create(makeValidEthernetHeader(), buffer);
  ASSERT_TRUE(builder_res.ok());
  auto &builder = *builder_res.builder;

  std::vector<std::byte> max_payload(max_payload_size, std::byte{0x00});
  ecml::protocol::DatagramHeader hdr{.cmd = ecml::protocol::CommandType::LWR};
  ASSERT_EQ(builder.addDatagram(hdr, max_payload, 0),
            ecml::protocol::FrameBuilderAddDatagramStatus::Success);

  std::span<const std::byte> empty_payload{};
  EXPECT_EQ(builder.addDatagram(hdr, empty_payload, 0),
            ecml::protocol::FrameBuilderAddDatagramStatus::LengthLimitExceeded);
}

TEST(FrameBuilderUnit, FinalizeSuccessfully) {
  std::array<std::byte, 128> buffer{};
  std::fill(buffer.begin(), buffer.end(), std::byte{0xAA});

  auto builder_res =
      ecml::protocol::FrameBuilder::create(makeValidEthernetHeader(), buffer);
  ASSERT_TRUE(builder_res.ok());
  auto &builder = *builder_res.builder;

  std::array<std::byte, 2> payload{std::byte{0x11}, std::byte{0x22}};
  ecml::protocol::DatagramHeader hdr{.cmd = ecml::protocol::CommandType::APRD,
                                     .idx = 0x2A,
                                     .address = 0x01300000};
  ASSERT_EQ(builder.addDatagram(hdr, payload, 0x0001),
            ecml::protocol::FrameBuilderAddDatagramStatus::Success);

  auto fin_res = builder.finalize();
  ASSERT_TRUE(fin_res.ok());
  ASSERT_TRUE(fin_res.frame.has_value());

  const auto frame = *fin_res.frame;
  EXPECT_EQ(frame.size(), min_ethernet_frame_size);

  EXPECT_EQ(frame[14], std::byte{0x0E});
  EXPECT_EQ(frame[15], std::byte{0x10});

  const auto padding = frame.subspan(30);
  EXPECT_TRUE(std::all_of(padding.begin(), padding.end(),
                          [](std::byte b) { return b == std::byte{0x00}; }));
}

TEST(FrameBuilderUnit, FinalizeEmptyFrame) {
  std::array<std::byte, min_ethernet_frame_size> buffer{};
  auto builder_res =
      ecml::protocol::FrameBuilder::create(makeValidEthernetHeader(), buffer);
  ASSERT_TRUE(builder_res.ok());

  auto fin_res = builder_res.builder->finalize();
  EXPECT_FALSE(fin_res.ok());
  EXPECT_EQ(fin_res.status,
            ecml::protocol::FrameBuilderFinalizeResult::Status::EmptyFrame);
  EXPECT_FALSE(fin_res.frame.has_value());
}

TEST(FrameBuilderUnit, FinalizeFrameAlreadyFinalized) {
  std::array<std::byte, min_ethernet_frame_size> buffer{};
  auto builder_res =
      ecml::protocol::FrameBuilder::create(makeValidEthernetHeader(), buffer);
  ASSERT_TRUE(builder_res.ok());
  auto &builder = *builder_res.builder;

  std::array<std::byte, 2> payload{};
  ecml::protocol::DatagramHeader hdr{.cmd = ecml::protocol::CommandType::APRD};
  ASSERT_EQ(builder.addDatagram(hdr, payload, 0),
            ecml::protocol::FrameBuilderAddDatagramStatus::Success);
  ASSERT_TRUE(builder.finalize().ok());

  auto second_fin = builder.finalize();
  EXPECT_FALSE(second_fin.ok());
  EXPECT_EQ(second_fin.status, ecml::protocol::FrameBuilderFinalizeResult::
                                   Status::FrameAlreadyFinalized);
}

TEST(FrameParserUnit, CreateSuccessfully) {
  std::array<std::byte, min_ethernet_frame_size> raw{};
  auto builder_res =
      ecml::protocol::FrameBuilder::create(makeValidEthernetHeader(), raw);
  ASSERT_TRUE(builder_res.ok());

  std::array<std::byte, 2> payload{};
  ecml::protocol::DatagramHeader hdr{.cmd = ecml::protocol::CommandType::APRD};
  ASSERT_EQ(builder_res.builder->addDatagram(hdr, payload, 0),
            ecml::protocol::FrameBuilderAddDatagramStatus::Success);
  ASSERT_TRUE(builder_res.builder->finalize().ok());

  auto parser_res = ecml::protocol::FrameParser::create(raw);
  EXPECT_TRUE(parser_res.ok());
  EXPECT_EQ(parser_res.status,
            ecml::protocol::FrameParserCreateResult::Status::Success);
  EXPECT_TRUE(parser_res.parser.has_value());
}

TEST(FrameParserUnit, CreateBufferTooSmall) {
  std::array<std::byte, 15> tiny_buffer{};
  auto res = ecml::protocol::FrameParser::create(tiny_buffer);

  EXPECT_FALSE(res.ok());
  EXPECT_EQ(res.status,
            ecml::protocol::FrameParserCreateResult::Status::BufferTooSmall);
}

TEST(FrameParserUnit, CreateUnsupportedEtherType) {
  std::array<std::byte, min_ethernet_frame_size> raw{};
  auto builder_res =
      ecml::protocol::FrameBuilder::create(makeValidEthernetHeader(), raw);
  ASSERT_TRUE(builder_res.ok());

  std::array<std::byte, 2> payload{};
  ecml::protocol::DatagramHeader hdr{.cmd = ecml::protocol::CommandType::APRD};
  ASSERT_EQ(builder_res.builder->addDatagram(hdr, payload, 0),
            ecml::protocol::FrameBuilderAddDatagramStatus::Success);
  ASSERT_TRUE(builder_res.builder->finalize().ok());

  raw[12] = std::byte{0x08};
  raw[13] = std::byte{0x00};

  auto res = ecml::protocol::FrameParser::create(raw);
  EXPECT_FALSE(res.ok());
  EXPECT_EQ(
      res.status,
      ecml::protocol::FrameParserCreateResult::Status::UnsupportedEtherType);
}

TEST(FrameParserUnit, CreateInvalidProtocolType) {
  std::array<std::byte, min_ethernet_frame_size> raw{};
  auto builder_res =
      ecml::protocol::FrameBuilder::create(makeValidEthernetHeader(), raw);
  ASSERT_TRUE(builder_res.ok());

  std::array<std::byte, 2> payload{};
  ecml::protocol::DatagramHeader hdr{.cmd = ecml::protocol::CommandType::APRD};
  ASSERT_EQ(builder_res.builder->addDatagram(hdr, payload, 0),
            ecml::protocol::FrameBuilderAddDatagramStatus::Success);
  ASSERT_TRUE(builder_res.builder->finalize().ok());

  raw[15] = std::byte{0x20};

  auto res = ecml::protocol::FrameParser::create(raw);
  EXPECT_FALSE(res.ok());
  EXPECT_EQ(
      res.status,
      ecml::protocol::FrameParserCreateResult::Status::InvalidProtocolType);
}

TEST(FrameParserUnit, CreateReservedBitMismatch) {
  std::array<std::byte, min_ethernet_frame_size> raw{};
  auto builder_res =
      ecml::protocol::FrameBuilder::create(makeValidEthernetHeader(), raw);
  ASSERT_TRUE(builder_res.ok());

  std::array<std::byte, 2> payload{};
  ecml::protocol::DatagramHeader hdr{.cmd = ecml::protocol::CommandType::APRD};
  ASSERT_EQ(builder_res.builder->addDatagram(hdr, payload, 0),
            ecml::protocol::FrameBuilderAddDatagramStatus::Success);
  ASSERT_TRUE(builder_res.builder->finalize().ok());

  raw[15] |= std::byte{0x08};

  auto res = ecml::protocol::FrameParser::create(raw);
  EXPECT_FALSE(res.ok());
  EXPECT_EQ(
      res.status,
      ecml::protocol::FrameParserCreateResult::Status::ReservedBitMismatch);
}

TEST(FrameParserUnit, CreatePayloadTruncated) {
  std::array<std::byte, min_ethernet_frame_size> raw{};
  auto builder_res =
      ecml::protocol::FrameBuilder::create(makeValidEthernetHeader(), raw);
  ASSERT_TRUE(builder_res.ok());

  std::array<std::byte, 2> payload{};
  ecml::protocol::DatagramHeader hdr{.cmd = ecml::protocol::CommandType::APRD};
  ASSERT_EQ(builder_res.builder->addDatagram(hdr, payload, 0),
            ecml::protocol::FrameBuilderAddDatagramStatus::Success);
  ASSERT_TRUE(builder_res.builder->finalize().ok());

  raw[14] = std::byte{100};
  raw[15] = std::byte{0x10};

  auto res = ecml::protocol::FrameParser::create(raw);
  EXPECT_FALSE(res.ok());
  EXPECT_EQ(res.status,
            ecml::protocol::FrameParserCreateResult::Status::PayloadTruncated);
}

TEST(FrameParserUnit, NextDatagramSuccessfully) {
  std::array<std::byte, min_ethernet_frame_size> raw{};
  auto builder_res =
      ecml::protocol::FrameBuilder::create(makeValidEthernetHeader(), raw);
  ASSERT_TRUE(builder_res.ok());

  std::array<std::byte, 2> payload{std::byte{0xAB}, std::byte{0xCD}};
  ecml::protocol::DatagramHeader hdr{.cmd = ecml::protocol::CommandType::APRD,
                                     .idx = 0x2A,
                                     .address = 0x01300000};
  ASSERT_EQ(builder_res.builder->addDatagram(hdr, payload, 0x0001),
            ecml::protocol::FrameBuilderAddDatagramStatus::Success);
  ASSERT_TRUE(builder_res.builder->finalize().ok());

  auto parser_res = ecml::protocol::FrameParser::create(raw);
  ASSERT_TRUE(parser_res.ok());

  auto dg = parser_res.parser->nextDatagram();
  ASSERT_TRUE(dg.ok());
  EXPECT_EQ(dg.status,
            ecml::protocol::FrameParserNextDatagramResult::Status::Success);
  ASSERT_TRUE(dg.datagram.has_value());
  EXPECT_EQ(dg.datagram->header().idx, 0x2A);
  EXPECT_EQ(dg.datagram->wkc(), 1U);
  EXPECT_EQ(dg.datagram->data()[0], std::byte{0xAB});
}

TEST(FrameParserUnit, NextDatagramEndOfStreamSuccessfully) {
  std::array<std::byte, min_ethernet_frame_size> raw{};
  auto builder_res =
      ecml::protocol::FrameBuilder::create(makeValidEthernetHeader(), raw);
  ASSERT_TRUE(builder_res.ok());

  std::array<std::byte, 2> payload{};
  ecml::protocol::DatagramHeader hdr{.cmd = ecml::protocol::CommandType::APRD};
  ASSERT_EQ(builder_res.builder->addDatagram(hdr, payload, 0),
            ecml::protocol::FrameBuilderAddDatagramStatus::Success);
  ASSERT_TRUE(builder_res.builder->finalize().ok());

  auto parser_res = ecml::protocol::FrameParser::create(raw);
  ASSERT_TRUE(parser_res.ok());

  ASSERT_TRUE(parser_res.parser->nextDatagram().datagram.has_value());

  auto eof_res = parser_res.parser->nextDatagram();
  EXPECT_TRUE(eof_res.ok());
  EXPECT_TRUE(eof_res.done());
  EXPECT_EQ(eof_res.status,
            ecml::protocol::FrameParserNextDatagramResult::Status::EndOfStream);
  EXPECT_FALSE(eof_res.datagram.has_value());
}

TEST(FrameParserUnit, ResetSuccessfully) {
  std::array<std::byte, min_ethernet_frame_size> raw{};
  auto builder_res =
      ecml::protocol::FrameBuilder::create(makeValidEthernetHeader(), raw);
  ASSERT_TRUE(builder_res.ok());

  std::array<std::byte, 2> payload{std::byte{0x01}, std::byte{0x00}};
  ecml::protocol::DatagramHeader hdr{.cmd = ecml::protocol::CommandType::APRD,
                                     .idx = 0x55};
  ASSERT_EQ(builder_res.builder->addDatagram(hdr, payload, 0),
            ecml::protocol::FrameBuilderAddDatagramStatus::Success);
  ASSERT_TRUE(builder_res.builder->finalize().ok());

  auto parser_res = ecml::protocol::FrameParser::create(raw);
  ASSERT_TRUE(parser_res.ok());
  auto &parser = *parser_res.parser;

  ASSERT_TRUE(parser.nextDatagram().datagram.has_value());
  ASSERT_TRUE(parser.nextDatagram().done());

  parser.reset();
  auto dg_again = parser.nextDatagram();
  ASSERT_TRUE(dg_again.datagram.has_value());
  EXPECT_EQ(dg_again.datagram->header().idx, 0x55);
}

TEST(FrameParserUnit, NextDatagramUnexpectedEndOfPayload) {
  std::array<std::byte, min_ethernet_frame_size> raw{};
  auto builder_res =
      ecml::protocol::FrameBuilder::create(makeValidEthernetHeader(), raw);
  ASSERT_TRUE(builder_res.ok());

  std::array<std::byte, 4> payload{};
  ecml::protocol::DatagramHeader hdr{.cmd = ecml::protocol::CommandType::APRD};
  ASSERT_EQ(builder_res.builder->addDatagram(hdr, payload, 0),
            ecml::protocol::FrameBuilderAddDatagramStatus::Success);
  ASSERT_TRUE(builder_res.builder->finalize().ok());

  raw[22] = std::byte{20};

  auto parser_res = ecml::protocol::FrameParser::create(raw);
  ASSERT_TRUE(parser_res.ok());

  auto dg_res = parser_res.parser->nextDatagram();
  EXPECT_FALSE(dg_res.ok());
  EXPECT_EQ(dg_res.status, ecml::protocol::FrameParserNextDatagramResult::
                               Status::UnexpectedEndOfPayload);
}

TEST(FrameParserUnit, NextDatagramUnknownCommand) {
  std::array<std::byte, min_ethernet_frame_size> raw{};
  auto builder_res =
      ecml::protocol::FrameBuilder::create(makeValidEthernetHeader(), raw);
  ASSERT_TRUE(builder_res.ok());

  std::array<std::byte, 2> payload{};
  ecml::protocol::DatagramHeader hdr{.cmd = ecml::protocol::CommandType::APRD};
  ASSERT_EQ(builder_res.builder->addDatagram(hdr, payload, 0),
            ecml::protocol::FrameBuilderAddDatagramStatus::Success);
  ASSERT_TRUE(builder_res.builder->finalize().ok());

  raw[16] = std::byte{0x0F};

  auto parser_res = ecml::protocol::FrameParser::create(raw);
  ASSERT_TRUE(parser_res.ok());

  auto dg_res = parser_res.parser->nextDatagram();
  EXPECT_FALSE(dg_res.ok());
  EXPECT_EQ(
      dg_res.status,
      ecml::protocol::FrameParserNextDatagramResult::Status::UnknownCommand);
}

TEST(FrameParserUnit, NextDatagramInvalidDatagramLength) {
  std::array<std::byte, min_ethernet_frame_size> raw{};
  auto builder_res =
      ecml::protocol::FrameBuilder::create(makeValidEthernetHeader(), raw);
  ASSERT_TRUE(builder_res.ok());

  std::array<std::byte, 2> payload{};
  ecml::protocol::DatagramHeader hdr{.cmd = ecml::protocol::CommandType::APRD};
  ASSERT_EQ(builder_res.builder->addDatagram(hdr, payload, 0),
            ecml::protocol::FrameBuilderAddDatagramStatus::Success);
  ASSERT_TRUE(builder_res.builder->finalize().ok());

  raw[22] = std::byte{0xDC};
  raw[23] = std::byte{0x05};

  auto parser_res = ecml::protocol::FrameParser::create(raw);
  ASSERT_TRUE(parser_res.ok());

  auto dg_res = parser_res.parser->nextDatagram();
  EXPECT_FALSE(dg_res.ok());
  EXPECT_EQ(dg_res.status, ecml::protocol::FrameParserNextDatagramResult::
                               Status::InvalidDatagramLength);
}

TEST(FrameParserUnit, NextDatagramReservedBitsMismatch) {
  std::array<std::byte, min_ethernet_frame_size> raw{};
  auto builder_res =
      ecml::protocol::FrameBuilder::create(makeValidEthernetHeader(), raw);
  ASSERT_TRUE(builder_res.ok());

  std::array<std::byte, 2> payload{};
  ecml::protocol::DatagramHeader hdr{.cmd = ecml::protocol::CommandType::APRD};
  ASSERT_EQ(builder_res.builder->addDatagram(hdr, payload, 0),
            ecml::protocol::FrameBuilderAddDatagramStatus::Success);
  ASSERT_TRUE(builder_res.builder->finalize().ok());

  raw[23] |= std::byte{0x08};

  auto parser_res = ecml::protocol::FrameParser::create(raw);
  ASSERT_TRUE(parser_res.ok());

  auto dg_res = parser_res.parser->nextDatagram();
  EXPECT_FALSE(dg_res.ok());
  EXPECT_EQ(dg_res.status, ecml::protocol::FrameParserNextDatagramResult::
                               Status::ReservedBitsMismatch);
}

TEST(FrameParserUnit, NextDatagramPrematureChainTermination) {
  std::array<std::byte, min_ethernet_frame_size> raw{};
  auto builder_res =
      ecml::protocol::FrameBuilder::create(makeValidEthernetHeader(), raw);
  ASSERT_TRUE(builder_res.ok());

  std::array<std::byte, 2> payload{};
  ecml::protocol::DatagramHeader hdr{.cmd = ecml::protocol::CommandType::APRD};
  ASSERT_EQ(builder_res.builder->addDatagram(hdr, payload, 0),
            ecml::protocol::FrameBuilderAddDatagramStatus::Success);
  ASSERT_TRUE(builder_res.builder->finalize().ok());

  raw[23] |= std::byte{0x80};

  auto parser_res = ecml::protocol::FrameParser::create(raw);
  ASSERT_TRUE(parser_res.ok());

  auto dg_res = parser_res.parser->nextDatagram();
  EXPECT_FALSE(dg_res.ok());
  EXPECT_EQ(dg_res.status, ecml::protocol::FrameParserNextDatagramResult::
                               Status::PrematureChainTermination);
}

TEST(FrameParserUnit, NextDatagramDanglingBytesAfterChain) {
  std::array<std::byte, min_ethernet_frame_size> raw{};
  auto builder_res =
      ecml::protocol::FrameBuilder::create(makeValidEthernetHeader(), raw);
  ASSERT_TRUE(builder_res.ok());

  std::array<std::byte, 2> payload{};
  ecml::protocol::DatagramHeader hdr{.cmd = ecml::protocol::CommandType::APRD};
  ASSERT_EQ(builder_res.builder->addDatagram(hdr, payload, 0),
            ecml::protocol::FrameBuilderAddDatagramStatus::Success);
  ASSERT_EQ(builder_res.builder->addDatagram(hdr, payload, 0),
            ecml::protocol::FrameBuilderAddDatagramStatus::Success);
  ASSERT_TRUE(builder_res.builder->finalize().ok());

  raw[23] &= ~std::byte{0x80};

  auto parser_res = ecml::protocol::FrameParser::create(raw);
  ASSERT_TRUE(parser_res.ok());

  auto dg_res = parser_res.parser->nextDatagram();
  EXPECT_FALSE(dg_res.ok());
  EXPECT_EQ(dg_res.status, ecml::protocol::FrameParserNextDatagramResult::
                               Status::DanglingBytesAfterChain);
}
