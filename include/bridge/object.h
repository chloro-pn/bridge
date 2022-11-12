#pragma once

#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "async_executor/executor.h"
#include "async_simple/coro/Collect.h"
#include "async_simple/coro/Lazy.h"
#include "async_simple/coro/SyncAwait.h"
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

class Data;
class DataView;
class Array;
class ArrayItem;
class Map;
class MapItem;
class MapView;
class MapViewItem;

class BridgePool {
 public:
  BridgePool();

  BridgePool(const BridgePool&) = delete;

  BridgePool(BridgePool&&) = default;

  BridgePool& operator=(BridgePool&&) = default;
  BridgePool& operator=(const BridgePool&) = delete;

  void Merge(BridgePool&& other);

  void Clear() {
    data_pool_.Clear();
    data_view_pool_.Clear();
    array_pool_.Clear();
    array_item_pool_.Clear();
    map_pool_.Clear();
    map_view_pool_.Clear();
    map_item_pool_.Clear();
    map_view_item_pool_.Clear();
  }

  template <typename... Args>
  unique_ptr<Data> data(Args&&... args);

  template <typename... Args>
  unique_ptr<DataView> data_view(Args&&... args);

  unique_ptr<Array> array();

  ArrayItem* array_item();

  unique_ptr<Map> map();

  MapItem* map_item();

  unique_ptr<MapView> map_view();

  MapViewItem* map_view_item();

  unique_ptr<Object> object_factory(ObjectType type, bool parse_ref);

 private:
  ObjectPool<Data> data_pool_;
  ObjectPool<DataView> data_view_pool_;
  ObjectPool<Array> array_pool_;
  ObjectPool<ArrayItem> array_item_pool_;
  ObjectPool<Map> map_pool_;
  ObjectPool<MapItem> map_item_pool_;
  ObjectPool<MapView> map_view_pool_;
  ObjectPool<MapViewItem> map_view_item_pool_;
};

class Data : public Object, public RequireDestruct {
 public:
  // 默认构造函数，Data处于不合法状态
  Data() : Object(ObjectType::Data, false), data_type_(BRIDGE_INVALID) {}

  template <typename T>
  explicit Data(T&& obj) : Object(ObjectType::Data, false) {
    data_type_ = DataTypeTrait<T>::dt;
    // 不需要抛出异常，编译期推导保证
    assert(data_type_ != BRIDGE_CUSTOM && data_type_ != BRIDGE_INVALID);
    serialize(std::forward<T>(obj), variant_);
  }

  template <bridge_custom_type T>
  explicit Data(T&& obj) : Object(ObjectType::Data, false) {
    data_type_ = DataTypeTrait<T>::dt;
    // 不需要抛出异常，编译期推导保证
    assert(data_type_ == BRIDGE_CUSTOM);
    serialize(obj.SerializeToBridge(), variant_);
  }

  template <typename T>
  Data& operator=(T&& obj) {
    destruct_by_datatype();
    data_type_ = DataTypeTrait<T>::dt;
    assert(data_type_ != BRIDGE_CUSTOM && data_type_ != BRIDGE_INVALID);
    serialize(std::forward<T>(obj), variant_);
    return *this;
  }

  template <bridge_custom_type T>
  Data& operator=(T&& obj) {
    destruct_by_datatype();
    data_type_ = DataTypeTrait<T>::dt;
    assert(data_type_ == BRIDGE_CUSTOM);
    serialize(obj.SerializeToBridge(), variant_);
    return *this;
  }

  template <bridge_data_type T>
  std::optional<T> Get() const {
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
    BRIDGE_CHECK_DATA_TYPE(data_type_);
    if (data_type_ == BRIDGE_STRING && map != nullptr) {
      uint32_t id = parseLength(inner, offset);
      std::string_view str = map->GetStr(id);
      variant_.construct<std::string>(str);
      return;
    }
    uint64_t size = parseLength(inner, offset);
    parseData(data_type_, inner.curAddr(), size, variant_);
    inner.skip(size);
    BRIDGE_CHECK_OOR(inner);
    offset += size;
  }

  template <typename Outer>
  requires bridge_outer_concept<Outer>
  void valueSeri(Outer& outer, StringMap* map, SplitInfo* si, bool& need_to_split) const {
    need_to_split = false;
    BRIDGE_CHECK_DATA_TYPE(data_type_);
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

class DataView : public Object, public DontRequireDestruct {
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
    BRIDGE_CHECK_DATA_TYPE(data_type_);
    if (data_type_ == BRIDGE_STRING && map != nullptr) {
      uint32_t id = parseLength(inner, offset);
      variant_.construct<std::string_view>(map->GetStr(id));
      return;
    }
    uint64_t size = parseLength(inner, offset);
    parseData(data_type_, inner.curAddr(), size, variant_);
    inner.skip(size);
    BRIDGE_CHECK_OOR(inner);
    offset += size;
  }

  template <typename Outer>
  requires bridge_outer_concept<Outer>
  void valueSeri(Outer& outer, StringMap* map, SplitInfo* si, bool& need_to_split) const {
    need_to_split = false;
    BRIDGE_CHECK_DATA_TYPE(data_type_);
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

class BridgePool;

// CRTP. 复用逻辑
template <typename Derived>
class BridgeContainerType {
 public:
  BridgeContainerType(BridgePool& bp) : bridge_pool_(bp) {}

 protected:
  BridgePool& bridge_pool_;
  /*
   * @brief 通过Derived.Size()获取容器中子元素的个数并序列化
   */
  template <typename Outer>
  requires bridge_outer_concept<Outer>
  void seri_count(Outer& outer) const {
    uint32_t count = static_cast<const Derived*>(this)->Size();
    seriLength(count, outer);
  }

  /*
   * @brief 如果si != nullptr，向si申请一个id(默认为0)并序列化到outer中。
   * @return 申请的id或者0。
   */
  template <typename Outer>
  requires bridge_outer_concept<Outer> uint32_t seri_split_id(Outer& outer, SplitInfo* si)
  const {
    uint32_t split_info_id = 0;
    if (si != nullptr) {
      split_info_id = si->RequestId();
    }
    seriLength(split_info_id, outer);
    return split_info_id;
  }

  /*
   * @brief 向outer中序列化一个字符串，如果map != nullptr，则将其注册成为id
   */
  template <typename Outer>
  requires bridge_outer_concept<Outer>
  void seri_key(Outer& outer, StringMap* map, std::string_view key) const {
    if (map == nullptr) {
      uint32_t key_length = key.size();
      seriLength(key_length, outer);
      outer.append(key);
    } else {
      uint32_t id = map->RegisterIdFromString(key);
      seriLength(id, outer);
    }
  }

  /*
   * @brief 从inner的offset位置开始解析一个key，如果map != nullptr则解析一个id并从map中获取对应的key。
   */
  template <typename Inner>
  requires bridge_inner_concept<Inner> std::string_view parse_key(const Inner& inner, size_t& offset,
                                                                  const StringMap* map)
  const {
    std::string_view key_view;
    if (map == nullptr) {
      uint64_t key_length = parseLength(inner, offset);
      key_view = std::string_view(inner.curAddr(), key_length);
      inner.skip(key_length);
      BRIDGE_CHECK_OOR(inner);
      offset += key_length;
    } else {
      uint32_t id = parseLength(inner, offset);
      key_view = map->GetStr(id);
    }
    return key_view;
  }

  /*
   * @brief 序列化节点cur，如果节点本身是需要分片的，则将节点类型的最高位置1。
   */
  template <typename Outer>
  requires bridge_outer_concept<Outer>
  void seri_child(Outer& outer, StringMap* map, SplitInfo* si, const Object* cur) const {
    size_t object_type_position = outer.size();
    seriObjectType(cur->GetType(), outer);
    bool child_need_to_split = false;
    cur->valueSeri(outer, map, si, child_need_to_split);
    if (child_need_to_split == true) {
      outer[object_type_position] |= 0x80;
      assert(cur->GetType() != ObjectType::Data);
    }
  }

  /*
   * @brief 从inner的offset位置开始反序列化一个节点
   */
  template <typename Inner>
  requires bridge_inner_concept<Inner> unique_ptr<Object> parse_child(const Inner& inner, size_t& offset,
                                                                      bool parse_ref, const StringMap* map) {
    ObjectType type = parseObjectType(inner, offset);
    auto v = bridge_pool_.object_factory(type, parse_ref);
    // auto v = ObjectFactory(type, parse_ref);
    v->valueParse(inner, offset, parse_ref, map);
    return v;
  }

  /*
   * @brief 根据block_size的信息选择是否将split_info_id和block_size注册到si中。
   *   - 如果分块大于1个，则需要注册分块信息
   *   - 如果分块等于1个，检查分块是否大于临界值，如果大于，则注册分块信息，否则不注册分块信息。
   *   - 如果分块为0个（空数组）,不注册分块信息
   * @node 如果不注册的话，申请的split_info_id就浪费了，无法归还给si。
   */
  bool register_split_info(uint32_t split_info_id, std::vector<uint32_t>&& block_size, SplitInfo* si) const {
    if (si != nullptr) {
      if (block_size.empty() == false && (block_size.size() >= 2 || block_size.back() >= split_block_size)) {
        si->RegisterSplitInfo(split_info_id, std::move(block_size));
        return true;
      }
    }
    return false;
  }
};

struct ArrayItem : public DontRequireDestruct {
  unique_ptr<Object> node_;
  ArrayItem* next_;

  ArrayItem() : node_(nullptr, object_pool_deleter), next_(nullptr) {}
};

class Array : public Object, public BridgeContainerType<Array>, public DontRequireDestruct {
 public:
  explicit Array(BridgePool& bp)
      : Object(ObjectType::Array, false), BridgeContainerType<Array>(bp), last_child_(nullptr), size_(0) {}

  void Merge(const std::tuple<ArrayItem*, ArrayItem*, size_t, BridgePool>& block) {
    auto head = std::get<0>(block);
    auto tail = std::get<1>(block);
    auto count = std::get<2>(block);
    if (head == nullptr || tail == nullptr) {
      assert(head == nullptr && tail == nullptr);
      return;
    }
    tail->next_ = last_child_;
    last_child_ = head;
    size_ = size_ + count;
  }

  void Insert(unique_ptr<Object>&& value) {
    auto node = bridge_pool_.array_item();
    node->node_ = std::move(value);
    if (last_child_ != nullptr) {
      node->next_ = last_child_;
      last_child_ = node;
    } else {
      last_child_ = node;
    }
    size_ += 1;
  }

  void Clear() {
    // 由资源池释放资源
    last_child_ = nullptr;
    size_ = 0;
  }

  size_t Size() const { return size_; }

  const Object* operator[](size_t n) const {
    if (n >= Size()) {
      return nullptr;
    }
    // 逆序存储
    n = Size() - 1 - n;
    size_t i = 0;
    ArrayItem* tmp = last_child_;
    while (i < n) {
      if (tmp == nullptr) {
        return nullptr;
      }
      tmp = tmp->next_;
      i += 1;
    }
    if (tmp == nullptr) {
      return nullptr;
    }
    return tmp->node_.get();
  }

  template <typename Inner>
  requires bridge_inner_concept<Inner>
  void valueParse(const Inner& inner, size_t& offset, bool parse_ref, const StringMap* map) {
    Clear();
    ArrayItem* head = nullptr;
    ArrayItem* tail = nullptr;
    uint64_t count = parseLength(inner, offset);
    // 对于不需要并行解析的情况，我们仅将这个id解析出来即可，没有其他开销
    uint32_t splite_info_id = parseLength(inner, offset);
    for (uint64_t i = 0; i < count; ++i) {
      auto v = parse_child(inner, offset, parse_ref, map);
      ArrayItem* tmp = bridge_pool_.array_item();
      tmp->node_ = std::move(v);
      if (head == nullptr) {
        head = tmp;
        tail = head;
      } else {
        tail->next_ = tmp;
        tail = tmp;
      }
    }
    size_ = static_cast<size_t>(count);
    last_child_ = head;
  }

  template <typename Outer>
  requires bridge_outer_concept<Outer>
  void valueSeri(Outer& outer, StringMap* map, SplitInfo* si, bool& need_to_split) const {
    seri_count(outer);
    uint32_t split_info_id = seri_split_id(outer, si);
    size_t current_size = outer.size();
    std::vector<uint32_t> block_size;
    ArrayItem* cur = last_child_;
    while (cur != nullptr) {
      seri_child(outer, map, si, cur->node_.get());
      if (si != nullptr) {
        size_t tmp_size = outer.size();
        // 每当子节点序列化大小超过split_block_size字节，则生成一个分片
        if (tmp_size - current_size >= split_block_size) {
          block_size.push_back(tmp_size - current_size);
          current_size = tmp_size;
        }
      }
      cur = cur->next_;
    }
    // 最后一个分片，可能是0，是0的话忽略
    if (si != nullptr && outer.size() - current_size > 0) {
      block_size.push_back(outer.size() - current_size);
    }
    need_to_split = register_split_info(split_info_id, std::move(block_size), si);
  }

  void dump(std::string& buf, int level) const {
    std::string prefix(level, ' ');
    buf.append(prefix + ObjectTypeToStr(GetType()) + "[ " + std::to_string(Size()) + " ]");
    ArrayItem* cur = last_child_;
    while (cur != nullptr) {
      auto each = cur->node_.get();
      buf.push_back('\n');
      each->dump(buf, level + 1);
      cur = cur->next_;
    }
  }

  struct ArrayIterator {
    ArrayItem* item_;

    explicit ArrayIterator(ArrayItem* it = nullptr) : item_(it) {}

    bool operator==(const ArrayIterator& other) const { return item_ == other.item_; }

    bool operator!=(const ArrayIterator& other) const { return item_ != other.item_; }

    ArrayIterator& operator++() {
      item_ = item_->next_;
      return *this;
    }

    const Object* GetValue() const { return item_->node_.get(); }
  };

  auto Begin() const { return ArrayIterator(last_child_); }

  auto End() const { return ArrayIterator(nullptr); }

  using iter_type = ArrayIterator;

 private:
  ArrayItem* last_child_;
  size_t size_;
};

struct MapItem : public DontRequireDestruct {
  std::string key_;
  unique_ptr<Object> value_;
  MapItem* next_;

  MapItem() : value_(nullptr, object_pool_deleter), next_(nullptr) {}
};

class Map : public Object, public BridgeContainerType<Map>, public RequireDestruct {
 public:
  struct MapIterator {
    MapItem* item_;

    explicit MapIterator(MapItem* it = nullptr) : item_(it) {}

    bool operator==(const MapIterator& other) const { return item_ == other.item_; }

    bool operator!=(const MapIterator& other) const { return item_ != other.item_; }

    MapIterator& operator++() {
      item_ = item_->next_;
      return *this;
    }

    const std::string& GetKey() const { return item_->key_; }

    const Object* GetValue() const { return item_->value_.get(); }
  };

  explicit Map(BridgePool& bp)
      : Object(ObjectType::Map, false), BridgeContainerType<Map>(bp), last_node_(nullptr), size_(0) {}

  void Merge(const std::tuple<MapItem*, MapItem*, size_t, BridgePool>& block) {
    auto head = std::get<0>(block);
    auto tail = std::get<1>(block);
    auto count = std::get<2>(block);
    if (head == nullptr || tail == nullptr) {
      assert(head == nullptr && tail == nullptr);
      return;
    }
    tail->next_ = last_node_;
    last_node_ = head;
    size_ = size_ + count;
  }

  void Insert(const std::string& key, unique_ptr<Object>&& value) {
    auto node = bridge_pool_.map_item();
    node->key_ = key;
    node->value_ = std::move(value);
    if (last_node_ == nullptr) {
      last_node_ = node;
    } else {
      node->next_ = last_node_;
      last_node_ = node;
    }
    size_ += 1;
  }

  const Object* operator[](const std::string& key) const {
    MapItem* cur = last_node_;
    while (cur != nullptr) {
      if (cur->key_ == key) {
        return cur->value_.get();
      }
      cur = cur->next_;
    }
    return nullptr;
  }

  unique_ptr<Object> Get(const std::string& key) {
    MapItem* cur = last_node_;
    while (cur != nullptr) {
      if (cur->key_ == key) {
        return std::move(cur->value_);
      }
      cur = cur->next_;
    }
    return unique_ptr<Object>(nullptr, object_pool_deleter);
  }

  size_t Size() const { return size_; }

  void Clear() {
    last_node_ = nullptr;
    size_ = 0;
  }

  auto Begin() const { return MapIterator(last_node_); }

  auto End() const { return MapIterator(nullptr); }

  template <typename Inner>
  requires bridge_inner_concept<Inner>
  void valueParse(const Inner& inner, size_t& offset, bool parse_ref, const StringMap* map) {
    Clear();
    uint64_t count = parseLength(inner, offset);
    // 对于不需要并行解析的情况，我们仅将这个id解析出来即可，没有其他开销
    uint32_t splite_info_id = parseLength(inner, offset);
    BRIDGE_CHECK_OOR(inner);
    for (uint64_t i = 0; i < count; ++i) {
      // parse key
      std::string_view key_view = parse_key(inner, offset, map);
      // parse value
      auto v = parse_child(inner, offset, parse_ref, map);
      Insert(std::string(key_view), std::move(v));
    }
    size_ = static_cast<size_t>(count);
  }

  template <typename Outer>
  requires bridge_outer_concept<Outer>
  void valueSeri(Outer& outer, StringMap* map, SplitInfo* si, bool& need_to_split) const {
    seri_count(outer);
    uint32_t split_info_id = seri_split_id(outer, si);

    size_t current_size = outer.size();
    std::vector<uint32_t> block_size;
    MapItem* cur = last_node_;
    while (cur != nullptr) {
      seri_key(outer, map, cur->key_);
      seri_child(outer, map, si, cur->value_.get());
      if (si != nullptr) {
        size_t tmp_size = outer.size();
        // 每当子节点序列化大小超过split_block_size字节，则生成一个分片
        if (tmp_size - current_size >= split_block_size) {
          block_size.push_back(tmp_size - current_size);
          current_size = tmp_size;
        }
      }
      cur = cur->next_;
    }
    // 最后一个分片，可能是0，是0的话忽略
    if (si != nullptr && outer.size() - current_size > 0) {
      block_size.push_back(outer.size() - current_size);
    }
    need_to_split = register_split_info(split_info_id, std::move(block_size), si);
  }

  void dump(std::string& buf, int level) const {
    std::string prefix(level, ' ');
    buf.append(prefix + ObjectTypeToStr(GetType()) + "[ " + std::to_string(Size()) + " ]");
    std::string key_prefix(level + 1, ' ');
    MapItem* cur = last_node_;
    while (cur != nullptr) {
      buf.push_back('\n');
      buf.append(key_prefix + "< " + cur->key_ + " > : ");
      buf.push_back('\n');
      cur->value_->dump(buf, level + 2);
      cur = cur->next_;
    }
  }

  using iter_type = MapIterator;

 private:
  MapItem* last_node_;
  size_t size_;
};

struct MapViewItem : public DontRequireDestruct {
  std::string_view key_;
  unique_ptr<Object> value_;
  MapViewItem* next_;

  MapViewItem() : value_(nullptr, object_pool_deleter), next_(nullptr) {}
};

class MapView : public Object, public BridgeContainerType<MapView>, public DontRequireDestruct {
 public:
  struct MapViewIterator {
    MapViewItem* item_;

    explicit MapViewIterator(MapViewItem* it = nullptr) : item_(it) {}

    bool operator==(const MapViewIterator& other) const { return item_ == other.item_; }

    bool operator!=(const MapViewIterator& other) const { return item_ != other.item_; }

    MapViewIterator& operator++() {
      item_ = item_->next_;
      return *this;
    }

    std::string_view GetKey() const { return item_->key_; }

    const Object* GetValue() const { return item_->value_.get(); }
  };

  explicit MapView(BridgePool& bp)
      : Object(ObjectType::Map, true), BridgeContainerType<MapView>(bp), last_node_(nullptr), size_(0) {}

  void Merge(const std::tuple<MapViewItem*, MapViewItem*, size_t, BridgePool>& block) {
    auto head = std::get<0>(block);
    auto tail = std::get<1>(block);
    auto count = std::get<2>(block);
    if (head == nullptr || tail == nullptr) {
      assert(head == nullptr && tail == nullptr);
      return;
    }
    tail->next_ = last_node_;
    last_node_ = head;
    size_ = size_ + count;
  }

  const Object* operator[](const std::string& key) const {
    MapViewItem* cur = last_node_;
    while (cur != nullptr) {
      if (cur->key_ == key) {
        return cur->value_.get();
      }
      cur = cur->next_;
    }
    return nullptr;
  }

  template <typename Inner>
  requires bridge_inner_concept<Inner>
  void valueParse(const Inner& inner, size_t& offset, bool parse_ref, const StringMap* map) {
    Clear();
    uint64_t count = parseLength(inner, offset);
    // 对于不需要并行解析的情况，我们仅将这个id解析出来即可，没有其他开销
    uint32_t splite_info_id = parseLength(inner, offset);
    for (uint64_t i = 0; i < count; ++i) {
      std::string_view key_view = parse_key(inner, offset, map);
      auto v = parse_child(inner, offset, parse_ref, map);
      Insert(key_view, std::move(v));
    }
    size_ = static_cast<size_t>(count);
  }

  template <typename Outer>
  requires bridge_outer_concept<Outer>
  void valueSeri(Outer& outer, StringMap* map, SplitInfo* si, bool& need_to_split) const {
    seri_count(outer);
    uint32_t split_info_id = seri_split_id(outer, si);

    size_t current_size = outer.size();
    std::vector<uint32_t> block_size;
    MapViewItem* cur = last_node_;
    while (cur != nullptr) {
      seri_key(outer, map, cur->key_);
      seri_child(outer, map, si, cur->value_.get());
      if (si != nullptr) {
        size_t tmp_size = outer.size();
        // 每当子节点序列化大小超过split_block_size字节，则生成一个分片
        if (tmp_size - current_size >= split_block_size) {
          block_size.push_back(tmp_size - current_size);
          current_size = tmp_size;
        }
      }
      cur = cur->next_;
    }
    // 最后一个分片，可能是0，是0的话忽略
    if (si != nullptr && outer.size() - current_size > 0) {
      block_size.push_back(outer.size() - current_size);
    }
    need_to_split = register_split_info(split_info_id, std::move(block_size), si);
  }

  void dump(std::string& buf, int level) const {
    std::string prefix(level, ' ');
    buf.append(prefix + ObjectTypeToStr(GetType()) + "[ " + std::to_string(Size()) + " ]");
    std::string key_prefix(level + 1, ' ');
    MapViewItem* cur = last_node_;
    while (cur != nullptr) {
      buf.push_back('\n');
      buf.append(key_prefix + "< " + std::string(cur->key_) + " > : ");
      buf.push_back('\n');
      cur->value_->dump(buf, level + 2);
      cur = cur->next_;
    }
  }

  auto Begin() const { return MapViewIterator(last_node_); }

  auto End() const { return MapViewIterator(nullptr); }

  size_t Size() const { return size_; }

  void Clear() {
    last_node_ = nullptr;
    size_ = 0;
  }

  unique_ptr<Object> Get(const std::string& key) {
    MapViewItem* cur = last_node_;
    while (cur != nullptr) {
      if (cur->key_ == key) {
        return std::move(cur->value_);
      }
      cur = cur->next_;
    }
    return unique_ptr<Object>(nullptr, object_pool_deleter);
  }

  void Insert(std::string_view key_view, unique_ptr<Object>&& value) {
    auto node = bridge_pool_.map_view_item();
    node->key_ = key_view;
    node->value_ = std::move(value);
    if (last_node_ == nullptr) {
      last_node_ = node;
    } else {
      node->next_ = last_node_;
      last_node_ = node;
    }
    size_ += 1;
  }

  using iter_type = MapViewIterator;

 private:
  MapViewItem* last_node_;
  size_t size_;
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

inline BridgePool::BridgePool() = default;

inline void BridgePool::Merge(BridgePool&& other) {
  data_pool_.Merge(std::move(other.data_pool_));
  data_view_pool_.Merge(std::move(other.data_view_pool_));
  array_pool_.Merge(std::move(other.array_pool_));
  array_item_pool_.Merge(std::move(other.array_item_pool_));
  map_pool_.Merge(std::move(other.map_pool_));
  map_item_pool_.Merge(std::move(other.map_item_pool_));
  map_view_pool_.Merge(std::move(other.map_view_pool_));
  map_view_item_pool_.Merge(std::move(other.map_view_item_pool_));
}

template <typename... Args>
inline unique_ptr<Data> BridgePool::data(Args&&... args) {
  return unique_ptr<Data>(data_pool_.Alloc(std::forward<Args>(args)...), object_pool_deleter);
}

template <typename... Args>
inline unique_ptr<DataView> BridgePool::data_view(Args&&... args) {
  return unique_ptr<DataView>(data_view_pool_.Alloc(std::forward<Args>(args)...), object_pool_deleter);
}

inline unique_ptr<Array> BridgePool::array() {
  return unique_ptr<Array>(array_pool_.Alloc(*this), object_pool_deleter);
}

inline ArrayItem* BridgePool::array_item() { return array_item_pool_.Alloc(); }

inline unique_ptr<Map> BridgePool::map() { return unique_ptr<Map>(map_pool_.Alloc(*this), object_pool_deleter); }

inline MapItem* BridgePool::map_item() { return map_item_pool_.Alloc(); }

inline unique_ptr<MapView> BridgePool::map_view() {
  return unique_ptr<MapView>(map_view_pool_.Alloc(*this), object_pool_deleter);
}

inline MapViewItem* BridgePool::map_view_item() { return map_view_item_pool_.Alloc(); }

inline unique_ptr<Object> BridgePool::object_factory(ObjectType type, bool parse_ref) {
  if (type == ObjectType::Data) {
    if (parse_ref == false) {
      return data();
    } else {
      return data_view();
    }
  } else if (type == ObjectType::Map) {
    if (parse_ref == false) {
      return map();
    } else {
      return map_view();
    }
  } else if (type == ObjectType::Array) {
    return array();
  } else {
    return unique_ptr<Object>(nullptr, object_pool_deleter);
  }
}

class ObjectWrapper;

#define BRIDGE_ITERATOR_DISPATCH

class ObjectWrapperIterator {
 public:
  using holder_type = Map::iter_type;
  using holder_type_view = MapView::iter_type;
  using holder_type_array = Array::iter_type;

  enum class IteratorType {
    Map,
    MapView,
    Array,
    None,
  };

  ObjectWrapperIterator(holder_type iter, holder_type end) : iter_(iter), end_(end), itype_(IteratorType::Map) {}

  ObjectWrapperIterator(holder_type_view iter, holder_type_view end)
      : iter_view_(iter), end_view_(end), itype_(IteratorType::MapView) {}

  ObjectWrapperIterator(holder_type_array iter, holder_type_array end)
      : iter_array_(iter), end_array_(end), itype_(IteratorType::Array) {}

  bool Valid() const {
    if (itype_ == IteratorType::Map) {
      return iter_ != end_;
    } else if (itype_ == IteratorType::MapView) {
      return iter_view_ != end_view_;
    } else if (itype_ == IteratorType::Array) {
      return iter_array_ != end_array_;
    }
    throw std::runtime_error("invalid iterator type on Valid");
  }

  ObjectWrapperIterator& operator++() {
    if (itype_ == IteratorType::Map) {
      ++iter_;
    } else if (itype_ == IteratorType::MapView) {
      ++iter_view_;
    } else if (itype_ == IteratorType::Array) {
      ++iter_array_;
    } else {
      throw std::runtime_error("invalid iterator type on operator++");
    }
    return *this;
  }

  std::string_view GetKey() const {
    if (itype_ == IteratorType::Map) {
      return iter_.GetKey();
    } else if (itype_ == IteratorType::MapView) {
      return iter_view_.GetKey();
    } else {
      throw std::runtime_error("invalid iterator type on GetKey");
    }
  }

  ObjectWrapper GetValue() const;

  IteratorType GetType() const { return itype_; }

 private:
  holder_type iter_;
  holder_type end_;
  holder_type_view iter_view_;
  holder_type_view end_view_;
  holder_type_array iter_array_;
  holder_type_array end_array_;
  IteratorType itype_;
};

template <typename T>
struct get_proxy_ {
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
  explicit get_proxy_(const Object* obj) : obj_(obj) {}
};

template <>
struct get_proxy_<std::string> {
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
  explicit get_proxy_(const Object* obj) : obj_(obj) {}
};

template <>
struct get_proxy_<std::string_view> {
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
  explicit get_proxy_(const Object* obj) : obj_(obj) {}
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
    if (obj_ == nullptr || (obj_->GetType() != ObjectType::Map && obj_->GetType() != ObjectType::Array)) {
      return std::optional<ObjectWrapperIterator>();
    }
    if (obj_->GetType() == ObjectType::Array) {
      auto tmp = static_cast<const Array*>(obj_);
      return ObjectWrapperIterator(tmp->Begin(), tmp->End());
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
    return get_proxy_<T>(obj_).Get();
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
  if (itype_ == IteratorType::Map) {
    return ObjectWrapper(iter_.GetValue());
  } else if (itype_ == IteratorType::MapView) {
    return ObjectWrapper(iter_view_.GetValue());
  } else if (itype_ == IteratorType::Array) {
    return ObjectWrapper(iter_array_.GetValue());
  } else {
    throw std::runtime_error("invalid iterator type on GetValue");
  }
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

// 协程Executor
using namespace async_simple::coro;

struct ParseResult {
  size_t offset;
  unique_ptr<Object> v;
  BridgePool bp;
  ParseResult(size_t os, unique_ptr<Object>&& obj, BridgePool&& bp) noexcept
      : offset(os), v(std::move(obj)), bp(std::move(bp)) {}
};

template <typename ItemType>
inline BridgePool&& GetBPFromTuple(std::tuple<ItemType*, ItemType*, size_t, BridgePool>&& it) {
  return std::get<3>(std::move(it));
}

inline Lazy<ParseResult> ParseArray(std::string_view content, const SplitInfo& si, const StringMap* sm, size_t os,
                                    bool parse_ref);

inline Lazy<ParseResult> ParseMap(std::string_view content, const SplitInfo& si, const StringMap* sm, size_t os,
                                  bool parse_ref);

inline Lazy<std::tuple<ArrayItem*, ArrayItem*, size_t, BridgePool>> ParseArrayBlock(std::string_view content,
                                                                                    size_t begin, size_t end,
                                                                                    const SplitInfo& si, bool parse_ref,
                                                                                    const StringMap* sm) {
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
    // 检测即将解析的对象是否需要并行解析
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
      } else if (type == ObjectType::Map) {
        auto parse_result = co_await ParseMap(content, si, sm, w.currentIndex(), parse_ref);
        parsed_node = std::move(parse_result.v);
        w.skipTo(parse_result.offset);
        offset = w.currentIndex();
        bp.Merge(std::move(parse_result.bp));
      } else {
        throw std::runtime_error("parse array block error : data need to split");
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
      } else if (type == ObjectType::Map) {
        auto parse_result = co_await ParseMap(content, si, sm, w.currentIndex(), false);
        parsed_object = std::move(parse_result.v);
        w.skipTo(parse_result.offset);
        offset = w.currentIndex();
        bp.Merge(std::move(parse_result.bp));
      } else {
        throw std::runtime_error("parse array block error : data need to split");
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

inline Lazy<std::tuple<MapViewItem*, MapViewItem*, size_t, BridgePool>> ParseMapViewBlock(std::string_view content,
                                                                                          size_t begin, size_t end,
                                                                                          const SplitInfo& si,
                                                                                          const StringMap* sm) {
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
      } else if (type == ObjectType::Map) {
        assert(type == ObjectType::Map);
        auto parse_result = co_await ParseMap(content, si, sm, w.currentIndex(), true);
        parsed_object = std::move(parse_result.v);
        w.skipTo(parse_result.offset);
        offset = w.currentIndex();
        bp.Merge(std::move(parse_result.bp));
      } else {
        throw std::runtime_error("parse array block error : data need to split");
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

enum class SeriType : char {
  NORMAL,
  REPLACE,
};

constexpr inline char SeriTypeToChar(SeriType st) { return static_cast<char>(st); }

inline void SeriTotalSizeToOuter(uint64_t total_size, std::string& outer) {
  if (outer.size() < sizeof(uint64_t)) {
    throw std::runtime_error("seri total size error");
  }
  if (Endian::Instance().GetEndianType() == Endian::Type::Little) {
    total_size = flipByByte(total_size);
  }
  *reinterpret_cast<uint64_t*>(&outer[0]) = total_size;
}

inline uint64_t ParseTotalSize(const std::string& inner) {
  if (inner.size() < sizeof(uint64_t)) {
    throw std::runtime_error("parse total size error");
  }
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

inline std::string SerializeNormal(unique_ptr<Object>&& obj, BridgePool& bp) {
  SplitInfo sp_info;
  Map root(bp);
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

inline std::string SerializeReplace(unique_ptr<Object>&& obj, BridgePool& bp) {
  Map root(bp);
  StringMap string_map;
  SplitInfo sp_info;
  root.Insert("root", std::move(obj));
  std::string ret;
  ret.reserve(1024);
  // 首部8个字节存储不包含辅助结构的总大小，这里先占位
  ret.resize(sizeof(uint64_t));
  // serialize seri type.
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
inline std::string Serialize(unique_ptr<Object>&& obj, BridgePool& bp) {
  if constexpr (type == SeriType::NORMAL) {
    return SerializeNormal(std::move(obj), bp);
  } else {
    return SerializeReplace(std::move(obj), bp);
  }
}

/*
 * @brief 解析的调度器基类，负责从序列化数据中解析并初始化必要的元信息（total_size、seri
 * type、string_map和split_info等）
 */
class Scheduler {
 public:
  explicit Scheduler(const std::string& content, BridgePool& bp)
      : content_(content), use_string_map_(false), total_size_(0), bp_(bp) {
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

  BridgePool& GetBridgePool() { return bp_; }

 private:
  StringMap string_map_;
  SplitInfo split_info_;
  bool use_string_map_;
  bool need_to_split_;
  size_t total_size_;
  BridgePool& bp_;

  void initFromContent(const std::string& content) {
    size_t meta_size = GetMetaSize();
    if (content.size() < meta_size) {
      throw std::runtime_error("content.size() < meta_size");
    }
    uint64_t total_size = ParseTotalSize(content);
    char c = content[sizeof(uint64_t)];
    InnerWrapper wrapper(content);
    wrapper.skip(meta_size);
    BRIDGE_CHECK_OOR(wrapper);
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
      throw std::runtime_error("invalid seri type : " + std::to_string(c));
    }
    total_size_ = total_size;
    char split_flag = content[sizeof(uint64_t) + sizeof(char)];
    if (split_flag == 0x00) {
      need_to_split_ = false;
    } else if (split_flag == 0x0A) {
      need_to_split_ = true;
    } else {
      throw std::runtime_error("invalid split flag : " + std::to_string((int)split_flag));
    }
  }
};

/*
 * @brief 递归解析，默认的解析方式
 */
class NormalScheduler : public Scheduler {
 public:
  NormalScheduler(const std::string& content, bool parse_ref, BridgePool& bp)
      : Scheduler(content, bp), parse_ref_(parse_ref) {}

  unique_ptr<Object> Parse() override {
    size_t meta_size = GetMetaSize();
    InnerWrapper wrapper(content_);
    wrapper.skip(meta_size);
    BRIDGE_CHECK_OOR(wrapper);
    size_t offset = meta_size;
    unique_ptr<Object> root(nullptr, object_pool_deleter);
    if (parse_ref_ == false) {
      root = GetBridgePool().map();
    } else {
      root = GetBridgePool().map_view();
    }
    root->valueParse(wrapper, offset, parse_ref_, GetStringMap());
    if (offset != GetTotalSize()) {
      throw std::runtime_error("parse error");
    }
    return parse_ref_ == false ? AsMap(root)->Get("root") : static_cast<MapView*>(root.get())->Get("root");
  }

 private:
  bool parse_ref_;
};

/*
 * @brief 通过协程(c++20 coroutine) + thread_pool并行解析
 */
class CoroutineScheduler : public Scheduler {
 public:
  CoroutineScheduler(const std::string& content, bool parse_ref, size_t wn, BridgePool& bp)
      : Scheduler(content, bp), parse_ref_(parse_ref), worker_num_(wn) {}

  unique_ptr<Object> Parse() override;

  ~CoroutineScheduler() = default;

 private:
  bool parse_ref_;
  size_t worker_num_;
};

inline unique_ptr<Object> CoroutineScheduler::Parse() {
  size_t meta_size = GetMetaSize();
  // 判断根节点是否需要并行，如果不需要，直接构造NormalSchedule解析
  if (NeedToSplit() == false) {
    NormalScheduler ns(content_, parse_ref_, GetBridgePool());
    return ns.Parse();
  }
  BridgeExecutor e(worker_num_);
  async_simple::coro::RescheduleLazy Scheduled =
      ParseMap(content_, GetSplitInfo(), GetStringMap(), meta_size, parse_ref_).via(&e);
  auto root = async_simple::coro::syncAwait(Scheduled);
  GetBridgePool().Merge(std::move(root.bp));
  return parse_ref_ == false ? AsMap(root.v)->Get("root") : static_cast<MapView*>(root.v.get())->Get("root");
}

enum class SchedulerType {
  Normal,
  Coroutine,
};

struct ParseOption {
  SchedulerType type = SchedulerType::Normal;
  bool parse_ref = false;
  size_t worker_num_ = 1;
};

inline unique_ptr<Object> Parse(const std::string& content, BridgePool& bp, ParseOption po = ParseOption()) {
  if (po.type == SchedulerType::Normal) {
    NormalScheduler ns(content, po.parse_ref, bp);
    return ns.Parse();
  }
  if (po.type == SchedulerType::Coroutine) {
    CoroutineScheduler cs(content, po.parse_ref, po.worker_num_, bp);
    return cs.Parse();
  }
  throw std::logic_error("invalid po.type");
  return unique_ptr<Object>(nullptr, object_pool_deleter);
}
}  // namespace bridge