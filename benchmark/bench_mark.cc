#include <chrono>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "benchmark/benchmark.h"
#include "bridge/object.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

using namespace rapidjson;

std::vector<std::unordered_map<std::string, std::string>> initInfo() {
  std::vector<std::unordered_map<std::string, std::string>> ret;
  std::vector<std::string> key_set = {
      "{get_file_from_db}", "{update_timestamp}", "{post_to_db}", "{delete_by_timestamp}", "{custom_opration}",
  };

  std::vector<std::string> value_set = {
      "Barbara", "Elizabeth", "Katharine", "Judy", "Doris", "Rudy", "Amanda",
  };

  for (int i = 0; i < 1000; ++i) {
    std::unordered_map<std::string, std::string> tmp;
    for (int j = 0; j < 1000; ++j) {
      tmp[key_set[rand() % key_set.size()] + std::to_string(j)] = value_set[rand() % value_set.size()];
    }
    ret.push_back(std::move(tmp));
  }
  return ret;
}

std::vector<std::unordered_map<std::string, uint64_t>> initInfo2() {
  std::vector<std::unordered_map<std::string, uint64_t>> ret;
  std::vector<std::string> key_set = {
      "get_file_from_db", "update_timestamp", "post_to_db", "delete_by_timestamp", "custom_opration",
  };
  for (int i = 0; i < 1000; ++i) {
    std::unordered_map<std::string, uint64_t> tmp;
    for (int j = 0; j < 1000; ++j) {
      tmp[key_set[rand() % key_set.size()] + std::to_string(j)] = rand() % 10000;
    }
    ret.push_back(std::move(tmp));
  }
  return ret;
}

static std::vector<std::unordered_map<std::string, std::string>> info = initInfo();

static std::vector<std::unordered_map<std::string, uint64_t>> info2 = initInfo2();

std::string benchmark_rapidjson() {
  Document d;
  d.SetArray();
  for (auto& each : info) {
    Value obj;
    obj.SetObject();
    for (auto& each_record : each) {
      const std::string& key = each_record.first;
      const std::string& value = each_record.second;
      obj.AddMember(StringRef(&key[0], key.size()), StringRef(&value[0], value.size()), d.GetAllocator());
    }
    d.PushBack(obj, d.GetAllocator());
  }
  for (auto& each : info2) {
    Value obj;
    obj.SetObject();
    for (auto& each_record : each) {
      const std::string& key = each_record.first;
      uint64_t id = each_record.second;
      Value idder;
      idder.SetUint64(id);
      obj.AddMember(StringRef(&key[0], key.size()), idder, d.GetAllocator());
    }
    d.PushBack(obj, d.GetAllocator());
  }
  StringBuffer buf;
  Writer<StringBuffer> w(buf);
  d.Accept(w);
  const char* output = buf.GetString();
  std::string new_buf(output, buf.GetSize());
  return new_buf;
}

std::string rapidjson_str = benchmark_rapidjson();

void rapidjson_parse() {
  Document doc;
  char* ptr = new char[rapidjson_str.size() + 1];
  ptr[rapidjson_str.size()] = 0;
  memcpy((void*)ptr, &rapidjson_str[0], rapidjson_str.size());
  doc.ParseInsitu(ptr);
  delete ptr;
}

static void BM_Rapidjson(benchmark::State& state) {
  for (auto _ : state) benchmark_rapidjson();
}

static void BM_Rapidjson_Parse(benchmark::State& state) {
  for (auto _ : state) rapidjson_parse();
}

template <bridge::SeriType seri_type>
std::string benchmark_bridge() {
  auto array = bridge::array();
  for (auto& each : info) {
    auto map = bridge::map_view();
    for (auto& each_record : each) {
      const std::string& key = each_record.first;
      const std::string& value = each_record.second;
      map->Insert(key, bridge::data_view(value));
    }
    array->Insert(std::move(map));
  }
  for (auto& each : info2) {
    auto map = bridge::map_view();
    for (auto& each_record : each) {
      const std::string& key = each_record.first;
      const uint64_t& id = each_record.second;
      map->Insert(key, bridge::data(id));
    }
    array->Insert(std::move(map));
  }
  std::string ret = bridge::Serialize<seri_type>(std::move(array));
  // bridge::ClearResource();
  return ret;
}

const std::string& GetBridgeStr() {
  static std::string obj = benchmark_bridge<bridge::SeriType::NORMAL>();
  return obj;
}

const std::string& GetBridgeReplaceStr() {
  static std::string obj = benchmark_bridge<bridge::SeriType::REPLACE>();
  return obj;
}

void bridge_parse() {
  bridge::ParseOption po;
  po.parse_ref = true;
  auto root = bridge::Parse(GetBridgeStr(), po);
  // bridge::ClearResource();
}

void bridge_parse_replace() {
  bridge::ParseOption po;
  po.parse_ref = true;
  auto root = bridge::Parse(GetBridgeReplaceStr(), po);
  // bridge::ClearResource();
}

static void BM_Bridge(benchmark::State& state) {
  for (auto _ : state) benchmark_bridge<bridge::SeriType::NORMAL>();
}

static void BM_Bridge_Replace(benchmark::State& state) {
  for (auto _ : state) benchmark_bridge<bridge::SeriType::REPLACE>();
}

static void BM_Bridge_Parse(benchmark::State& state) {
  for (auto _ : state) bridge_parse();
}

static void BM_Bridge_Parse_Replace(benchmark::State& state) {
  for (auto _ : state) bridge_parse_replace();
}

BENCHMARK(BM_Rapidjson);
BENCHMARK(BM_Rapidjson_Parse);
BENCHMARK(BM_Bridge);
BENCHMARK(BM_Bridge_Parse);
BENCHMARK(BM_Bridge_Parse_Replace);
BENCHMARK(BM_Bridge_Replace);

BENCHMARK_MAIN();
