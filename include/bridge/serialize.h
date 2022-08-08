#pragma once

#include <cstring>
#include <string>
#include <vector>

#include "bridge/data_type.h"
#include "bridge/type_trait.h"
#include "bridge/util.h"

namespace bridge {

template <size_t n>
inline void serialize(const char (&arr)[n], bridge_variant& container) {
  size_t real_n = n;
  if (n > 0 && arr[n-1] == '\0') {
    real_n = n - 1;
  }
  std::string_view tmp(&arr[0], real_n);
  container.construct<std::string>(tmp);
}

inline void serialize(const std::string& obj, bridge_variant& container) { container.construct<std::string>(obj); }

inline void serialize(std::string&& obj, bridge_variant& container) { container.construct<std::string>(std::move(obj)); }

inline void serialize(const std::vector<char>& obj, bridge_variant& container) {
  container.construct<std::vector<char>>(obj);
}

inline void serialize(std::vector<char>&& obj, bridge_variant& container) {
  container.construct<std::vector<char>>(std::move(obj));
}

template <typename T>
requires bridge_integral<T> || bridge_floating<T>
inline void serialize(const T& obj, bridge_variant& container) {
  container.construct<T>(obj);
}

template <typename Outer>
requires bridge_outer_concept<Outer>
void seriDataType(uint8_t dt, Outer& outer) { outer.push_back(static_cast<char>(dt)); }

template <typename Outer>
requires bridge_outer_concept<Outer>
void seriLength(uint32_t length, Outer& outer) {
  char buf[128];
  unsigned char bytes = 0;
  varint_encode(length, buf, sizeof(buf), &bytes);
  outer.append(static_cast<const char*>(buf), static_cast<size_t>(bytes));
}

#define BRIDGE_SERI(data_type, real_type) \
else if (dt == data_type) { \
  real_type t = data.get<real_type>(); \
  if (Endian::Instance().GetEndianType() == Endian::Type::Little) { \
    t = flipByByte(t); \
  } \
  seriLength(sizeof(t), outer); \
  outer.append((const char*)&t, sizeof(t)); \
}

template <typename Outer>
requires bridge_outer_concept<Outer>
void seriData(uint8_t dt, const bridge_variant& data, Outer& outer) {
  if (dt == BRIDGE_BYTES || dt == BRIDGE_CUSTOM) {
    const std::vector<char>& tmp = data.get<std::vector<char>>();
    seriLength(tmp.size(), outer);
    outer.append(&tmp[0], tmp.size());
  } else if (dt == BRIDGE_STRING) {
    const std::string& tmp = data.get<std::string>();
    seriLength(tmp.size(), outer);
    outer.append(&tmp[0], tmp.size());
  }
  BRIDGE_SERI(BRIDGE_INT32, int32_t)
  BRIDGE_SERI(BRIDGE_UINT32, uint32_t)
  BRIDGE_SERI(BRIDGE_INT64, int64_t)
  BRIDGE_SERI(BRIDGE_UINT64, uint64_t)
  BRIDGE_SERI(BRIDGE_FLOAT, float)
  BRIDGE_SERI(BRIDGE_DOUBLE, double)
  else {
    assert(false);
  }
}

template <typename Outer>
requires bridge_outer_concept<Outer>
void seriObjectType(ObjectType type, Outer& outer) {
  assert(type != ObjectType::Invalid);
  char tmp = ObjectTypeToChar(type);
  outer.push_back(tmp);
}
}  // namespace bridge