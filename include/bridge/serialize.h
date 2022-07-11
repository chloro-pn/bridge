#pragma once

#include <cstring>
#include <string>
#include <vector>

namespace bridge {

template <typename T>
inline void serialize(const T& obj, std::vector<char>& container);

template <size_t n>
inline void serialize(const char (&arr)[n], std::vector<char>& container) {
  container.resize(n);
  memcpy(&container[0], &(arr[0]), n);
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

template <typename Outer>
void seriDataType(uint8_t dt, Outer& outer) {
  outer.push_back(static_cast<char>(dt));
}

template <typename Outer>
void seriLength(uint32_t length, Outer& outer) {
  char buf[128];
  unsigned char bytes = 0;
  varint_encode(length, buf, sizeof(buf), &bytes);
  outer.append(static_cast<const char*>(buf), static_cast<size_t>(bytes));
}

template <typename Outer>
void seriObjectType(ObjectType type, Outer& outer) {
  assert(type != ObjectType::Invalid);
  char tmp = ObjectTypeToChar(type);
  outer.push_back(tmp);
}
}  // namespace bridge