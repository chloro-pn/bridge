#pragma once

#include <string_view>
#include <tuple>
#include <vector>

#include "async_simple/coro/Collect.h"
#include "async_simple/coro/Lazy.h"
#include "bridge/object.h"
#include "bridge/split_info.h"
#include "bridge/string_map.h"
#include "bridge/util.h"

namespace bridge {

using namespace async_simple::coro;

struct ParseResult {
  size_t offset;
  unique_ptr<Object> v;
  BridgePool bp;
  ParseResult(size_t os, unique_ptr<Object>&& obj, BridgePool&& bp) noexcept : offset(os), v(std::move(obj)), bp(std::move(bp)) {}
};

template <typename ItemType>
inline BridgePool&& GetBPFromTuple(std::tuple<ItemType*, ItemType*, size_t, BridgePool>&& it) {
  return std::get<3>(std::move(it));
}

inline Lazy<ParseResult> ParseArray(std::string_view content, const SplitInfo& si, const StringMap* sm, size_t os,
                                    bool parse_ref);

inline Lazy<ParseResult> ParseMap(std::string_view content, const SplitInfo& si, const StringMap* sm, size_t os,
                                  bool parse_ref);

inline Lazy<std::tuple<ArrayItem*, ArrayItem*, size_t, BridgePool>> ParseArrayBlock(
    std::string_view content, size_t begin, size_t end, const SplitInfo& si, bool parse_ref, const StringMap* sm) {
  BridgePool bp;
  ArrayItem* head = nullptr;
  ArrayItem* tail = nullptr;
  size_t count = 0;
  InnerWrapper w(content);
  w.skip(begin);
  size_t offset = begin;
  while (offset < end) {
    ++count;
    bool need_to_split = false;
    ObjectType type = parseObjectType(w, offset, need_to_split);
    unique_ptr<Object> parsed_node(nullptr, object_pool_deleter);
    // 这个函数是用来检测即将解析的对象是否需要并行解析
    if (!need_to_split) {
      auto v = bp.object_factory(type, parse_ref);
      v->valueParse(w, offset, parse_ref, sm);
      parsed_node = std::move(v);
    } else {
      if (type == ObjectType::Array) {
        auto parse_result = co_await ParseArray(content, si, sm, w.currentIndex(), parse_ref);
        parsed_node = std::move(parse_result.v);
        w.skipTo(parse_result.offset);
        offset = w.currentIndex();
        bp.Merge(std::move(parse_result.bp));
      } else {
        assert(type == ObjectType::Map);
        auto parse_result = co_await ParseMap(content, si, sm, w.currentIndex(), parse_ref);
        parsed_node = std::move(parse_result.v);
        w.skipTo(parse_result.offset);
        offset = w.currentIndex();
        bp.Merge(std::move(parse_result.bp));
      }
    }
    if (head == nullptr) {
      head = bp.array_item();
      head->node_ = std::move(parsed_node);
      head->next_ = nullptr;
      tail = head;
    } else {
      auto tmp = bp.array_item();
      tmp->node_ = std::move(parsed_node);
      tail->next_ = tmp;
      tail = tmp;
    }
  }
  co_return std::tuple<ArrayItem*, ArrayItem*, size_t, BridgePool>(head, tail, count, std::move(bp));
}

// @brief: 从content[os]处开始解析一个array，不包含ObjectType。
inline Lazy<ParseResult> ParseArray(std::string_view content, const SplitInfo& si, const StringMap* sm, size_t os,
                                    bool parse_ref) {
  BridgePool bp;
  InnerWrapper w(content);
  w.skip(os);
  size_t offset = os;
  uint64_t count = parseLength(w, offset);
  uint32_t si_id = parseLength(w, offset);
  size_t begin = w.currentIndex();
  const auto& block_info = si.GetBlockInfoFromId(si_id);
  std::vector<Lazy<std::tuple<ArrayItem*, ArrayItem*, size_t, BridgePool>>> tasks;
  for (auto& each : block_info) {
    size_t block_size = each;
    size_t end = begin + block_size;
    auto task = ParseArrayBlock(content, begin, end, si, parse_ref, sm);
    tasks.emplace_back(std::move(task));
    begin = end;
  }
  // 通过实现多线程Executor，使以下协程在多个线程并行调度
  // 注: 协程只需要只读信息，不需要消息交互，因此是非常适合并行的
  auto objs = co_await collectAllPara(std::move(tasks));
  unique_ptr<Array> ret = bp.array();
  // 这里需要逆序合并
  for (auto it = objs.rbegin(); it != objs.rend(); ++it) {
    ret->Merge(it->value());
    bp.Merge(GetBPFromTuple(std::move(it->value())));
  }
  co_return ParseResult(begin, std::move(ret), std::move(bp));
}

inline Lazy<std::tuple<MapItem*, MapItem*, size_t, BridgePool>> ParseMapBlock(std::string_view content, size_t begin,
                                                                            size_t end, const SplitInfo& si,
                                                                            const StringMap* sm) {
  BridgePool bp;
  MapItem* head(nullptr);
  MapItem* tail(nullptr);
  size_t count = 0;
  InnerWrapper w(content);
  w.skip(begin);
  size_t offset = begin;
  while (offset < end) {
    count += 1;
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
    bool need_to_split = false;
    ObjectType type = parseObjectType(w, offset, need_to_split);
    unique_ptr<Object> parsed_object(nullptr, object_pool_deleter);
    if (!need_to_split) {
      auto v = bp.object_factory(type, false);
      v->valueParse(w, offset, false, sm);
      parsed_object = std::move(v);
    } else {
      if (type == ObjectType::Array) {
        auto parse_result = co_await ParseArray(content, si, sm, w.currentIndex(), false);
        parsed_object = std::move(parse_result.v);
        w.skipTo(parse_result.offset);
        offset = w.currentIndex();
        bp.Merge(std::move(parse_result.bp));
      } else {
        assert(type == ObjectType::Map);
        auto parse_result = co_await ParseMap(content, si, sm, w.currentIndex(), false);
        parsed_object = std::move(parse_result.v);
        w.skipTo(parse_result.offset);
        offset = w.currentIndex();
        bp.Merge(std::move(parse_result.bp));
      }
    }
    if (head == nullptr) {
      head = bp.map_item();
      head->key_ = key;
      head->value_ = std::move(parsed_object);
      head->next_ = nullptr;
      tail = head;
    } else {
      auto tmp = bp.map_item();
      tmp->key_ = key;
      tmp->value_ = std::move(parsed_object);
      tail->next_ = tmp;
      tail = tmp;
    }
  }
  co_return std::tuple<MapItem*, MapItem*, size_t, BridgePool>(head, tail, count, std::move(bp));
}

inline Lazy<std::tuple<MapViewItem*, MapViewItem*, size_t, BridgePool>> ParseMapViewBlock(
    std::string_view content, size_t begin, size_t end, const SplitInfo& si, const StringMap* sm) {
  BridgePool bp;
  MapViewItem* head = nullptr;
  MapViewItem* tail = nullptr;
  size_t count = 0;
  InnerWrapper w(content);
  w.skip(begin);
  size_t offset = begin;
  while (offset < end) {
    count += 1;
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
    bool need_to_split = false;
    ObjectType type = parseObjectType(w, offset, need_to_split);
    unique_ptr<Object> parsed_object(nullptr, object_pool_deleter);
    if (!need_to_split) {
      auto v = bp.object_factory(type, true);
      v->valueParse(w, offset, true, sm);
      parsed_object = std::move(v);
    } else {
      if (type == ObjectType::Array) {
        auto parse_result = co_await ParseArray(content, si, sm, w.currentIndex(), true);
        parsed_object = std::move(parse_result.v);
        w.skipTo(parse_result.offset);
        offset = w.currentIndex();
        bp.Merge(std::move(parse_result.bp));
      } else {
        assert(type == ObjectType::Map);
        auto parse_result = co_await ParseMap(content, si, sm, w.currentIndex(), true);
        parsed_object = std::move(parse_result.v);
        w.skipTo(parse_result.offset);
        offset = w.currentIndex();
        bp.Merge(std::move(parse_result.bp));
      }
    }
    if (head == nullptr) {
      head = bp.map_view_item();
      head->key_ = key;
      head->value_ = std::move(parsed_object);
      head->next_ = nullptr;
      tail = head;
    } else {
      auto tmp = bp.map_view_item();
      tmp->key_ = key;
      tmp->value_ = std::move(parsed_object);
      tail->next_ = tmp;
      tail = tmp;
    }
  }
  co_return std::tuple<MapViewItem*, MapViewItem*, size_t, BridgePool>(head, tail, count, std::move(bp));
}

inline Lazy<ParseResult> ParseMap(std::string_view content, const SplitInfo& si, const StringMap* sm, size_t os,
                                  bool parse_ref) {
  BridgePool bp;
  InnerWrapper w(content);
  w.skip(os);
  size_t offset = os;
  uint64_t count = parseLength(w, offset);
  uint32_t split_id = parseLength(w, offset);
  auto block_info = si.GetBlockInfoFromId(split_id);
  size_t begin = w.currentIndex();

  if (parse_ref == false) {
    unique_ptr<Map> ret = bp.map();
    std::vector<Lazy<std::tuple<MapItem*, MapItem*, size_t, BridgePool>>> tasks;
    for (auto& each : block_info) {
      size_t end = each + begin;
      auto task = ParseMapBlock(content, begin, end, si, sm);
      tasks.emplace_back(std::move(task));
      begin = end;
    }
    auto objs = co_await collectAllPara(std::move(tasks));

    for (auto& each_task : objs) {
      ret->Merge(each_task.value());
      bp.Merge(GetBPFromTuple(std::move(each_task.value())));
    }
    co_return ParseResult(begin, std::move(ret), std::move(bp));
  } else {
    unique_ptr<MapView> ret = bp.map_view();
    std::vector<Lazy<std::tuple<MapViewItem*, MapViewItem*, size_t, BridgePool>>> tasks;
    for (auto& each : block_info) {
      size_t end = each + begin;
      auto task = ParseMapViewBlock(content, begin, end, si, sm);
      tasks.emplace_back(std::move(task));
      begin = end;
    }
    auto objs = co_await collectAllPara(std::move(tasks));
    for (auto& each_task : objs) {
      ret->Merge(each_task.value());
      bp.Merge(GetBPFromTuple(std::move(each_task.value())));
    }
    co_return ParseResult(begin, std::move(ret), std::move(bp));
  }
}

}  // namespace bridge