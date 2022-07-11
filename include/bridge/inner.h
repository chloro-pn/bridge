#pragma once

#include <cassert>
#include <cstddef>
#include <string>

namespace bridge {

class InnerWrapper {
 public:
  InnerWrapper(const std::string& str) : str_(str), current_index_(0) {}

  const char* curAddr() const {
    assert(current_index_ < str_.size());
    return &str_[current_index_];
  }

  void skip(size_t k) const { current_index_ += k; }

  bool outOfRange() const { return current_index_ > str_.size(); }

 private:
  const std::string& str_;
  mutable size_t current_index_;
};

}  // namespace bridge