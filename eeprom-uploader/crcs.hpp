#ifndef CRCS_HPP_h987nx7rf91z9p8m
#define CRCS_HPP_h987nx7rf91z9p8m

#include <cctype>

namespace crcs {

using byte = unsigned char;
using std::size_t;

constexpr const byte Crc8Gen = 0x1d;

static constexpr byte
crc8(byte b, byte crc = 0x00) {
  crc ^= b;
  for (auto b = 0; b < 8; ++b) {
    if ((crc & 0x80)) {
      crc <<= 1;
      crc ^= Crc8Gen;
    } else {
      crc <<= 1;
    }
  }
  return crc;
}

static constexpr byte
crc8be32(uint32_t u, byte crc = 0x00) {
  crc = crc8(static_cast<byte>((u >> 24) & 0xff), crc);
  crc = crc8(static_cast<byte>((u >> 16) & 0xff), crc);
  crc = crc8(static_cast<byte>((u >> 8) & 0xff), crc);
  crc = crc8(static_cast<byte>(u & 0xff), crc);
  return crc;
}

static constexpr byte
crc8be16(uint16_t u, byte crc = 0x00) {
  crc = crc8(static_cast<byte>((u >> 8) & 0xff), crc);
  crc = crc8(static_cast<byte>(u & 0xff), crc);
  return crc;
}

template <typename It>
static constexpr byte
crc8(It begin, It end, byte crc = 0x00) {
  while (begin != end) {
    crc = crc8(*begin++, crc);
  }
  return crc;
}


template <size_t N>
static inline constexpr byte
crc8(const byte(&buf)[N], byte crc = 0x00) {
  return crc8(buf, buf + N, crc);
}

} // namespace crcs

#endif // CRCS_HPP_h987nx7rf91z9p8m
