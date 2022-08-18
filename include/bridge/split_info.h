#pragma once

#include <cassert>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "bridge/serialize.h"

namespace bridge {

/*
 * 记录Array和Map结构的分片信息
 */
class SplitInfo {
 public:
  SplitInfo() : next_id_(1) {}

  SplitInfo(SplitInfo&&) = default;

  size_t size() const { return info_.size(); }

  uint32_t RequestId() {
    uint32_t ret = next_id_;
    next_id_ += 1;
    return ret;
  }

  SplitInfo& operator=(SplitInfo&&) = default;

  void RegisterSplitInfo(uint32_t id, std::vector<uint32_t>&& info) { info_[id] = std::move(info); }

  void RegisterSplitInfo(uint32_t id, const std::vector<uint32_t>& info) { info_[id] = info; }

  const std::vector<uint32_t>& GetBlockInfoFromId(uint32_t id) const { return info_.at(id); }

  bool Find(uint32_t id) const { return info_.find(id) != info_.end(); }

  std::string Serialize() const {
    std::string ret;
    ret.reserve(1024);
    seriLength(static_cast<uint32_t>(info_.size()), ret);
    for (const auto& each : info_) {
      seriLength(each.first, ret);
      uint32_t count = each.second.size();
      seriLength(count, ret);
      for (size_t i = 0; i < count; ++i) {
        seriLength(each.second[i], ret);
      }
    }
    return ret;
  }

  static SplitInfo Construct(std::string_view bytes) {
    SplitInfo ret;
    InnerWrapper wrapper(bytes);
    size_t offset = 0;
    uint32_t count = parseLength(wrapper, offset);
    for (uint32_t i = 0; i < count; ++i) {
      uint32_t id = parseLength(wrapper, offset);
      uint32_t blocks = parseLength(wrapper, offset);
      std::vector<uint32_t> block_info;
      for (size_t j = 0; j < blocks; ++j) {
        uint32_t tmp = parseLength(wrapper, offset);
        block_info.push_back(tmp);
      }
      assert(ret.info_.count(id) == 0);
      ret.info_.insert({id, std::move(block_info)});
      if (id >= ret.next_id_) {
        ret.next_id_ = id + 1;
      }
    }
    return ret;
  }

 private:
  std::unordered_map<uint32_t, std::vector<uint32_t>> info_;
  uint32_t next_id_;
};

}  // namespace bridge