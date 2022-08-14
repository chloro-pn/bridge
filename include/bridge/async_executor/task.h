#pragma once

#include <vector>

#include "async_simple/coro/Collect.h"
#include "async_simple/coro/Lazy.h"
#include "bridge/object.h"
#include "bridge/split_info.h"

namespace bridge {

using namespace async_simple::coro;

size_t GetBlockSize(const std::string& content, ObjectType type, size_t begin, const SplitInfo& si) {
  if (type == ObjectType::Data) {
    return 0;
  }
  InnerWrapper w(content);
  w.skip(begin);
  size_t offset = begin;
  parseLength(w, offset);
  uint32_t id = parseLength(w, offset);
  if (id == 0) {
    return 0;
  }
  return si.GetBlockInfoFromId(id).size();
}

struct ParseArrayResult {
  size_t offset;
  unique_ptr<Array> v;
  ParseArrayResult(size_t os, unique_ptr<Array>&& obj) : offset(os), v(std::move(obj)) {}
};

Lazy<ParseArrayResult> ParseArray(const std::string& content, const SplitInfo& si, size_t os);

Lazy<std::vector<unique_ptr<Object>>> ParseArrayBlock(const std::string& content, size_t begin, size_t end,
                                                      const SplitInfo& si) {
  std::vector<unique_ptr<Object>> ret;
  InnerWrapper w(content);
  w.skip(begin);
  size_t offset = begin;
  while (offset < end) {
    ObjectType type = parseObjectType(w, offset);
    // 这个函数是用来检测即将解析的对象是否需要并行解析，如果是Data类型
    // 或者只有一个分块信息的Map和Array类型，本地解析
    if (GetBlockSize(content, type, w.currentIndex(), si) <= 1) {
      auto v = ObjectFactory(type);
      v->valueParse(w, offset, false, nullptr);
      ret.push_back(std::move(v));
    } else {
      if (type == ObjectType::Array) {
        auto parse_result = co_await ParseArray(content, si, w.currentIndex());
        ret.push_back(std::move(parse_result.v));
        w.skipTo(parse_result.offset);
        offset = w.currentIndex();
      } else {
        assert(false); // parse map not implement from now on
      }
    }
  }
  co_return ret;
}

// @brief: 从content[os]处开始解析一个array，不包含ObjectType。
Lazy<ParseArrayResult> ParseArray(const std::string& content, const SplitInfo& si, size_t os) {
  InnerWrapper w(content);
  w.skip(os);
  size_t offset = os;
  uint64_t count = parseLength(w, offset);
  uint32_t si_id = parseLength(w, offset);
  size_t begin = w.currentIndex();
  auto block_info = si.GetBlockInfoFromId(si_id);
  std::vector<Lazy<std::vector<unique_ptr<Object>>>> tasks;
  for (auto& each : block_info) {
    size_t block_size = each;
    size_t end = begin + block_size;
    auto task = ParseArrayBlock(content, begin, end, si);
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
  co_return ParseArrayResult(begin, std::move(ret));
}

}  // namespace bridge