#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace bridge {

#define BRIDGE_BYTES 0x00
#define BRIDGE_STRING 0x01
#define BRIDGE_INVALID 0x0D

template <typename T>
struct InnerDataTypeTrait {
  static constexpr uint8_t dt = BRIDGE_INVALID;
};

#define DataTypeTraitStruct(x, y)             \
  template <>                                 \
  struct InnerDataTypeTrait<x> {              \
    static constexpr uint8_t dt = BRIDGE_##y; \
  };

DataTypeTraitStruct(std::vector<char>, BYTES);
DataTypeTraitStruct(std::string, STRING);

template <typename T>
struct DataTypeTrait {
  static constexpr uint8_t dt = InnerDataTypeTrait<T>::dt;
};

template <size_t n>
struct DataTypeTrait<char (&)[n]> {
  static constexpr uint8_t dt = BRIDGE_STRING;
};

template <size_t n>
struct DataTypeTrait<const char (&)[n]> {
  static constexpr uint8_t dt = BRIDGE_STRING;
};
}  // namespace bridge