#pragma once

#include "referee_host.hpp"

using Bytes = std::vector<uint8_t>;

// Independent reflected CRC reference, per the protocol appendix.
static uint16_t crc(const uint8_t* data, size_t size, uint16_t value, uint16_t poly)
{
  while (size--)
  {
    value ^= *data++;
    for (int bit = 0; bit < 8; ++bit)
    {
      value = (value >> 1) ^ ((value & 1) ? poly : 0);
    }
  }
  return value;
}

static Bytes frame(uint16_t id, const Bytes& payload, uint8_t seq = 0)
{
  Bytes bytes{0xA5, static_cast<uint8_t>(payload.size()),
              static_cast<uint8_t>(payload.size() >> 8), seq, 0};
  bytes[4] = static_cast<uint8_t>(crc(bytes.data(), 4, 0xFF, 0x8C));
  bytes.push_back(id & 0xFF);
  bytes.push_back(id >> 8);
  bytes.insert(bytes.end(), payload.begin(), payload.end());
  const auto CRC = crc(bytes.data(), bytes.size(), 0xFFFF, 0x8408);
  bytes.push_back(CRC & 0xFF);
  bytes.push_back(CRC >> 8);
  return bytes;
}

static void load(LibXR::UART& uart, const Bytes& bytes)
{
  uart.rx = bytes;
  uart.cursor = 0;
  uart.failed_reads = 0;
}

static bool receive(Referee& ref, LibXR::UART& uart, uint16_t id, const Bytes& payload)
{
  load(uart, frame(id, payload));
  ref.FindHeader();
  ref.last_parse_ = ref.ParseData();
  ref.Publish();
  return ref.last_parse_;
}
