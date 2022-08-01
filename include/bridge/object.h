#pragma once

#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "bridge/data_type.h"
#include "bridge/inner.h"
#include "bridge/object_type.h"
#include "bridge/parse.h"
#include "bridge/serialize.h"
#include "bridge/string_map.h"
#include "bridge/type_trait.h"

namespace bridge {

class Object {
 public:
  Object(ObjectType type, bool ref_type) : type_(type), ref_type_(ref_type) {}

  ObjectType GetType() const { return type_; }

  bool IsRefType() const { return ref_type_; }

  virtual ~Object() = default;

  template <typename Inner>
  requires bridge_inner_concept<Inner>
  void valueParse(const Inner& inner, size_t& offset, bool parse_ref, const StringMap* map);

  template <typename Outer>
  requires bridge_outer_concept<Outer>
  void valueSeri(Outer& outer, const StringMap* map) const;

  void registerId(StringMap& map) const;

 private:
  ObjectType type_;
  bool ref_type_;
};

std::unique_ptr<Object> ObjectFactory(ObjectType type, bool parse_ref = false);

class Data : public Object {
 public:
  // 默认构造函数，Data处于不合法状态
  Data() : Object(ObjectType::Data, false), data_type_(BRIDGE_INVALID) {}

  template <typename T>
  explicit Data(T&& obj) : Object(ObjectType::Data, false) {
    data_type_ = DataTypeTrait<T>::dt;
    assert(data_type_ != BRIDGE_CUSTOM && data_type_ != BRIDGE_INVALID);
    serialize(std::forward<T>(obj), data_);
  }

  template <bridge_custom_type T>
  explicit Data(T&& obj) : Object(ObjectType::Data, false) {
    data_type_ = DataTypeTrait<T>::dt;
    assert(data_type_ == BRIDGE_CUSTOM);
    serialize(obj.SerializeToBridge(), data_);
  }

  template <typename T>
  Data& operator=(T&& obj) {
    data_type_ = DataTypeTrait<T>::dt;
    serialize(std::forward<T>(obj), data_);
  }

  template <bridge_custom_type T>
  Data& operator=(T&& obj) {
    data_type_ = BRIDGE_CUSTOM;
    serialize(obj.SerializeToBridge(), data_);
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
    return std::optional(parse<T>(data_));
  }

  template <typename T>
  requires bridge_custom_type<T> std::optional<T> Get()
  const {
    static_assert(NoRefNoPointer<T>::value,
                  "get() method should not return ref or pointer type (including <T* const> and <const T*>)");
    // 当处于不合法状态时，总是返回空的optional
    if (DataTypeTrait<T>::dt != data_type_) {
      return std::optional<T>();
    }
    return std::optional(T::ConstructFromBridge(data_));
  }

  std::string_view GetView() const { return std::string_view(static_cast<const char*>(&data_[0]), data_.size()); }

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
      data_.resize(str.size());
      memcpy(&data_[0], &str[0], str.size());
      return;
    }
    uint64_t size = parseLength(inner, offset);
    if (inner.outOfRange()) {
      return;
    }
    data_.resize(size);
    memcpy(&data_[0], inner.curAddr(), size);
    inner.skip(size);
    if (inner.outOfRange()) {
      return;
    }
    offset += size;
  }

  template <typename Outer>
  requires bridge_outer_concept<Outer>
  void valueSeri(Outer& outer, const StringMap* map) const {
    assert(data_type_ != BRIDGE_INVALID);
    seriDataType(data_type_, outer);
    if (data_type_ == BRIDGE_STRING && map != nullptr) {
      uint32_t id = map->GetId(std::string_view(&data_[0], data_.size()));
      seriLength(id, outer);
      return;
    }
    uint32_t length = data_.size();
    seriLength(length, outer);
    outer.append(&data_[0], data_.size());
  }

  void registerId(StringMap& map) const {
    if (data_type_ == BRIDGE_STRING) {
      map.RegisterIdFromString(std::string_view(&data_[0], data_.size()));
    }
  }

 private:
  std::vector<char> data_;
  uint8_t data_type_;
};

class DataView : public Object {
 public:
  DataView() : Object(ObjectType::Data, true), data_type_(BRIDGE_INVALID), view_() {}

  explicit DataView(std::string_view data_view, uint8_t data_type)
      : Object(ObjectType::Data, true), data_type_(data_type), view_(data_view) {}

  std::string_view GetView() const { return view_; }

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
      view_ = map->GetStr(id);
      return;
    }
    uint64_t size = parseLength(inner, offset);
    if (inner.outOfRange()) {
      return;
    }
    view_ = std::string_view(inner.curAddr(), size);
    inner.skip(size);
    if (inner.outOfRange()) {
      return;
    }
    offset += size;
  }

  template <typename Outer>
  requires bridge_outer_concept<Outer>
  void valueSeri(Outer& outer, const StringMap* map) const {
    assert(data_type_ != BRIDGE_INVALID);
    seriDataType(data_type_, outer);
    if (data_type_ == BRIDGE_STRING && map != nullptr) {
      uint32_t id = map->GetId(view_);
      seriLength(id, outer);
      return;
    }
    uint32_t length = view_.size();
    seriLength(length, outer);
    outer.append(&view_[0], view_.size());
  }

  void registerId(StringMap& map) const {
    if (data_type_ == BRIDGE_STRING) {
      map.RegisterIdFromString(view_);
    }
  }

 private:
  std::string_view view_;
  uint8_t data_type_;
};

class Array : public Object {
 public:
  Array() : Object(ObjectType::Array, false) {}

  Array(std::vector<std::unique_ptr<Object>>&& values)
      : Object(ObjectType::Array, false), objects_(std::move(values)) {}

  void Insert(std::unique_ptr<Object>&& value) { objects_.push_back(std::move(value)); }

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
  void valueSeri(Outer& outer, const StringMap* map) const {
    uint32_t count = objects_.size();
    seriLength(count, outer);
    for (auto& each : objects_) {
      seriObjectType(each->GetType(), outer);
      each->valueSeri(outer, map);
    }
  }

  void registerId(StringMap& map) const {
    for (auto& each : objects_) {
      each->registerId(map);
    }
  }

 private:
  std::vector<std::unique_ptr<Object>> objects_;
};

class Map : public Object {
 public:
  Map() : Object(ObjectType::Map, false) {}

  bool Insert(const std::string& key, std::unique_ptr<Object>&& value) {
    if (objects_.count(key) != 0) {
      return false;
    }
    objects_[key] = std::move(value);
    return true;
  }

  bool Remove(const std::string& key) {
    if (objects_.count(key) != 0) {
      objects_.erase(key);
      return true;
    }
    return false;
  }

  const Object* operator[](const std::string& key) const {
    auto it = objects_.find(key);
    if (it == objects_.end()) {
      return nullptr;
    }
    return it->second.get();
  }

  std::unique_ptr<Object> Get(const std::string& key) {
    if (objects_.count(key) == 0) {
      return nullptr;
    }
    return std::move(objects_[key]);
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
      objects_.insert({std::string(key_view), std::move(v)});
    }
  }

  template <typename Outer>
  requires bridge_outer_concept<Outer>
  void valueSeri(Outer& outer, const StringMap* map) const {
    uint32_t count = objects_.size();
    seriLength(count, outer);
    for (auto& each : objects_) {
      // key seri
      if (map == nullptr) {
        uint32_t key_length = each.first.size();
        seriLength(key_length, outer);
        outer.append(each.first);
      } else {
        uint32_t id = map->GetId(each.first);
        seriLength(id, outer);
      }
      seriObjectType(each.second->GetType(), outer);
      each.second->valueSeri(outer, map);
    }
  }

  void registerId(StringMap& map) const {
    for (auto& each : objects_) {
      map.RegisterIdFromString(each.first);
      each.second->registerId(map);
    }
  }

 private:
  std::unordered_map<std::string, std::unique_ptr<Object>> objects_;
};

class MapView : public Object {
 public:
  MapView() : Object(ObjectType::Map, true) {}

  const Object* operator[](const std::string& key) const {
    auto it = objects_.find(key);
    if (it == objects_.end()) {
      return nullptr;
    }
    return it->second.get();
  }

  template <typename Inner>
  requires bridge_inner_concept<Inner>
  void valueParse(const Inner& inner, size_t& offset, bool parse_ref, const StringMap* map) {
    objects_.clear();
    uint64_t count = parseLength(inner, offset);
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
      objects_[key_view] = std::move(v);
    }
  }

  template <typename Outer>
  requires bridge_outer_concept<Outer>
  void valueSeri(Outer& outer, const StringMap* map) const {
    uint32_t count = objects_.size();
    seriLength(count, outer);
    for (auto& each : objects_) {
      // key seri
      if (map == nullptr) {
        uint32_t key_length = each.first.size();
        seriLength(key_length, outer);
        outer.append(&each.first[0], each.first.size());
      } else {
        uint32_t id = map->GetId(each.first);
        seriLength(id, outer);
      }
      seriObjectType(each.second->GetType(), outer);
      each.second->valueSeri(outer, map);
    }
  }

  void registerId(StringMap& map) const {
    for (auto& each : objects_) {
      map.RegisterIdFromString(each.first);
      each.second->registerId(map);
    }
  }

  auto Begin() const { return objects_.begin(); }

  auto End() const { return objects_.end(); }

  size_t Size() const { return objects_.size(); }

  std::unique_ptr<Object> Get(const std::string& key) {
    if (objects_.count(key) == 0) {
      return nullptr;
    }
    return std::move(objects_[key]);
  }

 private:
  std::unordered_map<std::string_view, std::unique_ptr<Object>> objects_;
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
void Object::valueSeri(Outer& outer, const StringMap* map) const { BRIDGE_DISPATCH(valueSeri, const, outer, map) }

inline void Object::registerId(StringMap& map) const { BRIDGE_DISPATCH(registerId, const, map) }

template <typename T, typename... Args>
inline std::unique_ptr<T> ValueFactory(Args&&... args) {
  return std::make_unique<T>(std::forward<Args>(args)...);
}

inline std::unique_ptr<Object> ObjectFactory(ObjectType type, bool parse_ref) {
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
    return nullptr;
  }
}

class ObjectWrapper;

class ObjectWrapperIterator {
 public:
  using holder_type = std::unordered_map<std::string, std::unique_ptr<Object>>::const_iterator;
  using holder_type_view = std::unordered_map<std::string_view, std::unique_ptr<Object>>::const_iterator;

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
    if (obj_ == nullptr || obj_->GetType() != ObjectType::Data) {
      return std::optional<T>();
    }
    if (obj_->IsRefType() == false) {
      return static_cast<const Data*>(obj_)->Get<T>();
    }
    return std::optional<T>();
  }

  std::optional<std::string_view> GetView() const {
    if (obj_ == nullptr || obj_->GetType() != ObjectType::Data) {
      return std::optional<std::string_view>();
    }
    if (obj_->IsRefType() == true) {
      return static_cast<const DataView*>(obj_)->GetView();
    }
    return static_cast<const Data*>(obj_)->GetView();
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

inline Map* AsMap(std::unique_ptr<Object>& obj) { return AsMap(obj.get()); }

inline Array* AsArray(Object* obj) {
  if (obj->GetType() != ObjectType::Array) {
    return nullptr;
  }
  return static_cast<Array*>(obj);
}

inline const Array* AsArray(const Object* obj) { return AsArray(const_cast<Object*>(obj)); }

inline Array* AsArray(std::unique_ptr<Object>& obj) { return AsArray(obj.get()); }

inline Data* AsData(Object* obj) {
  if (obj->GetType() != ObjectType::Data || obj->IsRefType() == true) {
    return nullptr;
  }
  return static_cast<Data*>(obj);
}

inline const Data* AsData(const Object* obj) { return AsData(const_cast<Object*>(obj)); }

inline Data* AsData(std::unique_ptr<Object>& obj) { return AsData(obj.get()); }

enum class SeriType : char {
  NORMAL,
  REPLACE,
};

constexpr inline char SeriTypeToChar(SeriType st) { return static_cast<char>(st); }

inline std::string SerializeNormal(std::unique_ptr<Object>&& obj) {
  Map root;
  root.Insert("root", std::move(obj));
  std::string ret;
  ret.reserve(1024);
  ret.push_back(SeriTypeToChar(SeriType::NORMAL));
  root.valueSeri(ret, nullptr);
  return ret;
}

inline std::string SerializeReplace(std::unique_ptr<Object>&& obj) {
  Map root;
  StringMap string_map;
  root.Insert("root", std::move(obj));
  root.registerId(string_map);
  std::string ret;
  ret.reserve(1024);
  ret.push_back(SeriTypeToChar(SeriType::REPLACE));
  auto string_map_str = string_map.Serialize();
  seriLength(string_map_str.size(), ret);
  ret.append(string_map_str);
  root.valueSeri(ret, &string_map);
  return ret;
}

template <SeriType type = SeriType::NORMAL>
inline std::string Serialize(std::unique_ptr<Object>&& obj) {
  if constexpr (type == SeriType::NORMAL) {
    return SerializeNormal(std::move(obj));
  } else {
    return SerializeReplace(std::move(obj));
  }
}

inline std::unique_ptr<Object> Parse(const std::string& content, bool parse_ref = false) {
  if (content.empty() == true) {
    return nullptr;
  }
  char c = content[0];
  InnerWrapper wrapper(content);
  wrapper.skip(1);
  size_t offset = 1;
  std::unique_ptr<Object> root;
  if (parse_ref == false) {
    root = ValueFactory<Map>();
  } else {
    root = ValueFactory<MapView>();
  }
  if (c == SeriTypeToChar(SeriType::NORMAL)) {
    root->valueParse(wrapper, offset, parse_ref, nullptr);
    if (wrapper.outOfRange() || offset != content.size()) {
      return nullptr;
    }
  } else if (c == SeriTypeToChar(SeriType::REPLACE)) {
    size_t length = parseLength(wrapper, offset);
    std::string_view string_map_str(wrapper.curAddr(), length);
    wrapper.skip(length);
    offset += length;

    StringMap sm = StringMap::Construct(string_map_str);
    root->valueParse(wrapper, offset, parse_ref, &sm);
    if (wrapper.outOfRange() || offset != content.size()) {
      return nullptr;
    }
  } else {
    assert(false);
    return nullptr;
  }
  return parse_ref == false ? AsMap(root)->Get("root") : static_cast<MapView*>(root.get())->Get("root");
}

}  // namespace bridge