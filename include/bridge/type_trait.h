#pragma once

#include <string>
#include <vector>
#include <type_traits>

namespace bridge {

template <typename T>
struct NoRefNoPointer {
  static constexpr bool value = true;
};

template <typename T>
struct NoRefNoPointer<T&> {
  static constexpr bool value = false;
};

template <typename T>
struct NoRefNoPointer<T&&> {
  static constexpr bool value = false;
};

template <typename T>
struct NoRefNoPointer<T*> {
  static constexpr bool value = false;
};

template <typename T>
struct NoRefNoPointer<T* const> {
  static constexpr bool value = false;
};

template <typename T>
concept bridge_integral = std::is_same_v<int32_t, T> || std::is_same_v<uint32_t, T> || std::is_same_v<int64_t, T> ||
                          std::is_same_v<uint64_t, T>;

template <typename T>
struct bridge_cstr_ref_trait {
  static constexpr bool value = false;
};

template <size_t n>
struct bridge_cstr_ref_trait<char(&)[n]> {
  static constexpr bool value = true;
};

template <size_t n>
struct bridge_cstr_ref_trait<const char(&)[n]> {
  static constexpr bool value = true;
};

template <typename T>
concept bridge_data_type = bridge_integral<T> || std::is_same_v<std::string, T> || std::is_same_v<std::vector<char>, T> || bridge_cstr_ref_trait<T>::value;

};  // namespace bridge