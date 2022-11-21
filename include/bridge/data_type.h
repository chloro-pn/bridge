#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "bridge/type_trait.h"
#include "bridge/variant.h"

namespace bridge {

#define BRIDGE_BYTES 0x00
#define BRIDGE_STRING 0x01
#define BRIDGE_INT32 0x02
#define BRIDGE_UINT32 0x03
#define BRIDGE_INT64 0x04
#define BRIDGE_UINT64 0x05
#define BRIDGE_FLOAT 0x06
#define BRIDGE_DOUBLE 0x07
#define BRIDGE_CUSTOM 0x0C
#define BRIDGE_INVALID 0x0D

#define BRIDGE_CHECK_DATA_TYPE(t)                                                                                \
  if (t != BRIDGE_BYTES && t != BRIDGE_STRING && t != BRIDGE_INT32 && t != BRIDGE_UINT32 && t != BRIDGE_INT64 && \
      t != BRIDGE_UINT64 && t != BRIDGE_FLOAT && t != BRIDGE_DOUBLE && t != BRIDGE_CUSTOM)                       \
    throw std::runtime_error("datatype check error");

/*
 * 在BRIDGE_xxx整型字面量和bridge支持的类型间建立映射关系
 * 这种映射关系并不是一一对应的，例如：
 *   c-style string、std::string、std::string_view均映射为BRIDGE_STRING;
 *   BRIDGE_CUSTOM 和 BRIDGE_BYTES 均映射为std::vector<char>;
 * 但是不会出现歧义。
 */

template <typename T>
struct InnerDataTypeTrait;

template <typename T>
requires bridge_custom_type<T>
struct InnerDataTypeTrait<T> {
  static constexpr uint8_t dt = BRIDGE_CUSTOM;
};

#define DataTypeTraitStruct(x, y)             \
  template <>                                 \
  struct InnerDataTypeTrait<x> {              \
    static constexpr uint8_t dt = BRIDGE_##y; \
  };

DataTypeTraitStruct(bridge_binary_type, BYTES);
DataTypeTraitStruct(std::string, STRING);
DataTypeTraitStruct(int32_t, INT32);
DataTypeTraitStruct(uint32_t, UINT32);
DataTypeTraitStruct(int64_t, INT64);
DataTypeTraitStruct(uint64_t, UINT64);
DataTypeTraitStruct(float, FLOAT);
DataTypeTraitStruct(double, DOUBLE);

inline const char* DataTypeToStr(uint8_t data_type) {
  switch (data_type) {
    case BRIDGE_BYTES: {
      return "bytes";
    }
    case BRIDGE_STRING: {
      return "string";
    }
    case BRIDGE_INT32: {
      return "int32";
    }
    case BRIDGE_UINT32: {
      return "uint32";
    }
    case BRIDGE_INT64: {
      return "int64";
    }
    case BRIDGE_UINT64: {
      return "uint64";
    }
    case BRIDGE_FLOAT: {
      return "float";
    }
    case BRIDGE_DOUBLE: {
      return "double";
    }
    case BRIDGE_CUSTOM: {
      return "custom";
    }
    default: {
      return "invalid";
    }
  }
}

/*
 * 根据指定的类型获取对应的整型字面量
 */

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

using bridge_variant = variant<bridge_binary_type, std::string, int32_t, uint32_t, int64_t, uint64_t, float, double>;
// view类型只对于整型数据直接持有，对于字节数组和字符串类型持有string_view引用
using bridge_view_variant = variant<std::string_view, int32_t, uint32_t, int64_t, uint64_t, float, double>;
}  // namespace bridge