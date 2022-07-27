#pragma once

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "bridge/data_type.h"
#include "bridge/object.h"

namespace bridge {

class Pattern {
 public:
  virtual bool Match(const Object* obj) = 0;

  ~Pattern() {}

 private:
};

class MapPattern : public Pattern {
 public:
  MapPattern() : set_size_pattern_(false), size_pattern_(0) {}

  bool Match(const Object* obj) override {
    if (obj->GetType() != ObjectType::Map) {
      return false;
    }
    if (obj->IsRefType() == false) {
      const Map* map = AsMap(obj);
      if (set_size_pattern_) {
        if (size_pattern_ != map->Size()) {
          return false;
        }
      }
      for (const auto& each : match_kv_) {
        const Object* value = map->operator[](each.first);
        if (value == nullptr) {
          return false;
        }
        bool child_match = each.second->Match(value);
        if (child_match == false) {
          return false;
        }
      }
    } else {
      const MapView* map_view = static_cast<const MapView*>(obj);
      if (set_size_pattern_) {
        if (size_pattern_ != map_view->Size()) {
          return false;
        }
      }
      for (const auto& each : match_kv_) {
        const Object* value = map_view->operator[](each.first);
        if (value == nullptr) {
          return false;
        }
        bool child_match = each.second->Match(value);
        if (child_match == false) {
          return false;
        }
      }
    }
    return true;
  }

  void setPattern(const std::string& key, std::unique_ptr<Pattern>&& pattern) { match_kv_[key] = std::move(pattern); }

  void removePattern(const std::string& key) { match_kv_.erase(key); }

  void setSize(size_t n) {
    size_pattern_ = n;
    set_size_pattern_ = true;
  }

 private:
  std::unordered_map<std::string, std::unique_ptr<Pattern>> match_kv_;
  // kv数量限制
  bool set_size_pattern_;
  size_t size_pattern_;
};

class ArrayPattern : public Pattern {
 public:
  ArrayPattern() : set_length_pattern_(false) {}

  bool Match(const Object* obj) override {
    if (obj->GetType() != ObjectType::Array) {
      return false;
    }
    const Array* arr = AsArray(obj);
    if (set_length_pattern_) {
      if (objects_.size() != arr->Size()) {
        return false;
      }
    }
    for (size_t i = 0; i < objects_.size(); ++i) {
      const Object* value = arr->operator[](i);
      if (value == nullptr) {
        return false;
      }
      bool value_match = objects_[i]->Match(value);
      if (value_match == false) {
        return false;
      }
    }
    return true;
  }

  ArrayPattern(size_t n) : set_length_pattern_(true) { objects_.resize(n); }

  void setNthPattern(size_t n, std::unique_ptr<Pattern>&& pattern) {
    if (set_length_pattern_ && n < objects_.size()) {
      objects_[n] = std::move(pattern);
    }
  }

 private:
  std::vector<std::unique_ptr<Pattern>> objects_;
  bool set_length_pattern_;
};

class DataPattern : public Pattern {
 public:
  DataPattern() : set_data_type_pattern_(false) {}
  DataPattern(uint8_t data_type) : data_type_(data_type), set_data_type_pattern_(true) {}

  bool Match(const Object* obj) override {
    if (obj->GetType() != ObjectType::Data) {
      return false;
    }
    if (obj->IsRefType() == false) {
      const Data* data = AsData(obj);
      if (set_data_type_pattern_ == true) {
        if (data_type_ != data->GetDataType()) {
          return false;
        }
      }
    } else {
      const DataView* data = static_cast<const DataView*>(obj);
      if (set_data_type_pattern_ == true) {
        if (data_type_ != data->GetDataType()) {
          return false;
        }
      }
    }
    return true;
  }

 private:
  uint8_t data_type_;
  bool set_data_type_pattern_;
};

class OrPattern : public Pattern {
 public:
  void PushPattern(std::unique_ptr<Pattern>&& pattern) { patterns_.push_back(std::move(pattern)); }

  bool Match(const Object* obj) override {
    for (auto& each : patterns_) {
      if (each->Match(obj) == true) {
        return true;
      }
    }
    return false;
  }

 private:
  std::vector<std::unique_ptr<Pattern>> patterns_;
};

class AndPattern : public Pattern {
 public:
  void PushPattern(std::unique_ptr<Pattern>&& pattern) { patterns_.push_back(std::move(pattern)); }

  bool Match(const Object* obj) override {
    for (auto& each : patterns_) {
      if (each->Match(obj) == false) {
        return false;
      }
    }
    return true;
  }

 private:
  std::vector<std::unique_ptr<Pattern>> patterns_;
};

}  // namespace bridge