#pragma once

#include <cstdlib>
#include <memory>
#include <utility>
#include <vector>

#include "bridge/variant.h"

namespace bridge {
template <typename T>
struct Block {
 public:
  explicit Block(size_t capacity) : top_(0), capacity_(capacity), buf_(nullptr) {
    if (capacity_ > 0) {
      buf_ = static_cast<char*>(std::aligned_alloc(alignof(T), capacity_ * sizeof(T)));
    }
  }

  Block(Block&& other) : top_(other.top_), capacity_(other.capacity_), buf_(other.buf_) { other.buf_ = nullptr; }

  bool Full() const noexcept { return top_ == capacity_; }

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
    if (buf_ == nullptr) {
      return;
    }
    for (size_t i = 0; i < top_; ++i) {
      T* obj = reinterpret_cast<T*>(buf_ + i * sizeof(T));
      obj->~T();
    }
    std::free(buf_);
    buf_ = nullptr;
    top_ = 0;
    capacity_ = 0;
  }

 private:
  char* buf_;
  size_t top_;
  size_t capacity_;
};

/*
 * 对象池，使用场景：短时间内申请大量T类型对象，结束后批量释放
 * todo: 由于多线程解析器的引入，对象池的申请和释放操作需要是线程安全的，目前暂且改成thread_local，后续重构
 */
template <typename T>
class ObjectPool {
 public:
  ObjectPool() { blocks_.reserve(4); }

  ~ObjectPool() { Clear(); }

  void Clear() { blocks_.clear(); }

  template <typename... Args>
  T* Alloc(Args&&... args) {
    static size_t capacity_array[] = {2, 16, 128, 1024, 4096, 10240};
    size_t len = sizeof(capacity_array) / sizeof(size_t);
    if (blocks_.empty() || blocks_.back().Full()) {
      size_t capacity = blocks_.size() < len ? capacity_array[blocks_.size()] : capacity_array[len - 1];
      blocks_.push_back(Block<T>(capacity));
    }
    return blocks_.back().Alloc(std::forward<Args>(args)...);
  }

  static ObjectPool& Instance() {
    static thread_local ObjectPool obj;
    return obj;
  }

 private:
  std::vector<Block<T>> blocks_;
};

inline void object_pool_deleter(void* ptr) {}

template <typename T>
using unique_ptr = std::unique_ptr<T, void (*)(void*)>;

}  // namespace bridge