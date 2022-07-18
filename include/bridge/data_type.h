#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "bridge/type_trait.h"

namespace bridge {

#define BRIDGE_BYTES 0x00
#define BRIDGE_STRING 0x01
#define BRIDGE_INT32 0x02
#define BRIDGE_UINT32 0x03
#define BRIDGE_INT64 0x04
#define BRIDGE_UINT64 0x05
#define BRIDGE_CUSTOM 0x0C
#define BRIDGE_INVALID 0x0D

template <typename T>
struct InnerDataTypeTrait {
  static constexpr uint8_t dt = BRIDGE_CUSTOM;
};

#define DataTypeTraitStruct(x, y)             \
  template <>                                 \
  struct InnerDataTypeTrait<x> {              \
    static constexpr uint8_t dt = BRIDGE_##y; \
  };

DataTypeTraitStruct(std::vector<char>, BYTES);
DataTypeTraitStruct(std::string, STRING);
DataTypeTraitStruct(int32_t, INT32);
DataTypeTraitStruct(uint32_t, UINT32);
DataTypeTraitStruct(int64_t, INT64);
DataTypeTraitStruct(uint64_t, UINT64);

template <typename T>
struct DataTypeTrait {
  using decay_t = std::remove_const_t<std::remove_reference_t<T>>;
  static constexpr uint8_t dt = InnerDataTypeTrait<decay_t>::dt;
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