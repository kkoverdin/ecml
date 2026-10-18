#include <gtest/gtest.h>

#include "ecml/ecml_datagram.hpp"

namespace {

constexpr size_t min_datagram_size =
    ecml::protocol::DatagramHeader::size + sizeof(uint16_t);

constexpr uint8_t sample_idx = 0x2A;
constexpr uint32_t sample_address = 0x01300000;
constexpr uint16_t sample_irq = 0x0000;
constexpr uint16_t sample_wkc = 0x1234;

} // namespace

TEST(DatagramHeaderUnit, SerializeSuccesfully) {
  const ecml::protocol::DatagramHeader header{
      .cmd = ecml::protocol::CommandType::APRD,
      .idx = 0x2A,
      .address = 0x01300000,
      .len = 0x0002,
      .more_datagrams = false,
      .circulating = false,
      .irq = 0x0000};
  const std::array<std::byte, ecml::protocol::DatagramHeader::size>
      expected_buffer{std::byte{0x01}, std::byte{0x2A}, std::byte{0x00},
                      std::byte{0x00}, std::byte{0x30}, std::byte{0x01},
                      std::byte{0x02}, std::byte{0x00}, std::byte{0x00},
                      std::byte{0x00}};
  std::array<std::byte, ecml::protocol::DatagramHeader::size> buffer{};
  const auto status = header.serialize(buffer);
  EXPECT_EQ(status, ecml::protocol::DatagramHeaderSerializeStatus::Success);
  EXPECT_TRUE(
      std::equal(buffer.begin(), buffer.end(), expected_buffer.begin()));
}

TEST(DatagramHeaderUnit, SerializeBufferTooSmall) {
  const ecml::protocol::DatagramHeader header{
      .cmd = ecml::protocol::CommandType::APRD,
      .idx = 0x2A,
      .address = 0x01300000,
      .len = 0x0002,
      .more_datagrams = false,
      .circulating = false,
      .irq = 0x0000};
  std::array<std::byte, ecml::protocol::DatagramHeader::size - 1> buffer{};
  const auto status = header.serialize(buffer);
  EXPECT_EQ(status,
            ecml::protocol::DatagramHeaderSerializeStatus::BufferTooSmall);
}

TEST(DatagramHeaderUnit, SerializeLengthOutOfRange) {
  const ecml::protocol::DatagramHeader header{
      .cmd = ecml::protocol::CommandType::APRD,
      .idx = 0x2A,
      .address = 0x01300000,
      .len = ecml::protocol::datagram_max_len + 1,
      .more_datagrams = false,
      .circulating = false,
      .irq = 0x0000};
  std::array<std::byte, ecml::protocol::DatagramHeader::size> buffer{};
  const auto status = header.serialize(buffer);
  EXPECT_EQ(status,
            ecml::protocol::DatagramHeaderSerializeStatus::LengthOutOfRange);
}

TEST(DatagramHeaderUnit, DeserializeSuccessfully) {
  const std::array<std::byte, ecml::protocol::DatagramHeader::size> bytes{
      std::byte{0x01}, std::byte{0x2A}, std::byte{0x00}, std::byte{0x00},
      std::byte{0x30}, std::byte{0x01}, std::byte{0x02}, std::byte{0x00},
      std::byte{0x00}, std::byte{0x00}};
  const auto res = ecml::protocol::DatagramHeader::deserialize(bytes);
  ASSERT_TRUE(res.ok());
  ASSERT_TRUE(res.header.has_value());
  EXPECT_EQ(res.header->cmd, ecml::protocol::CommandType::APRD);
  EXPECT_EQ(res.header->idx, 0x2A);
  EXPECT_EQ(res.header->address, 0x01300000);
  EXPECT_EQ(res.header->len, 2);
  EXPECT_FALSE(res.header->circulating);
  EXPECT_FALSE(res.header->more_datagrams);
  EXPECT_EQ(res.header->irq, 0x0000);
  EXPECT_EQ(res.status,
            ecml::protocol::DatagramHeaderDeserializeResult::Status::Success);
}

TEST(DatagramHeaderUnit, DeserializeBufferTooSmall) {
  const std::array<std::byte, ecml::protocol::DatagramHeader::size - 1> bytes{
      std::byte{0x01}, std::byte{0x2A}, std::byte{0x00},
      std::byte{0x00}, std::byte{0x30}, std::byte{0x01},
      std::byte{0x02}, std::byte{0x00}, std::byte{0x00}};
  const auto res = ecml::protocol::DatagramHeader::deserialize(bytes);
  EXPECT_FALSE(res.ok());
  ASSERT_FALSE(res.header.has_value());
  EXPECT_EQ(
      res.status,
      ecml::protocol::DatagramHeaderDeserializeResult::Status::BufferTooSmall);
}

TEST(DatagramHeaderUnit, DeserializeUnknownCommand) {
  const std::array<std::byte, ecml::protocol::DatagramHeader::size> bytes{
      std::byte{0x0F}, std::byte{0x01}, std::byte{0x00}, std::byte{0x00},
      std::byte{0x00}, std::byte{0x00}, std::byte{0x04}, std::byte{0x00},
      std::byte{0x00}, std::byte{0x00}};
  const auto res = ecml::protocol::DatagramHeader::deserialize(bytes);
  EXPECT_FALSE(res.ok());
  ASSERT_FALSE(res.header.has_value());
  EXPECT_EQ(
      res.status,
      ecml::protocol::DatagramHeaderDeserializeResult::Status::UnknownCommand);
}

TEST(DatagramHeaderUnit, DeserializeReservedBitsMismatch) {
  const std::array<std::byte, ecml::protocol::DatagramHeader::size> bytes{
      std::byte{0x01}, std::byte{0x01}, std::byte{0x00}, std::byte{0x00},
      std::byte{0x00}, std::byte{0x00}, std::byte{0x02}, std::byte{0x08},
      std::byte{0x00}, std::byte{0x00}};
  const auto res = ecml::protocol::DatagramHeader::deserialize(bytes);
  EXPECT_FALSE(res.ok());
  ASSERT_FALSE(res.header.has_value());
  EXPECT_EQ(res.status, ecml::protocol::DatagramHeaderDeserializeResult::
                            Status::ReservedBitsMismatch);
}

TEST(DatagramHeaderUnit, DeserializeLengthOutOfRange) {
  const std::array<std::byte, ecml::protocol::DatagramHeader::size> bytes{
      std::byte{0x01}, std::byte{0x01}, std::byte{0x00}, std::byte{0x00},
      std::byte{0x00}, std::byte{0x00}, std::byte{0xCF}, std::byte{0x05},
      std::byte{0x00}, std::byte{0x00}};
  const auto res = ecml::protocol::DatagramHeader::deserialize(bytes);
  EXPECT_FALSE(res.ok());
  ASSERT_FALSE(res.header.has_value());
  EXPECT_EQ(res.status, ecml::protocol::DatagramHeaderDeserializeResult::
                            Status::LengthOutOfRange);
}

TEST(DatagramUnit, CreateSuccessfully) {
  std::array<std::byte, 4> payload{std::byte{0xDE}, std::byte{0xAD},
                                   std::byte{0xBE}, std::byte{0xEF}};

  ecml::protocol::DatagramHeader header{.cmd =
                                            ecml::protocol::CommandType::APRD,
                                        .idx = sample_idx,
                                        .address = sample_address,
                                        .len = 999,
                                        .more_datagrams = false,
                                        .circulating = false,
                                        .irq = sample_irq};

  auto res = ecml::protocol::Datagram::create(header, payload, sample_wkc);

  ASSERT_TRUE(res.datagram.has_value());
  EXPECT_EQ(res.status, ecml::protocol::DatagramCreateResult::Status::Success);
  EXPECT_EQ(res.datagram->header().len, payload.size());
  EXPECT_EQ(res.datagram->size(), ecml::protocol::DatagramHeader::size +
                                      payload.size() + sizeof(uint16_t));
  EXPECT_EQ(res.datagram->wkc(), sample_wkc);
}

TEST(DatagramUnit, CreatePayloadTooLarge) {
  std::vector<std::byte> oversized_payload(
      ecml::protocol::Datagram::max_payload_size + 1, std::byte{0x00});

  ecml::protocol::DatagramHeader header{
      .cmd = ecml::protocol::CommandType::LWR, .idx = 1, .address = 0x1000};

  auto res = ecml::protocol::Datagram::create(header, oversized_payload, 0);

  EXPECT_FALSE(res.datagram.has_value());
  EXPECT_EQ(res.status,
            ecml::protocol::DatagramCreateResult::Status::PayloadTooLarge);
}

TEST(DatagramUnit, CreateInvalidCommand) {
  std::array<std::byte, 2> payload{};

  ecml::protocol::DatagramHeader header{
      // NOLINTNEXTLINE
      .cmd = static_cast<ecml::protocol::CommandType>(0x0F),
      .idx = 1,
      .address = 0};

  auto res = ecml::protocol::Datagram::create(header, payload, 0);

  EXPECT_FALSE(res.datagram.has_value());
  EXPECT_EQ(res.status,
            ecml::protocol::DatagramCreateResult::Status::InvalidCommand);
}

TEST(DatagramUnit, SerializeSuccessfully) {
  std::array<std::byte, 4> payload{std::byte{0x11}, std::byte{0x22},
                                   std::byte{0x33}, std::byte{0x44}};

  ecml::protocol::DatagramHeader header{.cmd =
                                            ecml::protocol::CommandType::FPRD,
                                        .idx = 0x07,
                                        .address = 0x01300001,
                                        .len = 0,
                                        .more_datagrams = true,
                                        .circulating = false,
                                        .irq = 0x0000};

  auto create_res =
      ecml::protocol::Datagram::create(header, payload, sample_wkc);
  ASSERT_TRUE(create_res.datagram.has_value());

  std::array<std::byte, 16> wire_buffer{};
  const auto status = create_res.datagram->serialize(wire_buffer);

  EXPECT_EQ(status, ecml::protocol::DatagramSerializeStatus::Success);

  EXPECT_EQ(wire_buffer[0], std::byte{0x04});
  EXPECT_EQ(wire_buffer[1], std::byte{0x07});
  EXPECT_EQ(wire_buffer[6], std::byte{0x04});
  EXPECT_EQ(wire_buffer[7], std::byte{0x80});

  EXPECT_EQ(wire_buffer[10], std::byte{0x11});
  EXPECT_EQ(wire_buffer[11], std::byte{0x22});
  EXPECT_EQ(wire_buffer[12], std::byte{0x33});
  EXPECT_EQ(wire_buffer[13], std::byte{0x44});

  EXPECT_EQ(wire_buffer[14], std::byte{0x34});
  EXPECT_EQ(wire_buffer[15], std::byte{0x12});
}

TEST(DatagramUnit, SerializeEmptyPayloadDatagram) {
  std::span<std::byte> empty_payload{};
  ecml::protocol::DatagramHeader header{.cmd = ecml::protocol::CommandType::NOP,
                                        .idx = 0x01,
                                        .address = 0x00000000};

  auto create_res =
      ecml::protocol::Datagram::create(header, empty_payload, 0x0001);
  ASSERT_TRUE(create_res.datagram.has_value());
  EXPECT_EQ(create_res.datagram->size(), min_datagram_size);

  std::array<std::byte, min_datagram_size> wire_buffer{};
  EXPECT_EQ(create_res.datagram->serialize(wire_buffer),
            ecml::protocol::DatagramSerializeStatus::Success);

  EXPECT_EQ(wire_buffer[10], std::byte{0x01});
  EXPECT_EQ(wire_buffer[11], std::byte{0x00});
}

TEST(DatagramUnit, SerializeBufferTooSmall) {
  std::array<std::byte, 4> payload{};
  ecml::protocol::DatagramHeader header{.cmd =
                                            ecml::protocol::CommandType::APRD};

  auto create_res = ecml::protocol::Datagram::create(header, payload, 0);
  ASSERT_TRUE(create_res.datagram.has_value());

  std::array<std::byte, 15> small_buffer{};
  EXPECT_EQ(create_res.datagram->serialize(small_buffer),
            ecml::protocol::DatagramSerializeStatus::BufferTooSmall);
}

TEST(DatagramUnit, DeserializeSuccessfully) {
  std::array<std::byte, 18> raw_frame{
      std::byte{0x01}, std::byte{0x2A}, std::byte{0x00}, std::byte{0x00},
      std::byte{0x30}, std::byte{0x01}, std::byte{0x02}, std::byte{0x00},
      std::byte{0x00}, std::byte{0x00}, std::byte{0x01}, std::byte{0x00},
      std::byte{0x01}, std::byte{0x00}, std::byte{0xFF}, std::byte{0xFF},
      std::byte{0xFF}, std::byte{0xFF}};

  auto res = ecml::protocol::Datagram::deserialize(raw_frame);

  ASSERT_TRUE(res.datagram.has_value());
  EXPECT_EQ(res.status,
            ecml::protocol::DatagramDeserializeResult::Status::Success);
  EXPECT_EQ(res.bytes_consumed, 14U);

  EXPECT_EQ(res.datagram->header().cmd, ecml::protocol::CommandType::APRD);
  EXPECT_EQ(res.datagram->header().idx, sample_idx);
  EXPECT_EQ(res.datagram->header().address, sample_address);
  EXPECT_EQ(res.datagram->header().len, 2U);
  EXPECT_EQ(res.datagram->wkc(), 1U);

  ASSERT_EQ(res.datagram->data().size(), 2U);
  EXPECT_EQ(res.datagram->data().data(),
            raw_frame.data() + ecml::protocol::DatagramHeader::size);

  res.datagram->data()[0] = std::byte{0x08};
  EXPECT_EQ(raw_frame[10], std::byte{0x08});
}

TEST(DatagramUnit, DeserializeRejectsBufferSmallerThanMinimum) {
  std::array<std::byte, min_datagram_size - 1> small_buffer{};

  auto res = ecml::protocol::Datagram::deserialize(small_buffer);

  EXPECT_FALSE(res.datagram.has_value());
  EXPECT_EQ(res.status,
            ecml::protocol::DatagramDeserializeResult::Status::BufferTooSmall);
  EXPECT_EQ(res.bytes_consumed, 0U);
}

TEST(DatagramUnit, DeserializePayloadTruncated) {
  std::array<std::byte, 15> truncated_frame{
      std::byte{0x01}, std::byte{0x01}, std::byte{0x00}, std::byte{0x00},
      std::byte{0x00}, std::byte{0x00}, std::byte{0x04}, std::byte{0x00},
      std::byte{0x00}, std::byte{0x00}, std::byte{0xAA}, std::byte{0xBB},
      std::byte{0xCC}, std::byte{0xDD}, std::byte{0x01}};

  auto res = ecml::protocol::Datagram::deserialize(truncated_frame);

  EXPECT_FALSE(res.datagram.has_value());
  EXPECT_EQ(
      res.status,
      ecml::protocol::DatagramDeserializeResult::Status::PayloadTruncated);
  EXPECT_EQ(res.bytes_consumed, 0U);
}

TEST(DatagramUnit, DeserializeInvalidHeader) {
  std::array<std::byte, 12> bad_reserved_frame{
      std::byte{0x01}, std::byte{0x01}, std::byte{0x00}, std::byte{0x00},
      std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x08},
      std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};

  auto res_reserved = ecml::protocol::Datagram::deserialize(bad_reserved_frame);
  EXPECT_FALSE(res_reserved.datagram.has_value());
  EXPECT_EQ(res_reserved.status,
            ecml::protocol::DatagramDeserializeResult::Status::InvalidHeader);

  std::array<std::byte, 12> oversized_len_frame{
      std::byte{0x01}, std::byte{0x01}, std::byte{0x00}, std::byte{0x00},
      std::byte{0x00}, std::byte{0x00}, std::byte{0xDC}, std::byte{0x05},
      std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};

  auto res_oversized =
      ecml::protocol::Datagram::deserialize(oversized_len_frame);
  EXPECT_FALSE(res_oversized.datagram.has_value());
  EXPECT_EQ(res_oversized.status,
            ecml::protocol::DatagramDeserializeResult::Status::PayloadTooLarge);
}

TEST(DatagramUnit, SerializeDeserilaizeEquality) {
  std::array<std::byte, 6> original_payload{std::byte{0x10}, std::byte{0x20},
                                            std::byte{0x30}, std::byte{0x40},
                                            std::byte{0x50}, std::byte{0x60}};

  ecml::protocol::DatagramHeader original_header{
      .cmd = ecml::protocol::CommandType::LRW,
      .idx = 0x99,
      .address = 0x80010000,
      .len = 6,
      .more_datagrams = true,
      .circulating = true,
      .irq = 0x00A5};

  auto created = ecml::protocol::Datagram::create(original_header,
                                                  original_payload, 0x0003);
  ASSERT_TRUE(created.datagram.has_value());

  std::array<std::byte, 18> wire_buffer{};
  ASSERT_EQ(created.datagram->serialize(wire_buffer),
            ecml::protocol::DatagramSerializeStatus::Success);

  auto parsed = ecml::protocol::Datagram::deserialize(wire_buffer);
  ASSERT_TRUE(parsed.datagram.has_value());
  EXPECT_EQ(parsed.bytes_consumed, wire_buffer.size());

  EXPECT_EQ(parsed.datagram->header().cmd, original_header.cmd);
  EXPECT_EQ(parsed.datagram->header().idx, original_header.idx);
  EXPECT_EQ(parsed.datagram->header().address, original_header.address);
  EXPECT_EQ(parsed.datagram->header().len, original_payload.size());
  EXPECT_EQ(parsed.datagram->header().circulating, original_header.circulating);
  EXPECT_EQ(parsed.datagram->header().more_datagrams,
            original_header.more_datagrams);
  EXPECT_EQ(parsed.datagram->header().irq, original_header.irq);
  EXPECT_EQ(parsed.datagram->wkc(), 0x0003);
  EXPECT_TRUE(std::equal(original_payload.begin(), original_payload.end(),
                         parsed.datagram->data().begin()));
}
