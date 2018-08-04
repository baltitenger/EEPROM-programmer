#ifndef CRCS_HPP_h987nx7rf91z9p8m
#define CRCS_HPP_h987nx7rf91z9p8m

namespace crcs {

using byte = unsigned char;
using std::size_t;

constexpr const byte Crc8Gen = 0x1d;

template <typename It>
static constexpr byte
crc8(It begin, It end) {
  byte res = 0x00;
  while (begin != end) {
    res ^= *begin++;
    for (size_t b = 0; b < 8; ++b) {
      if ((res & 0x80)) {
        res <<= 1;
        res ^= Crc8Gen;
      } else {
        res <<= 1;
      }
    }
  }
  return res;
}

template <size_t N>
static inline constexpr byte
crc8(const byte(&buf)[N]) {
  return crc8(buf, buf + N);
}

} // namespace crcs

#endif // CRCS_HPP_h987nx7rf91z9p8m
