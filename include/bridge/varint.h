#pragma once

#include <cassert>

static constexpr char MSB = 0x80;
static constexpr char MSBALL = ~0x7F;

static constexpr unsigned long long N1 = 128;  // 2 ^ 7
static constexpr unsigned long long N2 = 16384;
static constexpr unsigned long long N3 = 2097152;
static constexpr unsigned long long N4 = 268435456;
static constexpr unsigned long long N5 = 34359738368;
static constexpr unsigned long long N6 = 4398046511104;
static constexpr unsigned long long N7 = 562949953421312;
static constexpr unsigned long long N8 = 72057594037927936;
static constexpr unsigned long long N9 = 9223372036854775808U;

inline char* varint_encode(unsigned long long n, char* buf, int len, unsigned char* bytes) {
  char* ptr = buf;

  while (n & MSBALL) {
    *(ptr++) = (n & 0xFF) | MSB;
    n = n >> 7;
    assert((ptr - buf) < len);
  }
  *ptr = n;
  if (bytes != 0) *bytes = ptr - buf + 1;

  return buf;
}

inline unsigned long long varint_decode(const char* buf, int len, unsigned char* bytes) {
  unsigned long long result = 0;
  int bits = 0;
  const char* ptr = buf;
  unsigned long long ll;
  while (*ptr & MSB) {
    ll = *ptr;
    result += ((ll & 0x7F) << bits);
    ptr++;
    bits += 7;
    assert((ptr - buf) < len);
  }
  ll = *ptr;
  result += ((ll & 0x7F) << bits);

  if (bytes != 0) *bytes = ptr - buf + 1;

  return result;
}

inline int varint_encoding_length(unsigned long long n) {
  return (n < N1
              ? 1
              : n < N2 ? 2
                       : n < N3 ? 3 : n < N4 ? 4 : n < N5 ? 5 : n < N6 ? 6 : n < N7 ? 7 : n < N8 ? 8 : n < N9 ? 9 : 10);
}
