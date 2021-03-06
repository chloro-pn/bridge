#pragma once

#include <cstring>
#include <string>
#include <vector>

#include "bridge/type_trait.h"

namespace bridge {

template <typename T>
inline void serialize(const T& obj, std::vector<char>& container);

template <size_t n>
inline void serialize(const char (&arr)[n], std::vector<char>& container) {
  size_t real_n = n;
  if (n > 0 && arr[n - 1] == '\0') {
    real_n = n - 1;
  }
  container.resize(real_n);
  memcpy(&container[0], &(arr[0]), real_n);
}

template <>
inline void serialize(const std::string& obj, std::vector<char>& container) {
  size_t len = obj.size();
  container.resize(len);
  memcpy(&container[0], obj.data(), len);
}

template <>
inline void serialize(const std::vector<char>& obj, std::vector<char>& container) {
  container = obj;
}

template <typename T>
requires bridge_integral<T> || bridge_floating<T>
inline void serialize(const T& obj, std::vector<char>& container) {
  T tmp = obj;
  container.resize(sizeof(obj));
  if (Endian::Instance().GetEndianType() == Endian::Type::Little) {
    tmp = flipByByte(tmp);
  }
  memcpy(&container[0], &tmp, sizeof(tmp));
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

template <typename Outer>
requires bridge_outer_concept<Outer>
void seriObjectType(ObjectType type, Outer& outer) {
  assert(type != ObjectType::Invalid);
  char tmp = ObjectTypeToChar(type);
  outer.push_back(tmp);
}
}  // namespace bridge