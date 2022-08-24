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
  ParseResult(size_t os, unique_ptr<Object>&& obj) : offset(os), v(std::move(obj)) {}
};

inline Lazy<ParseResult> ParseArray(std::string_view content, const SplitInfo& si, const StringMap* sm, size_t os,
                                    bool parse_ref);

inline Lazy<ParseResult> ParseMap(std::string_view content, const SplitInfo& si, const StringMap* sm, size_t os,
                                  bool parse_ref);

inline Lazy<std::tuple<Array::ArrayItem*, Array::ArrayItem*, size_t>> ParseArrayBlock(
    std::string_view content, size_t begin, size_t end, const SplitInfo& si, bool parse_ref, const StringMap* sm) {
  Array::ArrayItem* head = nullptr;
  Array::ArrayItem* tail = nullptr;
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
      auto v = ObjectFactory(type, parse_ref);
      v->valueParse(w, offset, parse_ref, sm);
      parsed_node = std::move(v);
    } else {
      if (type == ObjectType::Array) {
        auto parse_result = co_await ParseArray(content, si, sm, w.currentIndex(), parse_ref);
        parsed_node = std::move(parse_result.v);
        w.skipTo(parse_result.offset);
        offset = w.currentIndex();
      } else {
        assert(type == ObjectType::Map);
        auto parse_result = co_await ParseMap(content, si, sm, w.currentIndex(), parse_ref);
        parsed_node = std::move(parse_result.v);
        w.skipTo(parse_result.offset);
        offset = w.currentIndex();
      }
    }
    if (head == nullptr) {
      head = Array::ArrayItem::GetFromObjectPool();
      head->node_ = std::move(parsed_node);
      head->next_ = nullptr;
      tail = head;
    } else {
      auto tmp = Array::ArrayItem::GetFromObjectPool();
      tmp->node_ = std::move(parsed_node);
      tail->next_ = tmp;
      tail = tmp;
    }
  }
  co_return std::tuple<Array::ArrayItem*, Array::ArrayItem*, size_t>(head, tail, count);
}

// @brief: 从content[os]处开始解析一个array，不包含ObjectType。
inline Lazy<ParseResult> ParseArray(std::string_view content, const SplitInfo& si, const StringMap* sm, size_t os,
                                    bool parse_ref) {
  InnerWrapper w(content);
  w.skip(os);
  size_t offset = os;
  uint64_t count = parseLength(w, offset);
  uint32_t si_id = parseLength(w, offset);
  size_t begin = w.currentIndex();
  const auto& block_info = si.GetBlockInfoFromId(si_id);
  std::vector<Lazy<std::tuple<Array::ArrayItem*, Array::ArrayItem*, size_t>>> tasks;
  for (auto& each : block_info) {
    size_t block_size = each;
    size_t end = begin + block_size;
    auto task = ParseArrayBlock(content, begin, end, si, parse_ref, sm);
    tasks.push_back(std::move(task));
    begin = end;
  }
  // 通过实现多线程Executor，使以下协程在多个线程并行调度
  // 注: 协程只需要只读信息，不需要消息交互，因此是非常适合并行的
  auto objs = co_await collectAllPara(std::move(tasks));
  unique_ptr<Array> ret = array();
  // 这里需要逆序合并
  for (auto it = objs.rbegin(); it != objs.rend(); ++it) {
    ret->Merge(it->value());
  }
  co_return ParseResult(begin, std::move(ret));
}

inline Lazy<std::tuple<Map::MapItem*, Map::MapItem*, size_t>> ParseMapBlock(std::string_view content, size_t begin,
                                                                            size_t end, const SplitInfo& si,
                                                                            const StringMap* sm) {
  Map::MapItem* head(nullptr);
  Map::MapItem* tail(nullptr);
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
      auto v = ObjectFactory(type, false);
      v->valueParse(w, offset, false, sm);
      parsed_object = std::move(v);
    } else {
      if (type == ObjectType::Array) {
        auto parse_result = co_await ParseArray(content, si, sm, w.currentIndex(), false);
        parsed_object = std::move(parse_result.v);
        w.skipTo(parse_result.offset);
        offset = w.currentIndex();
      } else {
        assert(type == ObjectType::Map);
        auto parse_result = co_await ParseMap(content, si, sm, w.currentIndex(), false);
        parsed_object = std::move(parse_result.v);
        w.skipTo(parse_result.offset);
        offset = w.currentIndex();
      }
    }
    if (head == nullptr) {
      head = Map::MapItem::GetFromObjectPool();
      head->key_ = key;
      head->value_ = std::move(parsed_object);
      head->next_ = nullptr;
      tail = head;
    } else {
      auto tmp = Map::MapItem::GetFromObjectPool();
      tmp->key_ = key;
      tmp->value_ = std::move(parsed_object);
      tail->next_ = tmp;
      tail = tmp;
    }
  }
  co_return std::tuple<Map::MapItem*, Map::MapItem*, size_t>(head, tail, count);
}

inline Lazy<std::tuple<MapView::MapViewItem*, MapView::MapViewItem*, size_t>> ParseMapViewBlock(
    std::string_view content, size_t begin, size_t end, const SplitInfo& si, const StringMap* sm) {
  MapView::MapViewItem* head = nullptr;
  MapView::MapViewItem* tail = nullptr;
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
      auto v = ObjectFactory(type, true);
      v->valueParse(w, offset, true, sm);
      parsed_object = std::move(v);
    } else {
      if (type == ObjectType::Array) {
        auto parse_result = co_await ParseArray(content, si, sm, w.currentIndex(), true);
        parsed_object = std::move(parse_result.v);
        w.skipTo(parse_result.offset);
        offset = w.currentIndex();
      } else {
        assert(type == ObjectType::Map);
        auto parse_result = co_await ParseMap(content, si, sm, w.currentIndex(), true);
        parsed_object = std::move(parse_result.v);
        w.skipTo(parse_result.offset);
        offset = w.currentIndex();
      }
    }
    if (head == nullptr) {
      head = MapView::MapViewItem::GetFromObjectPool();
      head->key_ = key;
      head->value_ = std::move(parsed_object);
      head->next_ = nullptr;
      tail = head;
    } else {
      auto tmp = MapView::MapViewItem::GetFromObjectPool();
      tmp->key_ = key;
      tmp->value_ = std::move(parsed_object);
      tail->next_ = tmp;
      tail = tmp;
    }
  }
  co_return std::tuple<MapView::MapViewItem*, MapView::MapViewItem*, size_t>(head, tail, count);
}

inline Lazy<ParseResult> ParseMap(std::string_view content, const SplitInfo& si, const StringMap* sm, size_t os,
                                  bool parse_ref) {
  InnerWrapper w(content);
  w.skip(os);
  size_t offset = os;
  uint64_t count = parseLength(w, offset);
  uint32_t split_id = parseLength(w, offset);
  auto block_info = si.GetBlockInfoFromId(split_id);
  size_t begin = w.currentIndex();

  if (parse_ref == false) {
    unique_ptr<Map> ret = map();
    std::vector<Lazy<std::tuple<Map::MapItem*, Map::MapItem*, size_t>>> tasks;
    for (auto& each : block_info) {
      size_t end = each + begin;
      auto task = ParseMapBlock(content, begin, end, si, sm);
      tasks.push_back(std::move(task));
      begin = end;
    }
    auto objs = co_await collectAllPara(std::move(tasks));

    for (auto& each_task : objs) {
      ret->Merge(each_task.value());
    }
    co_return ParseResult(begin, std::move(ret));
  } else {
    unique_ptr<MapView> ret = map_view();
    std::vector<Lazy<std::tuple<MapView::MapViewItem*, MapView::MapViewItem*, size_t>>> tasks;
    for (auto& each : block_info) {
      size_t end = each + begin;
      auto task = ParseMapViewBlock(content, begin, end, si, sm);
      tasks.push_back(std::move(task));
      begin = end;
    }
    auto objs = co_await collectAllPara(std::move(tasks));
    for (auto& each_task : objs) {
      ret->Merge(each_task.value());
    }
    co_return ParseResult(begin, std::move(ret));
  }
}

}  // namespace bridge