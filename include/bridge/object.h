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
#include "bridge/type_trait.h"

namespace bridge {

class Object {
 public:
  explicit Object(ObjectType type) : type_(type) {}

  ObjectType GetType() const { return type_; }

  virtual ~Object() = default;

  template <typename Inner>
  void valueParse(const Inner& inner, size_t& offset);

  template <typename Outer>
  void valueSeri(Outer& outer) const;

 private:
  ObjectType type_;
};

std::unique_ptr<Object> ObjectFactory(ObjectType type);

class Data : public Object {
 public:
  // 默认构造函数，Data处于不合法状态
  Data() : Object(ObjectType::Data), data_type_(BRIDGE_INVALID) {}

  template <typename T>
  explicit Data(T&& obj) : Object(ObjectType::Data) {
    data_type_ = DataTypeTrait<T>::dt;
    serialize(std::forward<T>(obj), data_);
  }

  template <typename T>
  std::optional<T> Get() const {
    static_assert(NoRefNoPointer<T>::value,
                  "get() method should not return ref or pointer type (including <T* const> and <const T*>)");
    // 当处于不合法状态时，总是返回空的optional
    if (DataTypeTrait<T>::dt != data_type_) {
      return std::optional<T>();
    }
    return std::optional(parse<T>(data_));
  }

  const char* GetRaw() const {
    if (data_.empty() == true) {
      return nullptr;
    }
    return &data_[0];
  }

  template <typename Inner>
  void valueParse(const Inner& inner, size_t& offset) {
    uint64_t size = parseLength(inner, offset);
    if (inner.outOfRange()) {
      return;
    }
    data_type_ = parseDataType(inner, offset);
    if (inner.outOfRange()) {
      return;
    }
    assert(size > 1);
    size = size - 1;
    data_.resize(size);
    memcpy(&data_[0], inner.curAddr(), size);
    inner.skip(size);
    if (inner.outOfRange()) {
      return;
    }
    offset += size;
  }

  template <typename Outer>
  void valueSeri(Outer& outer) const {
    assert(data_type_ != BRIDGE_INVALID);
    uint32_t length = data_.size() + 1;
    seriLength(length, outer);
    seriDataType(data_type_, outer);
    outer.append(&data_[0], data_.size());
  }

 private:
  std::vector<char> data_;
  uint8_t data_type_;
};

class Array : public Object {
 public:
  Array() : Object(ObjectType::Array) {}

  Array(std::vector<std::unique_ptr<Object>>&& values) : Object(ObjectType::Array), objects_(std::move(values)) {}

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
  void valueParse(const Inner& inner, size_t& offset) {
    objects_.clear();
    parseLength(inner, offset);
    if (inner.outOfRange()) {
      return;
    }
    uint64_t count = parseLength(inner, offset);
    for (uint64_t i = 0; i < count; ++i) {
      ObjectType type = parseObjectType(inner, offset);
      if (inner.outOfRange()) {
        return;
      }
      auto v = ObjectFactory(type);
      v->valueParse(inner, offset);
      if (inner.outOfRange()) {
        return;
      }
      objects_.push_back(std::move(v));
    }
  }

  template <typename Outer>
  void valueSeri(Outer& outer) const {
    std::string tmp;
    uint32_t count = objects_.size();
    seriLength(count, tmp);
    for (auto& each : objects_) {
      seriObjectType(each->GetType(), tmp);
      each->valueSeri(tmp);
    }
    seriLength(tmp.size(), outer);
    outer.append(tmp);
  }

 private:
  std::vector<std::unique_ptr<Object>> objects_;
};

class Map : public Object {
 public:
  Map() : Object(ObjectType::Map) {}

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

  template <typename Inner>
  void valueParse(const Inner& inner, size_t& offset) {
    objects_.clear();
    parseLength(inner, offset);
    if (inner.outOfRange()) {
      return;
    }
    uint64_t count = parseLength(inner, offset);
    if (inner.outOfRange()) {
      return;
    }
    for (uint64_t i = 0; i < count; ++i) {
      // parse key
      uint64_t key_length = parseLength(inner, offset);
      if (inner.outOfRange()) {
        return;
      }
      std::string key;
      key.resize(key_length);
      memcpy(&key[0], inner.curAddr(), key_length);
      inner.skip(key_length);
      if (inner.outOfRange()) {
        return;
      }
      offset += key_length;
      // parse value
      ObjectType type = parseObjectType(inner, offset);
      if (inner.outOfRange()) {
        return;
      }
      auto v = ObjectFactory(type);
      v->valueParse(inner, offset);
      if (inner.outOfRange()) {
        return;
      }
      objects_[key] = std::move(v);
    }
  }

  template <typename Outer>
  void valueSeri(Outer& outer) const {
    std::string tmp;
    uint32_t count = objects_.size();
    seriLength(count, tmp);
    for (auto& each : objects_) {
      // key seri
      uint32_t key_length = each.first.size();
      seriLength(key_length, tmp);
      tmp.append(each.first);
      seriObjectType(each.second->GetType(), tmp);
      each.second->valueSeri(tmp);
    }
    seriLength(tmp.size(), outer);
    outer.append(tmp);
  }

 private:
  std::unordered_map<std::string, std::unique_ptr<Object>> objects_;
};

template <typename Inner>
void Object::valueParse(const Inner& inner, size_t& offset) {
  if (type_ == ObjectType::Map) {
    static_cast<Map*>(this)->valueParse(inner, offset);
  } else if (type_ == ObjectType::Array) {
    static_cast<Array*>(this)->valueParse(inner, offset);
  } else if (type_ == ObjectType::Data) {
    static_cast<Data*>(this)->valueParse(inner, offset);
  }
}

template <typename Outer>
void Object::valueSeri(Outer& outer) const {
  if (type_ == ObjectType::Map) {
    static_cast<const Map*>(this)->valueSeri(outer);
  } else if (type_ == ObjectType::Array) {
    static_cast<const Array*>(this)->valueSeri(outer);
  } else if (type_ == ObjectType::Data) {
    static_cast<const Data*>(this)->valueSeri(outer);
  }
}

template <typename T, typename... Args>
std::unique_ptr<T> ValueFactory(Args&&... args) {
  return std::make_unique<T>(std::forward<Args>(args)...);
}

std::unique_ptr<Object> ObjectFactory(ObjectType type) {
  if (type == ObjectType::Data) {
    return ValueFactory<Data>();
  } else if (type == ObjectType::Map) {
    return ValueFactory<Map>();
  } else if (type == ObjectType::Array) {
    return ValueFactory<Array>();
  } else {
    return nullptr;
  }
}

class ObjectWrapper {
 public:
  ObjectWrapper() : obj_(nullptr) {}

  explicit ObjectWrapper(const Object* obj) : obj_(obj) {}

  ObjectWrapper operator[](const std::string& key) const {
    if (obj_ == nullptr || obj_->GetType() != ObjectType::Map) {
      return ObjectWrapper(nullptr);
    }
    return ObjectWrapper(static_cast<const Map*>(obj_)->operator[](key));
  }

  ObjectWrapper operator[](size_t n) const {
    if (obj_ == nullptr || obj_->GetType() != ObjectType::Array) {
      return ObjectWrapper(nullptr);
    }
    return ObjectWrapper(static_cast<const Array*>(obj_)->operator[](n));
  }

  std::optional<size_t> Size() const {
    if (obj_ == nullptr || (obj_->GetType() != ObjectType::Map || obj_->GetType() != ObjectType::Array)) {
      return std::optional<size_t>();
    }
    if (obj_->GetType() == ObjectType::Map) {
      return static_cast<const Map*>(obj_)->Size();
    }
    return static_cast<const Array*>(obj_)->Size();
  }

  template <typename T>
  std::optional<T> Get() const {
    if (obj_ == nullptr || obj_->GetType() != ObjectType::Data) {
      return std::optional<T>(nullptr);
    }
    return static_cast<const Data*>(obj_)->Get<T>();
  }

  std::optional<const char*> GetRaw() const {
    if (obj_ == nullptr || obj_->GetType() != ObjectType::Data) {
      return std::optional<const char*>(nullptr);
    }
    return static_cast<const Data*>(obj_)->GetRaw();
  }

  bool Empty() const { return obj_ == nullptr; }

 private:
  const Object* obj_;
};

inline Map* AsMap(std::unique_ptr<Object>& obj) {
  if (obj->GetType() != ObjectType::Map) {
    return nullptr;
  }
  return static_cast<Map*>(obj.get());
}

inline Array* AsArray(std::unique_ptr<Object>& obj) {
  if (obj->GetType() != ObjectType::Array) {
    return nullptr;
  }
  return static_cast<Array*>(obj.get());
}

inline Data* AsData(std::unique_ptr<Object>& obj) {
  if (obj->GetType() != ObjectType::Data) {
    return nullptr;
  }
  return static_cast<Data*>(obj.get());
}

std::string Serialize(std::unique_ptr<Object>&& obj) {
  Map root;
  root.Insert("root", std::move(obj));
  std::string ret;
  root.valueSeri(ret);
  return ret;
}

std::unique_ptr<Object> Parse(const std::string& content) {
  auto root = ValueFactory<Map>();
  size_t offset = 0;
  InnerWrapper wrapper(content);
  root->valueParse(wrapper, offset);
  if (wrapper.outOfRange() || offset != content.size()) {
    return nullptr;
  }
  return root->Get("root");
}

}  // namespace bridge