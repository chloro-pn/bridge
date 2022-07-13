#pragma once

#include "bridge/object.h"
#include "bridge/type_trait.h"
#include <vector>
#include <unordered_map>

#include <iostream>

namespace bridge {

template <typename T> requires bridge_data_type<T>
inline std::unique_ptr<Object> vector(const std::vector<T>& vec) {
  auto ret = ValueFactory<Array>();
  for(const auto& each : vec) {
    auto v = ValueFactory<Data>(each);
    std::cout << int(v->GetDataType()) << std::endl;
    ret->Insert(std::move(v));
  }
  return ret;
}

template <typename T> requires bridge_data_type<T>
inline std::unique_ptr<Object> unorderde_map(const std::unordered_map<std::string, T>& map) {
  auto ret = ValueFactory<Map>();
  for(const auto& each : map) {
    ret->Insert(each.first, ValueFactory<Data>(each.second));
  }
  return ret;
}

}