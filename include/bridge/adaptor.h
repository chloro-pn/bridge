#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "bridge/object.h"
#include "bridge/type_trait.h"

namespace bridge {

template <typename T>
requires bridge_type<T>
inline unique_ptr<Object> adaptor(const T& t) { return ValueFactory<Data>(t); }

// 必须前置声明，否则vector版本看不到unordered_map版本
template <typename T>
requires bridge_adaptor_type<T> || bridge_type<T>
inline unique_ptr<Object> adaptor(const std::vector<T>& vec);

template <typename T>
requires bridge_adaptor_type<T> || bridge_type<T>
inline unique_ptr<Object> adaptor(const std::unordered_map<std::string, T>& vec);

template <typename T>
requires bridge_adaptor_type<T> || bridge_type<T>
inline unique_ptr<Object> adaptor(const std::unordered_map<std::string_view, T>& vec);

// 声明结束

template <typename T>
requires bridge_adaptor_type<T> || bridge_type<T>
inline unique_ptr<Object> adaptor(const std::vector<T>& vec) {
  auto ret = ValueFactory<Array>();
  for (const auto& each : vec) {
    auto v = adaptor(each);
    ret->Insert(std::move(v));
  }
  return ret;
}

template <typename T>
requires bridge_adaptor_type<T> || bridge_type<T>
inline unique_ptr<Object> adaptor(const std::unordered_map<std::string, T>& vec) {
  auto ret = ValueFactory<Map>();
  for (const auto& each : vec) {
    ret->Insert(each.first, adaptor(each.second));
  }
  return ret;
}

template <typename T>
requires bridge_adaptor_type<T> || bridge_type<T>
inline unique_ptr<Object> adaptor(const std::unordered_map<std::string_view, T>& vec) {
  auto ret = ValueFactory<Map>();
  for (const auto& each : vec) {
    ret->Insert(std::string(each.first), adaptor(each.second));
  }
  return ret;
}

}  // namespace bridge