#pragma once

#include <string>
#include <type_traits>
#include <vector>

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
struct bridge_cstr_ref_trait<char (&)[n]> {
  static constexpr bool value = true;
};

template <size_t n>
struct bridge_cstr_ref_trait<const char (&)[n]> {
  static constexpr bool value = true;
};

/*
 * 自定义类型可以接入bridge
 */
template <typename T>
concept bridge_custom_type = requires(const T& t, const std::vector<char>& bytes) {
  { t.SerializeToBridge() }
  ->std::same_as<std::vector<char>>;
  { T::ConstructFromBridge(bytes) }
  ->std::same_as<T>;
};

template <typename T>
concept bridge_data_type = bridge_integral<T> || std::is_same_v<std::string, T> ||
                           std::is_same_v<std::vector<char>, T> || bridge_cstr_ref_trait<T>::value;

template <typename T>
concept bridge_type = bridge_custom_type<T> || bridge_data_type<T>;

template <typename T>
concept bridge_inner_concept = requires(const T& inner, size_t n) {
  { inner.curAddr() }
  ->std::same_as<const char*>;
  {inner.skip(n)};
  { inner.outOfRange() }
  ->std::same_as<bool>;
};

template <typename T>
concept bridge_outer_concept = requires(T& outer, char c, std::string s, const char* ptr, size_t len) {
  {outer.push_back(c)};
  {outer.append(s)};
  {outer.append(ptr, len)};
};

};  // namespace bridge