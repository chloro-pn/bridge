#pragma once

#include <cassert>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace bridge {

class StringMap {
 public:
  StringMap() : next_id_(0), in_use_(false) {}

  uint32_t RegisterOrGetIdFromString(std::string_view str) {
    assert(in_use_ == true);
    auto it = map_.find(str);
    if (it != map_.end()) {
      return it->second;
    }
    map_[str] = next_id_;
    next_id_ += 1;
    return next_id_ - 1;
  }

  void Use() { in_use_ = true; }

  bool IfInUse() const { return in_use_ == true; }

  std::vector<char> SerializeToBridge() const {}

  static StringMap ConstructFromBridge(const std::vector<char>& bytes) {}

 private:
  std::unordered_map<std::string_view, uint32_t> map_;
  uint32_t next_id_;
  bool in_use_;
};

}  // namespace bridge