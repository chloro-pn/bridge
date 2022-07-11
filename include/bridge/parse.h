#pragma once

#include <cassert>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

#include "bridge/object_type.h"
#include "bridge/varint.h"

namespace bridge {

template <typename T>
inline T parse(const char* ptr, size_t n);

template <>
inline std::string parse(const char* ptr, size_t n) {
  std::string ret;
  if (ptr != nullptr) {
    assert(n != 0);
    ret.resize(n);
    memcpy(&ret[0], ptr, n);
  }
  return ret;
}

template <>
inline std::vector<char> parse(const char* ptr, size_t n) {
  std::vector<char> ret;
  if (ptr != nullptr) {
    assert(n != 0);
    ret.resize(n);
    memcpy(&ret[0], ptr, n);
  }
  return ret;
}

template <typename T>
T parse(const std::vector<char>& data) {
  if (data.empty() == true) {
    return parse<T>(nullptr, 0);
  }
  return parse<T>(&data[0], data.size());
}

template <typename Inner>
uint64_t parseLength(const Inner& inner, size_t& offset) {
  unsigned long long tmp;
  unsigned char bytes;
  tmp = varint_decode(inner.curAddr(), 128, &bytes);
  inner.skip(bytes);
  offset += bytes;
  return tmp;
}

template <typename Inner>
uint8_t parseDataType(const Inner& inner, size_t& offset) {
  uint8_t tmp = static_cast<uint8_t>(*inner.curAddr());
  inner.skip(sizeof(tmp));
  offset += sizeof(tmp);
  return tmp;
}

template <typename Inner>
ObjectType parseObjectType(const Inner& inner, size_t& offset) {
  char tmp = *inner.curAddr();
  inner.skip(sizeof(tmp));
  offset += sizeof(tmp);
  auto ret = CharToObjectType(tmp);
  assert(ret != ObjectType::Invalid);
  return ret;
}

}  // namespace bridge