#pragma once

#include <cassert>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include "bridge/data_type.h"
#include "bridge/object_type.h"
#include "bridge/type_trait.h"
#include "bridge/util.h"
#include "bridge/varint.h"

namespace bridge {

template <typename T>
const T& parse(const bridge_variant& data) {
  return data.get<T>();
}

template <typename Inner>
uint64_t parseLength(const Inner& inner, size_t& offset) {
  BRIDGE_CHECK_EMPTY(inner);
  unsigned long long tmp;
  unsigned char bytes;
  tmp = varint_decode(inner.curAddr(), inner.remain(), &bytes);
  inner.skip(bytes);
  BRIDGE_CHECK_OOR(inner);
  offset += bytes;
  return tmp;
}

inline uint64_t parseLength(const std::string& inner, size_t offset, char& skip) {
  if (inner.size() <= offset) {
    throw std::runtime_error("parseLength error : inner empty");
  }
  unsigned long long tmp;
  unsigned char bytes;
  tmp = varint_decode(&inner[offset], inner.size() - offset, &bytes);
  skip = bytes;
  if (inner.size() < offset + skip) {
    throw std::runtime_error("parseLength error : inner out of range");
  }
  return tmp;
}

template <typename Inner>
uint8_t parseDataType(const Inner& inner, size_t& offset) {
  BRIDGE_CHECK_EMPTY(inner);
  uint8_t tmp = static_cast<uint8_t>(*inner.curAddr());
  inner.skip(sizeof(tmp));
  BRIDGE_CHECK_OOR(inner);
  offset += sizeof(tmp);
  return tmp;
}

#define BRIDGE_PARSE(dt, real_type)                                   \
  else if (data_type == dt) {                                         \
    real_type t{0};                                                   \
    assert(sizeof(t) == len);                                         \
    memcpy(&t, ptr, len);                                             \
    if (Endian::Instance().GetEndianType() == Endian::Type::Little) { \
      t = flipByByte(t);                                              \
    }                                                                 \
    data.construct<real_type>(t);                                     \
  }

inline void parseData(uint8_t data_type, const char* ptr, size_t len, bridge_variant& data) {
  if (data_type == BRIDGE_BYTES || data_type == BRIDGE_CUSTOM) {
    data.construct<std::vector<char>>(ptr, ptr + len);
  } else if (data_type == BRIDGE_STRING) {
    data.construct<std::string>(ptr, len);
  }
  BRIDGE_PARSE(BRIDGE_INT32, int32_t)
  BRIDGE_PARSE(BRIDGE_UINT32, uint32_t)
  BRIDGE_PARSE(BRIDGE_INT64, int64_t)
  BRIDGE_PARSE(BRIDGE_UINT64, uint64_t)
  BRIDGE_PARSE(BRIDGE_FLOAT, float)
  BRIDGE_PARSE(BRIDGE_DOUBLE, double)
  else {
    throw std::runtime_error("parse data error : invalid data_type");
  }
}

inline void parseData(uint8_t data_type, const char* ptr, size_t len, bridge_view_variant& data) {
  if (data_type == BRIDGE_BYTES || data_type == BRIDGE_STRING) {
    data.construct<std::string_view>(ptr, len);
  }
  BRIDGE_PARSE(BRIDGE_INT32, int32_t)
  BRIDGE_PARSE(BRIDGE_UINT32, uint32_t)
  BRIDGE_PARSE(BRIDGE_INT64, int64_t)
  BRIDGE_PARSE(BRIDGE_UINT64, uint64_t)
  BRIDGE_PARSE(BRIDGE_FLOAT, float)
  BRIDGE_PARSE(BRIDGE_DOUBLE, double)
  else {
    throw std::runtime_error("parse data error : invalid data_type");
  }
}

template <typename Inner>
ObjectType parseObjectType(const Inner& inner, size_t& offset) {
  BRIDGE_CHECK_EMPTY(inner);
  char tmp = *inner.curAddr();
  inner.skip(sizeof(tmp));
  BRIDGE_CHECK_OOR(inner);
  offset += sizeof(tmp);
  tmp = tmp & 0X7F;
  auto ret = CharToObjectType(tmp);
  if (ret == ObjectType::Invalid) {
    throw std::runtime_error("parse object type error");
  }
  return ret;
}

template <typename Inner>
ObjectType parseObjectType(const Inner& inner, size_t& offset, bool& need_to_split) {
  BRIDGE_CHECK_EMPTY(inner);
  char tmp = *inner.curAddr();
  inner.skip(sizeof(tmp));
  BRIDGE_CHECK_OOR(inner);
  offset += sizeof(tmp);
  if ((tmp & 0x80) >> 1 == 0) {
    need_to_split = false;
  } else {
    need_to_split = true;
  }
  tmp = tmp & 0X7F;
  auto ret = CharToObjectType(tmp);
  if (ret == ObjectType::Invalid) {
    throw std::runtime_error("parse object type error");
  }
  return ret;
}

}  // namespace bridge