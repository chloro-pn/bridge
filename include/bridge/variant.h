#pragma once
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace bridge {

#define BRIDGE_VARIANT_MACRO(name, op)                                                       \
  template <typename... Ts>                                                                  \
  struct name;                                                                               \
                                                                                             \
  template <typename T>                                                                      \
  struct name<T> {                                                                           \
    constexpr static size_t value = op(T);                                                   \
  };                                                                                         \
                                                                                             \
  template <typename T, typename... Ts>                                                      \
  struct name<T, Ts...> {                                                                    \
    constexpr static size_t value = op(T) > name<Ts...>::value ? op(T) : name<Ts...>::value; \
  };

BRIDGE_VARIANT_MACRO(MaxSize, sizeof)
BRIDGE_VARIANT_MACRO(MaxAlign, alignof)

template <typename... Ts>
struct Count;

template <typename T>
struct Count<T> {
  constexpr static size_t count = 1;
};

template <typename T, typename... Ts>
struct Count<T, Ts...> {
  constexpr static size_t count = 1 + Count<Ts...>::count;
};

template <typename T, typename... Ts>
struct Index;

template <typename T, typename TheT, typename... Ts>
struct Index<T, TheT, Ts...> {
  constexpr static size_t index = std::is_same_v<T, TheT> ? 0 : 1 + Index<T, Ts...>::index;
};

template <typename T, typename LastType>
struct Index<T, LastType> {
  constexpr static size_t index = std::is_same_v<T, LastType> ? 0 : 1;
};

// 与std::variant相比不存储标识类型的flag，因此不会有因为内存对齐导致的额外开销
// 由使用者负责构造、析构
template <typename... Ts>
class alignas(MaxAlign<Ts...>::value) variant {
 private:
  template <typename T>
  struct ValidType {
    constexpr static bool value = Index<T, Ts...>::index < Count<Ts...>::count;
  };

 public:
  variant() = default;

  template <typename T, typename... Args>
  void construct(Args... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) {
    static_assert(ValidType<T>::value, "invalid type");
    new (buf_) T(std::forward<Args>(args)...);
  }

  // 由于已经丧失了存储对象的类型信息，因此使用者在拷贝/移动本对象前需要负责对象的销毁、移动和复制。本类实际上是不可移动且不可复制的。
  variant(const variant&) = delete;
  variant(variant&&) = delete;
  variant& operator=(const variant&) = delete;
  variant& operator=(variant&&) = delete;

  template <typename T>
  T& get() noexcept {
    static_assert(ValidType<T>::value, "invalid type");
    return *reinterpret_cast<T*>(buf_);
  }

  template <typename T>
  const T& get() const noexcept {
    static_assert(ValidType<T>::value, "invalid type");
    return *reinterpret_cast<T*>(buf_);
  }

  template <typename T>
  T&& move() noexcept {
    static_assert(ValidType<T>::value, "invalid type");
    return std::move(*reinterpret_cast<T*>(buf_));
  }

  template <typename T>
  void destruct() noexcept(std::is_nothrow_destructible_v<T>) {
    static_assert(ValidType<T>::value, "invalid type");
    reinterpret_cast<T*>(buf_)->~T();
  }

 private:
  char buf_[MaxSize<Ts...>::value];
};
}  // namespace bridge