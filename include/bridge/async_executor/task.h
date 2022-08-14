#pragma once

#include <vector>

#include "async_simple/coro/Collect.h"
#include "async_simple/coro/Lazy.h"
#include "bridge/object.h"
#include "bridge/split_info.h"

namespace bridge {

using namespace async_simple::coro;

Lazy<std::vector<unique_ptr<Object>>> ParseArrayBlock(const std::string& content, size_t begin, size_t end) {
  std::vector<unique_ptr<Object>> ret;
  InnerWrapper w(content);
  w.skip(begin);
  size_t offset = begin;
  while (offset < end) {
    ObjectType type = parseObjectType(w, offset);
    auto v = ObjectFactory(type);
    v->valueParse(w, offset, false, nullptr);
    ret.push_back(std::move(v));
  }
  co_return ret;
}

Lazy<unique_ptr<Array>> ParseArray(const std::string& content, const SplitInfo& si) {
  InnerWrapper w(content);
  size_t offset = 0;
  uint64_t count = parseLength(w, offset);
  uint32_t si_id = parseLength(w, offset);
  size_t begin = w.currentIndex();
  auto block_info = si.GetBlockInfoFromId(si_id);
  std::vector<Lazy<std::vector<unique_ptr<Object>>>> tasks;
  for (auto& each : block_info) {
    size_t block_size = each;
    size_t end = begin + block_size;
    auto task = ParseArrayBlock(content, begin, end);
    tasks.push_back(std::move(task));
    begin = end;
  }
  // 通过实现多线程Executor，使以下协程在多个线程并行调度
  // 注: 协程只需要只读信息，不需要消息交互，因此是非常适合并行的
  auto objs = co_await collectAll(std::move(tasks));
  unique_ptr<Array> ret = array();
  for (auto& eachresult : objs) {
    for (auto& eachobj : eachresult.value()) {
      ret->Insert(std::move(eachobj));
    }
  }
  co_return std::move(ret);
}

}  // namespace bridge