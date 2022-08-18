#pragma once

#include <chrono>
#include <cstdint>

#include "bridge/type_trait.h"

namespace bridge {

class Endian {
 public:
  enum class Type { Big, Little };

  static Endian& Instance() {
    static Endian obj;
    return obj;
  }

  Type GetEndianType() const { return type_; }

 private:
  Endian() {
    int i = 1;
    type_ = (*(char*)&i == 1) ? Type::Little : Type::Big;
  }

  Type type_;
};

template <typename T>
requires bridge_integral<T> || bridge_floating<T>
inline T flipByByte(T t) {
  T ret{0};
  char* ptr1 = reinterpret_cast<char*>(&ret);
  char* ptr2 = reinterpret_cast<char*>(&t);
  for (int i = 0; i < sizeof(T); ++i) {
    int j = sizeof(T) - 1 - i;
    ptr1[j] = ptr2[i];
  }
  return ret;
}

inline int GetCurrentMs() {
  auto duration = std::chrono::system_clock::now().time_since_epoch();
  auto dd = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
  return dd.count() % 1000;
}

class Timer {
 public:
  Timer() {}
  void Start() { start_ = std::chrono::system_clock::now(); }

  double End() {
    auto end = std::chrono::system_clock::now();
    auto use_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start_);
    return use_ms.count();
  }

 private:
  std::chrono::time_point<std::chrono::system_clock> start_;
};

}  // namespace bridge