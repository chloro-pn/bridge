#pragma once

#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
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
concept bridge_floating = std::is_same_v<float, T> || std::is_same_v<double, T>;

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
  { t.SerializeToBridge() } -> std::same_as<std::vector<char>>;
  { T::ConstructFromBridge(bytes) } -> std::same_as<T>;
};

template <typename T>
concept bridge_data_type = bridge_integral<T> || bridge_floating<T> || std::is_same_v<std::string, T> ||
    std::is_same_v<std::vector<char>, T> || bridge_cstr_ref_trait<T>::value;

template <typename T>
concept bridge_type = bridge_custom_type<T> || bridge_data_type<T>;

/* trait vector and unordered_map<std::string, ...> type */
template <typename T>
struct is_bridge_container {
  constexpr static bool value = false;
};

template <typename T>
struct is_bridge_container<std::vector<T>> {
  constexpr static bool value = true;
};

template <typename T>
struct is_bridge_container<std::unordered_map<std::string, T>> {
  constexpr static bool value = true;
};

template <typename T>
struct is_bridge_container<std::unordered_map<std::string_view, T>> {
  constexpr static bool value = true;
};

template <typename T>
concept bridge_container_type = is_bridge_container<T>::value;

template <typename T>
struct AdaptorTrait;

template <typename T>
struct AdaptorTrait<std::vector<T>> {
  using type = T;
};

template <typename T>
requires bridge_container_type<T>
struct AdaptorTrait<std::vector<T>> {
  using type = typename AdaptorTrait<T>::type;
};

template <typename T>
struct AdaptorTrait<std::unordered_map<std::string, T>> {
  using type = T;
};

template <typename T>
requires bridge_container_type<T>
struct AdaptorTrait<std::unordered_map<std::string, T>> {
  using type = typename AdaptorTrait<T>::type;
};

template <typename T>
struct AdaptorTrait<std::unordered_map<std::string_view, T>> {
  using type = T;
};

template <typename T>
requires bridge_container_type<T>
struct AdaptorTrait<std::unordered_map<std::string_view, T>> {
  using type = typename AdaptorTrait<T>::type;
};

template <typename T>
concept bridge_adaptor_type = bridge_type<typename AdaptorTrait<T>::type>;

template <typename T>
concept bridge_inner_concept = requires(const T& inner, size_t n) {
  { inner.curAddr() } -> std::same_as<const char*>;
  {inner.skip(n)};
  { inner.outOfRange() } -> std::same_as<bool>;
};

template <typename T>
concept bridge_outer_concept = requires(T& outer, char c, std::string s, const char* ptr, size_t len) {
  {outer.push_back(c)};
  {outer.append(s)};
  {outer.append(ptr, len)};
};

};  // namespace bridge