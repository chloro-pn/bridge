#pragma once

#include <vector>

#include "async_simple/coro/Collect.h"
#include "async_simple/coro/Lazy.h"
#include "bridge/object.h"
#include "bridge/split_info.h"
#include "bridge/string_map.h"

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

struct ParseResult {
  size_t offset;
  unique_ptr<Object> v;
  ParseResult(size_t os, unique_ptr<Object>&& obj) : offset(os), v(std::move(obj)) {}
};

Lazy<ParseResult> ParseArray(const std::string& content, const SplitInfo& si, const StringMap* sm, size_t os,
                             bool parse_ref);

Lazy<ParseResult> ParseMap(const std::string& content, const SplitInfo& si, const StringMap* sm, size_t os,
                           bool parse_ref);

Lazy<std::vector<unique_ptr<Object>>> ParseArrayBlock(const std::string& content, size_t begin, size_t end,
                                                      const SplitInfo& si, bool parse_ref, const StringMap* sm) {
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
      v->valueParse(w, offset, parse_ref, sm);
      ret.push_back(std::move(v));
    } else {
      if (type == ObjectType::Array) {
        auto parse_result = co_await ParseArray(content, si, sm, w.currentIndex(), parse_ref);
        ret.push_back(std::move(parse_result.v));
        w.skipTo(parse_result.offset);
        offset = w.currentIndex();
      } else {
        assert(type == ObjectType::Map);
        auto parse_result = co_await ParseMap(content, si, sm, w.currentIndex(), false);
        ret.push_back(std::move(parse_result.v));
        w.skipTo(parse_result.offset);
        offset = w.currentIndex();
      }
    }
  }
  co_return ret;
}

// @brief: 从content[os]处开始解析一个array，不包含ObjectType。
Lazy<ParseResult> ParseArray(const std::string& content, const SplitInfo& si, const StringMap* sm, size_t os,
                             bool parse_ref) {
  InnerWrapper w(content);
  w.skip(os);
  size_t offset = os;
  uint64_t count = parseLength(w, offset);
  uint32_t si_id = parseLength(w, offset);
  size_t begin = w.currentIndex();
  const auto& block_info = si.GetBlockInfoFromId(si_id);
  std::vector<Lazy<std::vector<unique_ptr<Object>>>> tasks;
  for (auto& each : block_info) {
    size_t block_size = each;
    size_t end = begin + block_size;
    auto task = ParseArrayBlock(content, begin, end, si, parse_ref, sm);
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
  co_return ParseResult(begin, std::move(ret));
}

Lazy<std::vector<std::pair<std::string, unique_ptr<Object>>>> ParseMapBlock(const std::string& content, size_t begin,
                                                                            size_t end, const SplitInfo& si,
                                                                            const StringMap* sm) {
  std::vector<std::pair<std::string, unique_ptr<Object>>> ret;
  InnerWrapper w(content);
  w.skip(begin);
  size_t offset = begin;
  while (offset < end) {
    std::string key;
    if (sm == nullptr) {
      uint32_t key_length = parseLength(w, offset);
      key = std::string(w.curAddr(), key_length);
      w.skip(key_length);
      offset += key_length;
    } else {
      uint32_t id = parseLength(w, offset);
      key = sm->GetStr(id);
    }
    ObjectType type = parseObjectType(w, offset);
    if (GetBlockSize(content, type, w.currentIndex(), si) <= 1) {
      auto v = ObjectFactory(type);
      v->valueParse(w, offset, false, sm);
      ret.push_back({std::move(key), std::move(v)});
    } else {
      if (type == ObjectType::Array) {
        auto parse_result = co_await ParseArray(content, si, sm, w.currentIndex(), false);
        ret.push_back({std::move(key), std::move(parse_result.v)});
        w.skipTo(parse_result.offset);
        offset = w.currentIndex();
      } else {
        assert(type == ObjectType::Map);
        auto parse_result = co_await ParseMap(content, si, sm, w.currentIndex(), false);
        ret.push_back({std::move(key), std::move(parse_result.v)});
        w.skipTo(parse_result.offset);
        offset = w.currentIndex();
      }
    }
  }
  co_return ret;
}

Lazy<std::vector<std::pair<std::string_view, unique_ptr<Object>>>> ParseMapViewBlock(const std::string& content,
                                                                                     size_t begin, size_t end,
                                                                                     const SplitInfo& si,
                                                                                     const StringMap* sm) {
  std::vector<std::pair<std::string_view, unique_ptr<Object>>> ret;
  InnerWrapper w(content);
  w.skip(begin);
  size_t offset = begin;
  while (offset < end) {
    std::string_view key;
    if (sm == nullptr) {
      uint32_t key_length = parseLength(w, offset);
      key = std::string_view(w.curAddr(), key_length);
      w.skip(key_length);
      offset += key_length;
    } else {
      uint32_t id = parseLength(w, offset);
      key = sm->GetStr(id);
    }
    ObjectType type = parseObjectType(w, offset);
    if (GetBlockSize(content, type, w.currentIndex(), si) <= 1) {
      auto v = ObjectFactory(type);
      v->valueParse(w, offset, true, sm);
      ret.push_back({std::move(key), std::move(v)});
    } else {
      if (type == ObjectType::Array) {
        auto parse_result = co_await ParseArray(content, si, sm, w.currentIndex(), true);
        ret.push_back({std::move(key), std::move(parse_result.v)});
        w.skipTo(parse_result.offset);
        offset = w.currentIndex();
      } else {
        assert(type == ObjectType::Map);
        auto parse_result = co_await ParseMap(content, si, sm, w.currentIndex(), true);
        ret.push_back({std::move(key), std::move(parse_result.v)});
        w.skipTo(parse_result.offset);
        offset = w.currentIndex();
      }
    }
  }
  co_return ret;
}

Lazy<ParseResult> ParseMap(const std::string& content, const SplitInfo& si, const StringMap* sm, size_t os,
                           bool parse_ref) {
  InnerWrapper w(content);
  w.skip(os);
  size_t offset = os;
  uint64_t count = parseLength(w, offset);
  uint32_t split_id = parseLength(w, offset);
  const auto& block_info = si.GetBlockInfoFromId(split_id);
  size_t begin = w.currentIndex();

  if (parse_ref == false) {
    unique_ptr<Map> ret = map();
    std::vector<Lazy<std::vector<std::pair<std::string, unique_ptr<Object>>>>> tasks;
    for (auto& each : block_info) {
      size_t end = each + begin;
      auto task = ParseMapBlock(content, begin, end, si, sm);
      tasks.push_back(std::move(task));
      begin = end;
    }
    auto objs = co_await collectAll(std::move(tasks));
    for (auto& each_task : objs) {
      for (auto& each_kv : each_task.value()) {
        ret->Insert(each_kv.first, std::move(each_kv.second));
      }
    }
    co_return ParseResult(begin, std::move(ret));
  } else {
    unique_ptr<MapView> ret = map_view();
    std::vector<Lazy<std::vector<std::pair<std::string_view, unique_ptr<Object>>>>> tasks;
    for (auto& each : block_info) {
      size_t end = each + begin;
      auto task = ParseMapViewBlock(content, begin, end, si, sm);
      tasks.push_back(std::move(task));
      begin = end;
    }
    auto objs = co_await collectAll(std::move(tasks));
    for (auto& each_task : objs) {
      for (auto& each_kv : each_task.value()) {
        ret->Insert(each_kv.first, std::move(each_kv.second));
      }
    }
    co_return ParseResult(begin, std::move(ret));
  }
}

}  // namespace bridge