#pragma once

#include <cassert>
#include <cstddef>
#include <string>
#include <string_view>

namespace bridge {

class InnerWrapper {
 public:
  InnerWrapper(std::string_view str) : str_(str), current_index_(0) {}

  const char* curAddr() const {
    assert(current_index_ < str_.size());
    return &str_[current_index_];
  }

  size_t remain() const { return str_.size() - current_index_; }

  void skip(size_t k) const { current_index_ += k; }

  size_t currentIndex() const { return current_index_; }

  bool outOfRange() const { return current_index_ > str_.size(); }

 private:
  std::string_view str_;
  mutable size_t current_index_;
};

}  // namespace bridge