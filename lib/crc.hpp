#ifndef CRC_HPP_h987nx7rf91z9p8m
#define CRC_HPP_h987nx7rf91z9p8m

namespace crc {

using byte = unsigned char;
using std::size_t;

// IndexSeq

template <size_t... Is_>
struct IndexSeq {
};

template <typename IndexSeq_, size_t End_>
struct MakeIndexSeqImpl;

template <size_t... Indexes_, size_t End_>
struct MakeIndexSeqImpl<IndexSeq<Indexes_...>, End_> {
  using type = typename MakeIndexSeqImpl<IndexSeq<End_ - 1, Indexes_...>, End_ - 1>::type;
};

template <size_t... Indexes_>
struct MakeIndexSeqImpl<IndexSeq<Indexes_...>, 0> {
  using type = IndexSeq<Indexes_...>;
};

template <size_t End_>
struct MakeIndexSeq {
  using type = typename MakeIndexSeqImpl<IndexSeq<>, End_>::type;
};

template <size_t End_>
using MakeIndexSeq_t = typename MakeIndexSeq<End_>::type;

// END IndexSeq

struct LookupTable {
  static constexpr const size_t SIZE = 256;
  byte data[SIZE];

  byte &
  operator[](size_t i) {
    return data[i];
  }
  byte
  operator[](size_t i) const {
    return data[i];
  }

  const byte *
  begin() const {
    return data;
  }
  const byte *
  end() const {
    return data + SIZE;
  }
};

constexpr const byte Crc8Gen = 0x1d;

static constexpr byte
calcCrc8TableElem(byte d) {
  for (size_t b = 0; b < 8; ++b) {
    if ((d & 0x80)) {
      d <<= 1;
      d ^= Crc8Gen;
    } else {
      d <<= 1;
    }
  }
  return d;
}

template <size_t... Is_>
static constexpr LookupTable
createCrc8Table(IndexSeq<Is_...>) {
  return { calcCrc8TableElem(Is_)... };
}

static constexpr const LookupTable Crc8Table = createCrc8Table(MakeIndexSeq_t<256>{});

template <typename It>
static constexpr byte
crc8(It begin, It end) {
  byte res = 0x00;
  while (begin != end) {
    res = Crc8Table[*begin++ ^ res];
  }
  return res;
}

template <size_t N>
static inline constexpr byte
crc8(const byte(&buf)[N]) {
  return crc8(buf, buf + N);
}

} // namespace crc;

#endif // CRC_HPP_h987nx7rf91z9p8m
