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
inline unique_ptr<Object> adaptor(const T& t, BridgePool& bp) { return bp.data(t); }

// 必须前置声明，否则vector版本看不到unordered_map版本
template <typename T>
requires bridge_adaptor_type<T> || bridge_type<T>
inline unique_ptr<Object> adaptor(const std::vector<T>& vec, BridgePool& bp);

template <typename T>
requires bridge_adaptor_type<T> || bridge_type<T>
inline unique_ptr<Object> adaptor(const std::unordered_map<std::string, T>& vec, BridgePool& bp);

template <typename T>
requires bridge_adaptor_type<T> || bridge_type<T>
inline unique_ptr<Object> adaptor(const std::unordered_map<std::string_view, T>& vec, BridgePool& bp);

// 声明结束

template <typename T>
requires bridge_adaptor_type<T> || bridge_type<T>
inline unique_ptr<Object> adaptor(const std::vector<T>& vec, BridgePool& bp) {
  auto ret = bp.array();
  for (const auto& each : vec) {
    auto v = adaptor(each, bp);
    ret->Insert(std::move(v));
  }
  return ret;
}

template <typename T>
requires bridge_adaptor_type<T> || bridge_type<T>
inline unique_ptr<Object> adaptor(const std::unordered_map<std::string, T>& vec, BridgePool& bp) {
  auto ret = bp.map();
  for (const auto& each : vec) {
    ret->Insert(each.first, adaptor(each.second, bp));
  }
  return ret;
}

template <typename T>
requires bridge_adaptor_type<T> || bridge_type<T>
inline unique_ptr<Object> adaptor(const std::unordered_map<std::string_view, T>& vec, BridgePool& bp) {
  auto ret = bp.map_view();
  for (const auto& each : vec) {
    ret->Insert(std::string(each.first), adaptor(each.second, bp));
  }
  return ret;
}

}  // namespace bridge