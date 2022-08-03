#pragma once

namespace bridge {

enum class ObjectType : char {
  Data,
  Map,
  Array,
  Invalid,
};

inline char ObjectTypeToChar(ObjectType type) { return static_cast<char>(type); }

inline const char* ObjectTypeToStr(ObjectType type) {
  switch (type) {
    case ObjectType::Data:
      return "Data";
    case ObjectType::Map:
      return "Map";
    case ObjectType::Array:
      return "Array";
    default:
      return "Invalid";
  }
}

inline ObjectType CharToObjectType(char c) {
  if (c == ObjectTypeToChar(ObjectType::Data)) {
    return ObjectType::Data;
  } else if (c == ObjectTypeToChar(ObjectType::Map)) {
    return ObjectType::Map;
  } else if (c == ObjectTypeToChar(ObjectType::Array)) {
    return ObjectType::Array;
  }
  return ObjectType::Invalid;
}

}  // namespace bridge