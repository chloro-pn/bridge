#pragma once

#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

namespace bridge {

class InnerWrapper {
 public:
  InnerWrapper(std::string_view str) : str_(str), current_index_(0) {}

  const char* curAddr() const {
    if (current_index_ >= str_.size()) {
      throw std::runtime_error("inner.curAddr() error");
    }
    return &str_[current_index_];
  }

  size_t remain() const { return str_.size() - current_index_; }

  void skip(size_t k) const { current_index_ += k; }

  void skipTo(size_t offset) { current_index_ = offset; }

  size_t currentIndex() const { return current_index_; }

  bool outOfRange() const { return current_index_ > str_.size(); }

 private:
  std::string_view str_;
  mutable size_t current_index_;
};

#define BRIDGE_CHECK_OOR(inner)                                   \
  if (inner.outOfRange()) {                                       \
    throw std::runtime_error("parse error : inner out of range"); \
  }

}  // namespace bridge