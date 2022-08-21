#pragma once

#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "bridge/data_type.h"
#include "bridge/inner.h"
#include "bridge/object_pool.h"
#include "bridge/object_type.h"
#include "bridge/parse.h"
#include "bridge/serialize.h"
#include "bridge/split_info.h"
#include "bridge/string_map.h"
#include "bridge/type_trait.h"
#include "bridge/variant.h"

namespace bridge {

constexpr size_t split_block_size = 1024 * 1024 * 10;

class Object {
 public:
  Object(ObjectType type, bool ref_type) : type_(type), ref_type_(ref_type) {}

  ObjectType GetType() const { return type_; }

  bool IsRefType() const { return ref_type_; }

  virtual ~Object() = default;

  // type dispatch functions:

  template <typename Inner>
  requires bridge_inner_concept<Inner>
  void valueParse(const Inner& inner, size_t& offset, bool parse_ref, const StringMap* map);

  template <typename Outer>
  requires bridge_outer_concept<Outer>
  void valueSeri(Outer& outer, StringMap* map, SplitInfo* si, bool& need_to_split) const;

  void dump(std::string& buf, int level) const;

 private:
  ObjectType type_;
  bool ref_type_;
};

unique_ptr<Object> ObjectFactory(ObjectType type, bool parse_ref = false);

class Data : public Object {
 public:
  // 默认构造函数，Data处于不合法状态
  Data() : Object(ObjectType::Data, false), data_type_(BRIDGE_INVALID) {}

  template <typename T>
  explicit Data(T&& obj) : Object(ObjectType::Data, false) {
    data_type_ = DataTypeTrait<T>::dt;
    assert(data_type_ != BRIDGE_CUSTOM && data_type_ != BRIDGE_INVALID);
    serialize(std::forward<T>(obj), variant_);
  }

  template <bridge_custom_type T>
  explicit Data(T&& obj) : Object(ObjectType::Data, false) {
    data_type_ = DataTypeTrait<T>::dt;
    assert(data_type_ == BRIDGE_CUSTOM);
    serialize(obj.SerializeToBridge(), variant_);
  }

  template <typename T>
  Data& operator=(T&& obj) {
    destruct_by_datatype();
    data_type_ = DataTypeTrait<T>::dt;
    serialize(std::forward<T>(obj), variant_);
    return *this;
  }

  template <bridge_custom_type T>
  Data& operator=(T&& obj) {
    destruct_by_datatype();
    data_type_ = BRIDGE_CUSTOM;
    serialize(obj.SerializeToBridge(), variant_);
    return *this;
  }

  template <typename T>
  requires bridge_data_type<T> std::optional<T> Get()
  const {
    static_assert(NoRefNoPointer<T>::value,
                  "get() method should not return ref or pointer type (including <T* const> and <const T*>)");
    // 当处于不合法状态时，总是返回空的optional
    if (DataTypeTrait<T>::dt != data_type_) {
      return std::optional<T>();
    }
    return std::optional(parse<T>(variant_));
  }

  template <bridge_data_type T>
  const T* GetPtr() const {
    static_assert(NoRefNoPointer<T>::value);
    if (DataTypeTrait<T>::dt != data_type_) {
      return nullptr;
    }
    return &parse<T>(variant_);
  }

  template <bridge_custom_type T>
  std::optional<T> Get() const {
    static_assert(NoRefNoPointer<T>::value,
                  "get() method should not return ref or pointer type (including <T* const> and <const T*>)");
    // 当处于不合法状态时，总是返回空的optional
    if (DataTypeTrait<T>::dt != data_type_) {
      return std::optional<T>();
    }
    return std::optional(T::ConstructFromBridge(variant_.get<std::vector<char>>()));
  }

  uint8_t GetDataType() const { return data_type_; }

  template <typename Inner>
  requires bridge_inner_concept<Inner>
  void valueParse(const Inner& inner, size_t& offset, bool parse_ref, const StringMap* map) {
    data_type_ = parseDataType(inner, offset);
    if (inner.outOfRange()) {
      return;
    }
    assert(data_type_ != BRIDGE_INVALID);
    if (data_type_ == BRIDGE_STRING && map != nullptr) {
      uint32_t id = parseLength(inner, offset);
      if (inner.outOfRange()) {
        return;
      }
      std::string_view str = map->GetStr(id);
      variant_.construct<std::string>(str);
      return;
    }
    uint64_t size = parseLength(inner, offset);
    if (inner.outOfRange()) {
      return;
    }
    parseData(data_type_, inner.curAddr(), size, variant_);
    inner.skip(size);
    if (inner.outOfRange()) {
      return;
    }
    offset += size;
  }

  template <typename Outer>
  requires bridge_outer_concept<Outer>
  void valueSeri(Outer& outer, StringMap* map, SplitInfo* si, bool& need_to_split) const {
    need_to_split = false;
    assert(data_type_ != BRIDGE_INVALID);
    seriDataType(data_type_, outer);
    if (data_type_ == BRIDGE_STRING && map != nullptr) {
      std::string_view view(variant_.get<std::string>());
      uint32_t id = map->RegisterIdFromString(view);
      seriLength(id, outer);
      return;
    }
    seriData(data_type_, variant_, outer);
  }

  void dump(std::string& buf, int level) const {
    std::string prefix(level, ' ');
    buf.append(prefix + ObjectTypeToStr(GetType()));
    buf.append("[ ");
    buf.append(DataTypeToStr(data_type_));
    buf.append(" ]");
  }

 private:
  bridge_variant variant_;
  uint8_t data_type_;

  void destruct_by_datatype() {
    if (data_type_ == BRIDGE_STRING) {
      variant_.destruct<std::string>();
    } else if (data_type_ == BRIDGE_CUSTOM || data_type_ == BRIDGE_BYTES) {
      variant_.destruct<std::vector<char>>();
    }
  }
};

class DataView : public Object {
 public:
  DataView() : Object(ObjectType::Data, true), data_type_(BRIDGE_INVALID) {}

  template <bridge_data_type T>
  explicit DataView(const T& obj) : Object(ObjectType::Data, true) {
    data_type_ = DataTypeTrait<T>::dt;
    assert(data_type_ != BRIDGE_CUSTOM && data_type_ != BRIDGE_INVALID);
    serialize(obj, variant_);
  }

  explicit DataView(const std::string_view& obj) : Object(ObjectType::Data, true) {
    data_type_ = BRIDGE_STRING;
    serialize(obj, variant_);
  }

  template <bridge_data_type T>
  DataView& operator=(const T& obj) {
    // data view 不持有资源，因此不需要析构
    // destruct_by_datatype();
    data_type_ = DataTypeTrait<T>::dt;
    assert(data_type_ != BRIDGE_CUSTOM && data_type_ != BRIDGE_INVALID);
    serialize(obj, variant_);
    return *this;
  }

  DataView& operator=(const std::string_view& view) {
    data_type_ = BRIDGE_STRING;
    serialize(view, variant_);
    return *this;
  }

  uint8_t GetDataType() const { return data_type_; }

  // 不支持Get<std::string>() 和 Get<std::vector<char>>()
  // 请使用Get<std::string_view>()
  template <typename T>
  requires bridge_integral<T> || bridge_floating<T> std::optional<T> Get()
  const {
    // 当处于不合法状态时，总是返回空的optional
    if (DataTypeTrait<T>::dt != data_type_) {
      return std::optional<T>();
    }
    return std::optional(variant_.get<T>());
  }

  template <typename T>
  requires std::same_as<T, std::string_view> std::optional<T> Get()
  const { return GetStrView(); }

  std::optional<std::string_view> GetStrView() const {
    if (!(data_type_ == BRIDGE_STRING || data_type_ == BRIDGE_BYTES)) {
      return std::optional<std::string_view>();
    }
    return std::optional(variant_.get<std::string_view>());
  }

  template <typename Inner>
  requires bridge_inner_concept<Inner>
  void valueParse(const Inner& inner, size_t& offset, bool parse_ref, const StringMap* map) {
    data_type_ = parseDataType(inner, offset);
    if (inner.outOfRange()) {
      return;
    }
    assert(data_type_ != BRIDGE_INVALID);
    if (data_type_ == BRIDGE_STRING && map != nullptr) {
      uint32_t id = parseLength(inner, offset);
      if (inner.outOfRange()) {
        return;
      }
      variant_.construct<std::string_view>(map->GetStr(id));
      return;
    }
    uint64_t size = parseLength(inner, offset);
    if (inner.outOfRange()) {
      return;
    }
    parseData(data_type_, inner.curAddr(), size, variant_);
    inner.skip(size);
    if (inner.outOfRange()) {
      return;
    }
    offset += size;
  }

  template <typename Outer>
  requires bridge_outer_concept<Outer>
  void valueSeri(Outer& outer, StringMap* map, SplitInfo* si, bool& need_to_split) const {
    need_to_split = false;
    assert(data_type_ != BRIDGE_INVALID);
    seriDataType(data_type_, outer);
    if (data_type_ == BRIDGE_STRING && map != nullptr) {
      uint32_t id = map->RegisterIdFromString(variant_.get<std::string_view>());
      seriLength(id, outer);
      return;
    }
    seriData(data_type_, variant_, outer);
  }

  void dump(std::string& buf, int level) const {
    std::string prefix(level, ' ');
    buf.append(prefix + ObjectTypeToStr(GetType()));
    buf.append("[ ");
    buf.append(DataTypeToStr(data_type_));
    buf.append(" ]");
  }

 private:
  bridge_view_variant variant_;
  uint8_t data_type_;
};

class Array : public Object {
 public:
  Array() : Object(ObjectType::Array, false) {}

  Array(std::vector<unique_ptr<Object>>&& values) : Object(ObjectType::Array, false), objects_(std::move(values)) {}

  void Insert(unique_ptr<Object>&& value) { objects_.push_back(std::move(value)); }

  template <typename It>
  void Insert(It begin, It end) {
    objects_.insert(objects_.end(), begin, end);
  }

  void Clear() { objects_.clear(); }

  size_t Size() const { return objects_.size(); }

  const Object* operator[](size_t n) const {
    if (n >= objects_.size()) {
      return nullptr;
    }
    return objects_[n].get();
  }

  template <typename Inner>
  requires bridge_inner_concept<Inner>
  void valueParse(const Inner& inner, size_t& offset, bool parse_ref, const StringMap* map) {
    objects_.clear();
    uint64_t count = parseLength(inner, offset);
    // 对于不需要并行解析的情况，我们仅将这个id解析出来即可，没有其他开销
    uint32_t splite_info_id = parseLength(inner, offset);
    for (uint64_t i = 0; i < count; ++i) {
      ObjectType type = parseObjectType(inner, offset);
      if (inner.outOfRange()) {
        return;
      }
      auto v = ObjectFactory(type, parse_ref);
      v->valueParse(inner, offset, parse_ref, map);
      if (inner.outOfRange()) {
        return;
      }
      objects_.push_back(std::move(v));
    }
  }

  template <typename Outer>
  requires bridge_outer_concept<Outer>
  void valueSeri(Outer& outer, StringMap* map, SplitInfo* si, bool& need_to_split) const {
    uint32_t count = objects_.size();
    seriLength(count, outer);
    uint32_t split_info_id = 0;
    if (si != nullptr) {
      split_info_id = si->RequestId();
    }
    seriLength(split_info_id, outer);
    size_t current_size = outer.size();
    std::vector<uint32_t> block_size;
    for (auto& each : objects_) {
      size_t object_type_position = outer.size();
      seriObjectType(each->GetType(), outer);
      bool child_need_to_split = false;
      each->valueSeri(outer, map, si, child_need_to_split);
      if (child_need_to_split == true) {
        assert(each->GetType() != ObjectType::Data);
        outer[object_type_position] |= 0x80;
      }
      if (si != nullptr) {
        size_t tmp_size = outer.size();
        // 每当子节点序列化大小超过split_block_size字节，则生成一个分片
        if (tmp_size - current_size >= split_block_size) {
          block_size.push_back(tmp_size - current_size);
          current_size = tmp_size;
        }
      }
    }
    // 最后一个分片，可能是0，是0的话忽略
    if (si != nullptr && outer.size() - current_size > 0) {
      block_size.push_back(outer.size() - current_size);
    }
    if (si != nullptr) {
      // 如果分块大于1个，则需要注册分块信息
      // 如果分块等于1个，检查分块是否大于临界值，如果大于，则注册分块信息，否则不注册分块信息。
      // 如果分块为0个（空数组）,不注册分块信息
      if (block_size.empty() == false && (block_size.size() >= 2 || block_size.back() >= split_block_size)) {
        si->RegisterSplitInfo(split_info_id, std::move(block_size));
        need_to_split = true;
      }
      // 如果不注册的话，通过split_info_id找不到对应的block_info_信息，本地解析
    }
  }

  void dump(std::string& buf, int level) const {
    std::string prefix(level, ' ');
    buf.append(prefix + ObjectTypeToStr(GetType()) + "[ " + std::to_string(Size()) + " ]");
    for (auto& each : objects_) {
      buf.push_back('\n');
      each->dump(buf, level + 1);
    }
  }

 private:
  std::vector<unique_ptr<Object>> objects_;
};

class Map : public Object {
 public:
  Map() : Object(ObjectType::Map, false) {}

  void Insert(const std::string& key, unique_ptr<Object>&& value) {
#ifdef BRIDGE_USE_UNORDERED_MAP
    objects_.insert({key, std::move(value)});
#else
    objects_.emplace_back(std::pair<std::string, unique_ptr<Object>>(key, std::move(value)));
#endif
  }

  const Object* operator[](const std::string& key) const {
#ifdef BRIDGE_USE_UNORDERED_MAP
    auto it = objects_.find(key);
    if (it == objects_.end()) {
      return nullptr;
    }
    return it->second.get();
#else
    for(auto& each : objects_) {
      if (each.first == key) {
        return each.second.get();
      }
    }
    return nullptr;
#endif
  }

  unique_ptr<Object> Get(const std::string& key) {
#ifdef BRIDGE_USE_UNORDERED_MAP
    auto it = objects_.find(key);
    if (it == objects_.end()) {
      return unique_ptr<Object>(nullptr, object_pool_deleter);
    }
    return std::move(it->second);
#else
    for(auto& each : objects_) {
      if (each.first == key) {
        return std::move(each.second);
      }
    }
    return unique_ptr<Object>(nullptr, object_pool_deleter);
#endif
  }

  size_t Size() const { return objects_.size(); }

  void Clear() { objects_.clear(); }

  auto Begin() const { return objects_.begin(); }

  auto End() const { return objects_.end(); }

  template <typename Inner>
  requires bridge_inner_concept<Inner>
  void valueParse(const Inner& inner, size_t& offset, bool parse_ref, const StringMap* map) {
    objects_.clear();
    uint64_t count = parseLength(inner, offset);
    // 对于不需要并行解析的情况，我们仅将这个id解析出来即可，没有其他开销
    uint32_t splite_info_id = parseLength(inner, offset);
    if (inner.outOfRange()) {
      return;
    }
    for (uint64_t i = 0; i < count; ++i) {
      // parse key
      std::string_view key_view;
      if (map == nullptr) {
        uint64_t key_length = parseLength(inner, offset);
        if (inner.outOfRange()) {
          return;
        }
        key_view = std::string_view(inner.curAddr(), key_length);
        inner.skip(key_length);
        if (inner.outOfRange()) {
          return;
        }
        offset += key_length;
      } else {
        uint32_t id = parseLength(inner, offset);
        if (inner.outOfRange()) {
          return;
        }
        key_view = map->GetStr(id);
      }
      // parse value
      ObjectType type = parseObjectType(inner, offset);
      if (inner.outOfRange()) {
        return;
      }
      auto v = ObjectFactory(type, parse_ref);
      v->valueParse(inner, offset, parse_ref, map);
      if (inner.outOfRange()) {
        return;
      }
      Insert(std::string(key_view), std::move(v));
    }
  }

  template <typename Outer>
  requires bridge_outer_concept<Outer>
  void valueSeri(Outer& outer, StringMap* map, SplitInfo* si, bool& need_to_split) const {
    uint32_t count = objects_.size();
    seriLength(count, outer);
    uint32_t split_info_id = 0;
    if (si != nullptr) {
      split_info_id = si->RequestId();
    }
    seriLength(split_info_id, outer);
    size_t current_size = outer.size();
    std::vector<uint32_t> block_size;
    for (auto& each : objects_) {
      // key seri
      if (map == nullptr) {
        uint32_t key_length = each.first.size();
        seriLength(key_length, outer);
        outer.append(each.first);
      } else {
        uint32_t id = map->RegisterIdFromString(each.first);
        seriLength(id, outer);
      }
      size_t object_type_position = outer.size();
      seriObjectType(each.second->GetType(), outer);
      bool child_need_to_split = false;
      each.second->valueSeri(outer, map, si, child_need_to_split);
      if (child_need_to_split == true) {
        outer[object_type_position] |= 0x80;
        assert(each.second->GetType() != ObjectType::Data);
      }
      if (si != nullptr) {
        size_t tmp_size = outer.size();
        // 每当子节点序列化大小超过split_block_size字节，则生成一个分片
        if (tmp_size - current_size >= split_block_size) {
          block_size.push_back(tmp_size - current_size);
          current_size = tmp_size;
        }
      }
    }
    // 最后一个分片，可能是0，是0的话忽略
    if (si != nullptr && outer.size() - current_size > 0) {
      block_size.push_back(outer.size() - current_size);
    }
    if (si != nullptr) {
      // 如果分块大于1个，则需要注册分块信息
      // 如果分块等于1个，检查分块是否大于临界值，如果大于，则注册分块信息，否则不注册分块信息。
      // 如果分块为0个（空数组）,不注册分块信息
      if (block_size.empty() == false && (block_size.size() >= 2 || block_size.back() >= split_block_size)) {
        si->RegisterSplitInfo(split_info_id, std::move(block_size));
        need_to_split = true;
      }
      // 如果不注册的话，通过split_info_id找不到对应的block_info_信息，本地解析
    }
  }

  void dump(std::string& buf, int level) const {
    std::string prefix(level, ' ');
    buf.append(prefix + ObjectTypeToStr(GetType()) + "[ " + std::to_string(Size()) + " ]");
    std::string key_prefix(level + 1, ' ');
    for (auto& each : objects_) {
      buf.push_back('\n');
      buf.append(key_prefix + "< " + each.first + " > : ");
      buf.push_back('\n');
      each.second->dump(buf, level + 2);
    }
  }

#ifdef BRIDGE_USE_UNORDERED_MAP
  using iter_type = typename std::unordered_map<std::string, unique_ptr<Object>>::const_iterator;
#else
  using iter_type = typename std::vector<std::pair<std::string, unique_ptr<Object>>>::const_iterator;
#endif

 private:
#ifdef BRIDGE_USE_UNORDERED_MAP
  std::unordered_map<std::string, unique_ptr<Object>> objects_;
#else
  std::vector<std::pair<std::string, unique_ptr<Object>>> objects_;
#endif
};

class MapView : public Object {
 public:
  MapView() : Object(ObjectType::Map, true) {}
  
  const Object* operator[](const std::string& key) const {
#ifdef BRIDGE_USE_UNORDERED_MAP
    auto it = objects_.find(key);
    if (it == objects_.end()) {
      return nullptr;
    }
    return it->second.get();
#else
   for(auto& each : objects_) {
    if (each.first == key) {
      return each.second.get();
    }
   }
   return nullptr;
#endif
  }

  template <typename Inner>
  requires bridge_inner_concept<Inner>
  void valueParse(const Inner& inner, size_t& offset, bool parse_ref, const StringMap* map) {
    objects_.clear();
    uint64_t count = parseLength(inner, offset);
    // 对于不需要并行解析的情况，我们仅将这个id解析出来即可，没有其他开销
    uint32_t splite_info_id = parseLength(inner, offset);
    if (inner.outOfRange()) {
      return;
    }
    for (uint64_t i = 0; i < count; ++i) {
      // parse key
      std::string_view key_view;
      if (map == nullptr) {
        uint64_t key_length = parseLength(inner, offset);
        if (inner.outOfRange()) {
          return;
        }
        key_view = std::string_view(inner.curAddr(), key_length);
        inner.skip(key_length);
        if (inner.outOfRange()) {
          return;
        }
        offset += key_length;
      } else {
        uint32_t id = parseLength(inner, offset);
        if (inner.outOfRange()) {
          return;
        }
        key_view = map->GetStr(id);
      }
      // parse value
      ObjectType type = parseObjectType(inner, offset);
      if (inner.outOfRange()) {
        return;
      }
      auto v = ObjectFactory(type, parse_ref);
      v->valueParse(inner, offset, parse_ref, map);
      if (inner.outOfRange()) {
        return;
      }
      Insert(key_view, std::move(v));
    }
  }

  template <typename Outer>
  requires bridge_outer_concept<Outer>
  void valueSeri(Outer& outer, StringMap* map, SplitInfo* si, bool& need_to_split) const {
    uint32_t count = objects_.size();
    seriLength(count, outer);
    uint32_t split_info_id = 0;
    if (si != nullptr) {
      split_info_id = si->RequestId();
    }
    seriLength(split_info_id, outer);
    size_t current_size = outer.size();
    std::vector<uint32_t> block_size;
    for (auto& each : objects_) {
      // key seri
      if (map == nullptr) {
        uint32_t key_length = each.first.size();
        seriLength(key_length, outer);
        outer.append(&each.first[0], each.first.size());
      } else {
        uint32_t id = map->RegisterIdFromString(each.first);
        seriLength(id, outer);
      }
      size_t object_type_position = outer.size();
      seriObjectType(each.second->GetType(), outer);
      bool child_need_to_split = false;
      each.second->valueSeri(outer, map, si, child_need_to_split);
      if (child_need_to_split == true) {
        outer[object_type_position] |= 0x80;
        assert(each.second->GetType() != ObjectType::Data);
      }
      if (si != nullptr) {
        size_t tmp_size = outer.size();
        // 每当子节点序列化大小超过split_block_size字节，则生成一个分片
        if (tmp_size - current_size >= split_block_size) {
          block_size.push_back(tmp_size - current_size);
          current_size = tmp_size;
        }
      }
    }
    // 最后一个分片，可能是0，是0的话忽略
    if (si != nullptr && outer.size() - current_size > 0) {
      block_size.push_back(outer.size() - current_size);
    }
    if (si != nullptr) {
      // 如果分块大于1个，则需要注册分块信息
      // 如果分块等于1个，检查分块是否大于临界值，如果大于，则注册分块信息，否则不注册分块信息。
      // 如果分块为0个（空数组）,不注册分块信息
      if (block_size.empty() == false && (block_size.size() >= 2 || block_size.back() >= split_block_size)) {
        si->RegisterSplitInfo(split_info_id, std::move(block_size));
        need_to_split = true;
      }
      // 如果不注册的话，通过split_info_id找不到对应的block_info_信息，本地解析
    }
  }

  void dump(std::string& buf, int level) const {
    std::string prefix(level, ' ');
    buf.append(prefix + ObjectTypeToStr(GetType()) + "[ " + std::to_string(Size()) + " ]");
    std::string key_prefix(level + 1, ' ');
    for (auto& each : objects_) {
      buf.push_back('\n');
      buf.append(key_prefix + "< " + std::string(each.first) + " > : ");
      buf.push_back('\n');
      each.second->dump(buf, level + 2);
    }
  }

  auto Begin() const { return objects_.begin(); }

  auto End() const { return objects_.end(); }

  size_t Size() const { return objects_.size(); }

  unique_ptr<Object> Get(const std::string& key) {
#ifdef BRIDGE_USE_UNORDERED_MAP
    auto it = objects_.find(key);
    if (it == objects_.end()) {
      return unique_ptr<Object>(nullptr, object_pool_deleter);
    }
    return std::move(it->second);
#else
    for(auto& each : objects_) {
      if (each.first == key) {
        return std::move(each.second);
      }
    }
    return unique_ptr<Object>(nullptr, object_pool_deleter);
#endif
  }

#ifdef BRIDGE_USE_UNORDERED_MAP
  void Insert(std::string_view key_view, unique_ptr<Object>&& value) { objects_.insert({key_view, std::move(value)}); }
#else
  void Insert(std::string_view key_view, unique_ptr<Object>&& value) {
    objects_.emplace_back(std::pair<std::string_view, unique_ptr<Object>>(std::move(key_view), std::move(value)));
  }
#endif

#ifdef BRIDGE_USE_UNORDERED_MAP
  using iter_type = typename std::unordered_map<std::string_view, unique_ptr<Object>>::const_iterator;
#else
  using iter_type = typename std::vector<std::pair<std::string_view, unique_ptr<Object>>>::const_iterator;
#endif

 private:
#ifdef BRIDGE_USE_UNORDERED_MAP
  std::unordered_map<std::string_view, unique_ptr<Object>> objects_;
#else
  std::vector<std::pair<std::string_view, unique_ptr<Object>>> objects_;
#endif
};

#define BRIDGE_SPACE_PLAC

#define BRIDGE_DISPATCH(func_name, spec, ...)                    \
  if (type_ == ObjectType::Map) {                                \
    if (IsRefType() == false) {                                  \
      static_cast<spec Map*>(this)->func_name(__VA_ARGS__);      \
    } else {                                                     \
      static_cast<spec MapView*>(this)->func_name(__VA_ARGS__);  \
    }                                                            \
  } else if (type_ == ObjectType::Array) {                       \
    static_cast<spec Array*>(this)->func_name(__VA_ARGS__);      \
  } else if (type_ == ObjectType::Data) {                        \
    if (IsRefType() == false) {                                  \
      static_cast<spec Data*>(this)->func_name(__VA_ARGS__);     \
    } else {                                                     \
      static_cast<spec DataView*>(this)->func_name(__VA_ARGS__); \
    }                                                            \
  }

template <typename Inner>
requires bridge_inner_concept<Inner>
void Object::valueParse(const Inner& inner, size_t& offset, bool parse_ref, const StringMap* map) {
  BRIDGE_DISPATCH(valueParse, BRIDGE_SPACE_PLAC, inner, offset, parse_ref, map)
}

template <typename Outer>
requires bridge_outer_concept<Outer>
void Object::valueSeri(Outer& outer, StringMap* map, SplitInfo* si, bool& need_to_split) const {
  BRIDGE_DISPATCH(valueSeri, const, outer, map, si, need_to_split)
}

inline void Object::dump(std::string& buf, int level) const { BRIDGE_DISPATCH(dump, const, buf, level) }

template <typename T, typename... Args>
inline unique_ptr<T> ValueFactory(Args&&... args) {
  return unique_ptr<T>(ObjectPool<T>::Instance().Alloc(std::forward<Args>(args)...), object_pool_deleter);
}

template <typename... Args>
inline unique_ptr<Data> data(Args&&... args) {
  return unique_ptr<Data>(ObjectPool<Data>::Instance().Alloc(std::forward<Args>(args)...), object_pool_deleter);
}

template <typename... Args>
inline unique_ptr<DataView> data_view(Args&&... args) {
  return unique_ptr<DataView>(ObjectPool<DataView>::Instance().Alloc(std::forward<Args>(args)...), object_pool_deleter);
}

template <typename... Args>
inline unique_ptr<Array> array(Args&&... args) {
  return unique_ptr<Array>(ObjectPool<Array>::Instance().Alloc(std::forward<Args>(args)...), object_pool_deleter);
}

template <typename... Args>
inline unique_ptr<Map> map(Args&&... args) {
  return unique_ptr<Map>(ObjectPool<Map>::Instance().Alloc(std::forward<Args>(args)...), object_pool_deleter);
}

template <typename... Args>
inline unique_ptr<MapView> map_view(Args&&... args) {
  return unique_ptr<MapView>(ObjectPool<MapView>::Instance().Alloc(std::forward<Args>(args)...), object_pool_deleter);
}

inline unique_ptr<Object> ObjectFactory(ObjectType type, bool parse_ref) {
  if (type == ObjectType::Data) {
    if (parse_ref == false) {
      return ValueFactory<Data>();
    } else {
      return ValueFactory<DataView>();
    }
  } else if (type == ObjectType::Map) {
    if (parse_ref == false) {
      return ValueFactory<Map>();
    } else {
      return ValueFactory<MapView>();
    }
  } else if (type == ObjectType::Array) {
    return ValueFactory<Array>();
  } else {
    return unique_ptr<Object>(nullptr, object_pool_deleter);
  }
}

class ObjectWrapper;

class ObjectWrapperIterator {
 public:
  using holder_type = Map::iter_type;
  using holder_type_view = MapView::iter_type;

  ObjectWrapperIterator(holder_type iter, holder_type end) : iter_(iter), end_(end), view_(false) {}

  ObjectWrapperIterator(holder_type_view iter, holder_type_view end) : iter_view_(iter), end_view_(end), view_(true) {}

  bool Valid() const { return view_ == false ? iter_ != end_ : iter_view_ != end_view_; }

  ObjectWrapperIterator& operator++() {
    if (view_ == false) {
      ++iter_;
    } else {
      ++iter_view_;
    }
    return *this;
  }

  std::string_view GetKey() const { return view_ == false ? iter_->first : iter_view_->first; }

  ObjectWrapper GetValue() const;

 private:
  holder_type iter_;
  holder_type end_;
  holder_type_view iter_view_;
  holder_type_view end_view_;
  bool view_;
};

template <typename T>
struct function_ {
  std::optional<T> Get() const {
    if (obj_ == nullptr || obj_->GetType() != ObjectType::Data) {
      return std::optional<T>();
    }
    if (obj_->IsRefType() == false) {
      return static_cast<const Data*>(obj_)->Get<T>();
    }
    return static_cast<const DataView*>(obj_)->Get<T>();
  }

  const Object* obj_;
  explicit function_(const Object* obj) : obj_(obj) {}
};

template <>
struct function_<std::string> {
  std::optional<std::string> Get() const {
    if (obj_ == nullptr || obj_->GetType() != ObjectType::Data) {
      return std::optional<std::string>();
    }
    if (obj_->IsRefType() == false) {
      return static_cast<const Data*>(obj_)->Get<std::string>();
    }
    return std::optional<std::string>();
  }

  const Object* obj_;
  explicit function_(const Object* obj) : obj_(obj) {}
};

template <>
struct function_<std::string_view> {
  std::optional<std::string_view> Get() const {
    if (obj_ == nullptr || obj_->GetType() != ObjectType::Data) {
      return std::optional<std::string_view>();
    }
    if (obj_->IsRefType() == true) {
      return static_cast<const DataView*>(obj_)->Get<std::string_view>();
    }
    return std::optional<std::string_view>();
  }

  const Object* obj_;
  explicit function_(const Object* obj) : obj_(obj) {}
};

class ObjectWrapper {
 public:
  ObjectWrapper() : obj_(nullptr) {}

  explicit ObjectWrapper(const Object* obj) : obj_(obj) {}

  std::optional<ObjectType> GetType() const {
    if (obj_ == nullptr) {
      return std::optional<ObjectType>();
    }
    return obj_->GetType();
  }

  ObjectWrapper operator[](const std::string& key) const {
    if (obj_ == nullptr || obj_->GetType() != ObjectType::Map) {
      return ObjectWrapper(nullptr);
    }
    if (obj_->IsRefType() == false) {
      return ObjectWrapper(static_cast<const Map*>(obj_)->operator[](key));
    } else {
      return ObjectWrapper(static_cast<const MapView*>(obj_)->operator[](key));
    }
  }

  std::optional<ObjectWrapperIterator> GetIteraotr() const {
    if (obj_ == nullptr || obj_->GetType() != ObjectType::Map) {
      return std::optional<ObjectWrapperIterator>();
    }
    if (obj_->IsRefType() == false) {
      auto tmp = static_cast<const Map*>(obj_);
      return ObjectWrapperIterator(tmp->Begin(), tmp->End());
    } else {
      auto tmp = static_cast<const MapView*>(obj_);
      return ObjectWrapperIterator(tmp->Begin(), tmp->End());
    }
  }

  ObjectWrapper operator[](size_t n) const {
    if (obj_ == nullptr || obj_->GetType() != ObjectType::Array) {
      return ObjectWrapper(nullptr);
    }
    return ObjectWrapper(static_cast<const Array*>(obj_)->operator[](n));
  }

  std::optional<size_t> Size() const {
    if (obj_ == nullptr || (obj_->GetType() != ObjectType::Map && obj_->GetType() != ObjectType::Array)) {
      return std::optional<size_t>();
    }
    if (obj_->GetType() == ObjectType::Map) {
      if (obj_->IsRefType() == false) {
        return static_cast<const Map*>(obj_)->Size();
      } else {
        return static_cast<const MapView*>(obj_)->Size();
      }
    }
    return static_cast<const Array*>(obj_)->Size();
  }

  template <typename T>
  std::optional<T> Get() const {
    return function_<T>(obj_).Get();
  }

  template <typename T>
  const T* GetPtr() const {
    if (obj_ == nullptr || obj_->GetType() != ObjectType::Data) {
      return nullptr;
    }
    // todo : 支持DataView
    if (obj_->IsRefType() == true) {
      return nullptr;
    }
    return static_cast<const Data*>(obj_)->GetPtr<T>();
  }

  bool Empty() const { return obj_ == nullptr; }

 private:
  const Object* obj_;
};

inline ObjectWrapper ObjectWrapperIterator::GetValue() const {
  if (view_ == false) {
    return ObjectWrapper(iter_->second.get());
  }
  return ObjectWrapper(iter_view_->second.get());
}

inline Map* AsMap(Object* obj) {
  if (obj->GetType() != ObjectType::Map || obj->IsRefType() == true) {
    return nullptr;
  }
  return static_cast<Map*>(obj);
}

inline const Map* AsMap(const Object* obj) { return AsMap(const_cast<Object*>(obj)); }

inline Map* AsMap(unique_ptr<Object>& obj) { return AsMap(obj.get()); }

inline Array* AsArray(Object* obj) {
  if (obj->GetType() != ObjectType::Array) {
    return nullptr;
  }
  return static_cast<Array*>(obj);
}

inline const Array* AsArray(const Object* obj) { return AsArray(const_cast<Object*>(obj)); }

inline Array* AsArray(unique_ptr<Object>& obj) { return AsArray(obj.get()); }

inline Data* AsData(Object* obj) {
  if (obj->GetType() != ObjectType::Data || obj->IsRefType() == true) {
    return nullptr;
  }
  return static_cast<Data*>(obj);
}

inline const Data* AsData(const Object* obj) { return AsData(const_cast<Object*>(obj)); }

inline Data* AsData(unique_ptr<Object>& obj) { return AsData(obj.get()); }

enum class SeriType : char {
  NORMAL,
  REPLACE,
};

constexpr inline char SeriTypeToChar(SeriType st) { return static_cast<char>(st); }

inline void SeriTotalSizeToOuter(uint64_t total_size, std::string& outer) {
  assert(outer.size() >= sizeof(uint64_t));
  if (Endian::Instance().GetEndianType() == Endian::Type::Little) {
    total_size = flipByByte(total_size);
  }
  *reinterpret_cast<uint64_t*>(&outer[0]) = total_size;
}

inline uint64_t ParseTotalSize(const std::string& inner) {
  assert(inner.size() >= sizeof(uint64_t));
  uint64_t total_size = *reinterpret_cast<const uint64_t*>(&inner[0]);
  if (Endian::Instance().GetEndianType() == Endian::Type::Little) {
    total_size = flipByByte(total_size);
  }
  return total_size;
}

template <typename T>
requires bridge_secondary_struct<T>
inline void SuffixSecondary(std::string& content, const T& secondary) {
  // 将secondary信息后缀在序列化数据后面
  auto secondary_str = secondary.Serialize();
  seriLength(secondary_str.size(), content);
  content.append(secondary_str);
}

template <typename T>
requires bridge_secondary_struct<T>
inline T ParseSecondary(const std::string& content, size_t& total_size) {
  char skip = 0;
  size_t secondary_size = parseLength(content, total_size, skip);
  std::string_view secondary_str(&content[total_size + skip], secondary_size);
  T ss = T::Construct(secondary_str);
  total_size = total_size + skip + secondary_size;
  return ss;
}

inline std::string SerializeNormal(unique_ptr<Object>&& obj) {
  SplitInfo sp_info;
  Map root;
  root.Insert("root", std::move(obj));
  std::string ret;
  ret.reserve(1024);
  ret.resize(sizeof(uint64_t));
  ret.push_back(SeriTypeToChar(SeriType::NORMAL));
  size_t split_flag_position = ret.size();
  ret.push_back(char(0));
  bool need_to_split = false;
  root.valueSeri(ret, nullptr, &sp_info, need_to_split);
  if (need_to_split == true) {
    ret[split_flag_position] = 0x0A;
  }
  size_t total_size = ret.size();
  SeriTotalSizeToOuter(total_size, ret);
  SuffixSecondary(ret, sp_info);
  return ret;
}

inline std::string SerializeReplace(unique_ptr<Object>&& obj) {
  Map root;
  StringMap string_map;
  SplitInfo sp_info;
  root.Insert("root", std::move(obj));
  std::string ret;
  ret.reserve(1024);
  // 首部8个字节存储不包含辅助结构的总大小，这里先占位
  ret.resize(sizeof(uint64_t));
  ret.push_back(SeriTypeToChar(SeriType::REPLACE));
  size_t split_flag_position = ret.size();
  ret.push_back(char(0));
  bool need_to_split = false;
  root.valueSeri(ret, &string_map, &sp_info, need_to_split);
  if (need_to_split == true) {
    ret[split_flag_position] = 0x0A;
  }
  size_t total_size = ret.size();
  SeriTotalSizeToOuter(total_size, ret);
  // 将string_map信息后缀在序列化数据后面
  SuffixSecondary(ret, string_map);
  SuffixSecondary(ret, sp_info);
  return ret;
}

template <SeriType type = SeriType::NORMAL>
inline std::string Serialize(unique_ptr<Object>&& obj) {
  if constexpr (type == SeriType::NORMAL) {
    return SerializeNormal(std::move(obj));
  } else {
    return SerializeReplace(std::move(obj));
  }
}

class Scheduler {
 public:
  explicit Scheduler(const std::string& content) : content_(content), use_string_map_(false), total_size_(0) {
    initFromContent(content);
  }

  virtual ~Scheduler() = default;

  virtual unique_ptr<Object> Parse() = 0;

 protected:
  const std::string& content_;

  const StringMap* GetStringMap() const {
    if (use_string_map_ == false) {
      return nullptr;
    }
    return &string_map_;
  }

  const SplitInfo& GetSplitInfo() const { return split_info_; }

  size_t GetTotalSize() const { return total_size_; }

  size_t GetMetaSize() const { return sizeof(uint64_t) + 2 * sizeof(char); }

  bool NeedToSplit() const { return need_to_split_; }

 private:
  StringMap string_map_;
  SplitInfo split_info_;
  bool use_string_map_;
  bool need_to_split_;
  size_t total_size_;

  void initFromContent(const std::string& content) {
    size_t meta_size = GetMetaSize();
    if (content.size() < meta_size) {
      throw std::logic_error("content.size() < meta_size");
    }
    uint64_t total_size = ParseTotalSize(content);
    char c = content[sizeof(uint64_t)];
    InnerWrapper wrapper(content);
    wrapper.skip(meta_size);
    size_t offset = meta_size;
    if (c == SeriTypeToChar(SeriType::NORMAL)) {
      uint64_t tmp_total_size = total_size;
      split_info_ = ParseSecondary<SplitInfo>(content, tmp_total_size);
    } else if (c == SeriTypeToChar(SeriType::REPLACE)) {
      // ParseSecondary会改变total_size，因此申请一个临时值
      uint64_t tmp_total_size = total_size;
      string_map_ = ParseSecondary<StringMap>(content, tmp_total_size);
      split_info_ = ParseSecondary<SplitInfo>(content, tmp_total_size);
      use_string_map_ = true;
    } else {
      throw std::logic_error("invalid seri type : " + std::to_string(c));
    }
    total_size_ = total_size;
    char split_flag = content[sizeof(uint64_t) + sizeof(char)];
    if (split_flag == 0x00) {
      need_to_split_ = false;
    } else if (split_flag == 0x0A) {
      need_to_split_ = true;
    } else {
      throw std::logic_error("invalid split flag : " + std::to_string((int)split_flag));
    }
  }
};

class NormalScheduler : public Scheduler {
 public:
  NormalScheduler(const std::string& content, bool parse_ref) : Scheduler(content), parse_ref_(parse_ref) {}

  unique_ptr<Object> Parse() override {
    size_t meta_size = GetMetaSize();
    InnerWrapper wrapper(content_);
    wrapper.skip(meta_size);
    size_t offset = meta_size;
    unique_ptr<Object> root(nullptr, object_pool_deleter);
    if (parse_ref_ == false) {
      root = ValueFactory<Map>();
    } else {
      root = ValueFactory<MapView>();
    }
    root->valueParse(wrapper, offset, parse_ref_, GetStringMap());
    if (offset != GetTotalSize()) {
      throw std::logic_error("parse error");
    }
    return parse_ref_ == false ? AsMap(root)->Get("root") : static_cast<MapView*>(root.get())->Get("root");
  }

 private:
  bool parse_ref_;
};

class CoroutineScheduler : public Scheduler {
 public:
  CoroutineScheduler(const std::string& content, bool parse_ref, size_t wn)
      : Scheduler(content), parse_ref_(parse_ref), worker_num_(wn) {}

  unique_ptr<Object> Parse() override;

  ~CoroutineScheduler() = default;

 private:
  bool parse_ref_;
  size_t worker_num_;
};

enum class SchedulerType {
  Normal,
  Coroutine,
};

struct ParseOption {
  SchedulerType type = SchedulerType::Normal;
  bool parse_ref = false;
  size_t worker_num_ = 1;
};

unique_ptr<Object> Parse(const std::string& content, ParseOption po = ParseOption());

inline void ClearResource() {
  ObjectPool<Data>::Instance().Clear();
  ObjectPool<DataView>::Instance().Clear();
  ObjectPool<Array>::Instance().Clear();
  ObjectPool<Map>::Instance().Clear();
  ObjectPool<MapView>::Instance().Clear();
}

}  // namespace bridge