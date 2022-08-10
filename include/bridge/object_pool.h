#pragma once

#include <memory>
#include <utility>
#include <vector>

#include "bridge/variant.h"

namespace bridge {
template <typename T, size_t capacity = 10240>
struct alignas(MaxAlign<T, size_t>::value) Block {
 public:
  Block() : top_(0) {}

  bool Full() const { return top_ == capacity; }

  template <typename... Args>
  T* Alloc(Args&&... args) {
    if (Full()) {
      return nullptr;
    }
    size_t offset = top_ * sizeof(T);
    new (buf_ + offset) T(std::forward<Args>(args)...);
    top_ += 1;
    return reinterpret_cast<T*>(buf_ + offset);
  }

  ~Block() {
    for (size_t i = 0; i < top_; ++i) {
      T* obj = reinterpret_cast<T*>(buf_ + i * sizeof(T));
      obj->~T();
    }
  }

 private:
  char buf_[capacity * sizeof(T)];
  size_t top_;
};

/*
 * 对象池，使用场景：短时间内申请大量T类型对象，结束后批量释放
 */
template <typename T>
class ObjectPool {
 public:
  ObjectPool() { blocks_.reserve(4); }

  ~ObjectPool() { Clear(); }

  void Clear() { blocks_.clear(); }

  template <typename... Args>
  T* Alloc(Args&&... args) {
    if (blocks_.empty() || blocks_.back()->Full()) {
      blocks_.push_back(std::unique_ptr<Block<T>>(new Block<T>()));
    }
    return blocks_.back()->Alloc(std::forward<Args>(args)...);
  }

  static ObjectPool& Instance() {
    static ObjectPool obj;
    return obj;
  }

 private:
  std::vector<std::unique_ptr<Block<T>>> blocks_;
};

inline void object_pool_deleter(void* ptr) {}

template <typename T>
using unique_ptr = std::unique_ptr<T, void (*)(void*)>;

}  // namespace bridge