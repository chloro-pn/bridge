#pragma once

#include <unordered_map>
#include <vector>

#include "bridge/object.h"
#include "bridge/type_trait.h"

namespace bridge {

template <typename T>
requires bridge_data_type<T> inline std::unique_ptr<Object> vector(const std::vector<T>& vec) {
  auto ret = ValueFactory<Array>();
  for (const auto& each : vec) {
    auto v = ValueFactory<Data>(each);
    ret->Insert(std::move(v));
  }
  return ret;
}

template <typename T>
requires bridge_data_type<T> inline std::unique_ptr<Object> unorderde_map(
    const std::unordered_map<std::string, T>& map) {
  auto ret = ValueFactory<Map>();
  for (const auto& each : map) {
    ret->Insert(each.first, ValueFactory<Data>(each.second));
  }
  return ret;
}

}  // namespace bridge