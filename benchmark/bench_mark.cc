#include <chrono>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "bridge/object.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

using namespace rapidjson;

class Timer {
 public:
  Timer() {}
  void Start() { start_ = std::chrono::system_clock::now(); }

  double End() {
    auto end = std::chrono::system_clock::now();
    auto use_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start_);
    return use_ms.count();
  }

 private:
  std::chrono::time_point<std::chrono::system_clock> start_;
};

std::vector<std::unordered_map<std::string, std::string>> initInfo() {
  std::vector<std::unordered_map<std::string, std::string>> ret;
  std::vector<std::string> key_set = {
      "get_file_from_db", "update_timestamp", "post_to_db", "delete_by_timestamp", "custom_opration",
  };

  std::vector<std::string> value_set = {
      "Barbara", "Elizabeth", "Katharine", "Judy", "Doris", "Rudy", "Amanda",
  };

  for (int i = 0; i < 300000; ++i) {
    std::unordered_map<std::string, std::string> tmp;

    tmp[key_set[i % key_set.size()]] = value_set[i % value_set.size()];
    ret.push_back(std::move(tmp));
  }
  return ret;
}

std::vector<std::unordered_map<std::string, uint64_t>> initInfo2() {
  std::vector<std::unordered_map<std::string, uint64_t>> ret;
  std::vector<std::string> key_set = {
      "get_file_from_db", "update_timestamp", "post_to_db", "delete_by_timestamp", "custom_opration",
  };
  for (int i = 0; i < 300000; ++i) {
    std::unordered_map<std::string, uint64_t> tmp;

    tmp[key_set[i % key_set.size()]] = i;
    ret.push_back(std::move(tmp));
  }
  return ret;
}

int main() {
  Timer timer;
  auto info = initInfo();
  auto info2 = initInfo2();
  timer.Start();
  Document d;
  d.SetArray();
  for (auto& each : info) {
    const std::string& key = each.begin()->first;
    const std::string& value = each.begin()->second;
    Value obj;
    obj.SetObject();
    obj.AddMember(StringRef(&key[0], key.size()), StringRef(&value[0], value.size()), d.GetAllocator());
    d.PushBack(obj, d.GetAllocator());
  }
  for (auto& each : info2) {
    const std::string& key = each.begin()->first;
    uint64_t id = each.begin()->second;
    Value obj;
    obj.SetObject();
    Value idder;
    idder.SetUint64(id);
    obj.AddMember(StringRef(&key[0], key.size()), idder, d.GetAllocator());
    d.PushBack(obj, d.GetAllocator());
  }
  StringBuffer buf;
  Writer<StringBuffer> w(buf);
  d.Accept(w);
  const char* output = buf.GetString();
  auto tmp = timer.End();
  std::cout << "rapidjson use " << tmp << " ms, size = " << buf.GetSize() << std::endl;
  std::string new_buf(output, buf.GetSize());
  timer.Start();
  Document parsed;
  parsed.ParseInsitu(&new_buf[0]);
  tmp = timer.End();
  std::cout << "rapidjson parse use " << tmp << " ms" << std::endl;

  timer.Start();
  auto array = bridge::array();
  for (auto& each : info) {
    const std::string& key = each.begin()->first;
    const std::string& value = each.begin()->second;
    auto map = bridge::map_view();
    map->Insert(key, bridge::data_view(value));
    array->Insert(std::move(map));
  }
  for (auto& each : info2) {
    const std::string& key = each.begin()->first;
    const uint64_t& id = each.begin()->second;
    auto map = bridge::map_view();
    map->Insert(key, bridge::data(id));
    array->Insert(std::move(map));
  }
  std::string ret = bridge::Serialize<bridge::SeriType::NORMAL>(std::move(array));
  bridge::ClearResource();
  tmp = timer.End();
  std::cout << "bridge use " << tmp << " ms, size = " << ret.size() << std::endl;
  timer.Start();
  auto root = bridge::Parse(ret, true);
  bridge::ClearResource();
  tmp = timer.End();
  std::cout << "bridge parse use " << tmp << " ms" << std::endl;
  return 0;
}