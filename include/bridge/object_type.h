#pragma once

namespace bridge {

enum class ObjectType : char {
  Data,
  Map,
  Array,
  Invalid,
};

inline char ObjectTypeToChar(ObjectType type) { return static_cast<char>(type); }

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