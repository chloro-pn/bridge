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
      "[Barbara]", "[Elizabeth]", "[Katharine]", "[Judy]", "[Doris]", "[Rudy]", "[Amanda]",
  };

  for (int i = 0; i < 1000; ++i) {
    std::unordered_map<std::string, std::string> tmp;

    tmp[key_set[i % key_set.size()]] = value_set[i % value_set.size()];
    ret.push_back(std::move(tmp));
  }
  return ret;
}

static std::vector<std::unordered_map<std::string, std::string>> info = initInfo();

std::vector<std::unordered_map<std::string, uint64_t>> initInfo2() {
  std::vector<std::unordered_map<std::string, uint64_t>> ret;
  std::vector<std::string> key_set = {
      "get_file_from_db", "update_timestamp", "post_to_db", "delete_by_timestamp", "custom_opration",
  };
  for (int i = 0; i < 1000; ++i) {
    std::unordered_map<std::string, uint64_t> tmp;

    tmp[key_set[i % key_set.size()]] = i;
    ret.push_back(std::move(tmp));
  }
  return ret;
}

static std::vector<std::unordered_map<std::string, uint64_t>> info2 = initInfo2();

std::string benchmark_rapidjson() {
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
  std::string new_buf(output, buf.GetSize());
  return new_buf;
}

std::string rapidjson_str = benchmark_rapidjson();

void rapidjson_parse() {
  Document doc;
  doc.Parse(&rapidjson_str[0], rapidjson_str.size());
}

static void BM_Rapidjson(benchmark::State& state) {
  for (auto _ : state) benchmark_rapidjson();
}

static void BM_Rapidjson_Parse(benchmark::State& state) {
  for (auto _ : state) rapidjson_parse();
}

template<bridge::SeriType type>
std::string benchmark_bridge() {
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
  std::string ret = bridge::Serialize<type>(std::move(array));
  return ret;
}

std::string bridge_str = benchmark_bridge<bridge::SeriType::NORMAL>();
std::string bridge_replace_str = benchmark_bridge<bridge::SeriType::REPLACE>();

void bridge_parse() {
  auto root = bridge::Parse(bridge_str, true);
}

void bridge_parse_replace() {
  auto root = bridge::Parse(bridge_replace_str, true);
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
