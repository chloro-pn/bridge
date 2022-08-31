#pragma once

#include <cassert>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "bridge/async_executor/thread_pool.h"
#include "bridge/inner.h"
#include "bridge/parse.h"
#include "bridge/serialize.h"

namespace bridge {

/*
 * note: 因为StringMap只在序列化和反序列化内部使用，此时buf应该都是有效的，因此全部使用string_view
 */
class StringMap {
 public:
  StringMap() : next_id_(0) {}

  StringMap(StringMap&&) = default;

  StringMap& operator=(StringMap&&) = default;

  uint32_t RegisterIdFromString(std::string_view str) {
    auto it = map_.find(str);
    if (it != map_.end()) {
      return it->second;
    }
    map_[str] = next_id_;
    uint32_t ret = next_id_;
    next_id_ += 1;
    return ret;
  }

  std::string_view GetStr(uint32_t id) const {
    if (re_map_ == nullptr) {
      const_cast<StringMap*>(this)->buildReMap();
    }
    if (re_map_->count(id) == 0) {
      throw std::runtime_error("string_view GetStr error");
    }
    return (*re_map_)[id];
  }

  void buildReMap() {
    re_map_.reset(new std::unordered_map<uint32_t, std::string_view>());
    for (auto& each : map_) {
      (*re_map_)[each.second] = each.first;
    }
  }

  std::string Serialize() const {
    std::string ret;
    ret.reserve(1024);
    uint32_t size = map_.size();
    seriLength(size, ret);
    for (auto& each : map_) {
      uint32_t key_length = each.first.size();
      seriLength(key_length, ret);
      ret.append(each.first);
      seriLength(each.second, ret);
    }
    return ret;
  }

  static StringMap Construct(std::string_view bytes) {
    StringMap ret;
    InnerWrapper wrapper(bytes);
    size_t offset = 0;
    uint32_t count = parseLength(wrapper, offset);
    for (uint32_t i = 0; i < count; ++i) {
      auto key_length = parseLength(wrapper, offset);
      std::string_view str(wrapper.curAddr(), key_length);
      offset += key_length;
      wrapper.skip(key_length);
      uint32_t id = parseLength(wrapper, offset);
      ret.map_.insert({str, id});
    }
    return ret;
  }

 private:
  std::unordered_map<std::string_view, uint32_t> map_;
  std::unique_ptr<std::unordered_map<uint32_t, std::string_view>> re_map_;
  uint32_t next_id_;
};

}  // namespace bridge