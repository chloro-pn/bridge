#include <chrono>
#include <cmath>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "bridge/async_executor/executor.h"
#include "bridge/async_executor/thread_pool.h"
#include "bridge/object.h"
#include "bridge/util.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

/*
 * 测试环境Ubuntu20.04 + g++11 + 6core
 */

/*
 * 开启字典映射：包压缩了50%左右，解析速度为rapidjson的45%左右
 * construct use 238 ms
 * serialize use 414 ms, size == 71626070
 * rapidjson construct use 118 ms
 * rapidjson serialize use 571 ms, size == 132991217
 * -----begin to parse------
 * bridge's coroutine parse use 146 ms
 * bridge's normal parse use 476 ms
 * rapidjson parse use 324 m
 */

/*
 * 关闭字典映射：包压缩了82%左右，解析速度为rapidjson的39%左右
 * construct use 225 ms
 * serialize use 335 ms, size == 109223570
 * rapidjson construct use 128 ms
 * rapidjson serialize use 607 ms, size == 132991217
 * -----begin to parse------
 * bridge's coroutine parse use 121 ms
 * bridge's normal parse use 477 ms
 * rapidjson parse use 308 ms
 */
struct MetricInfo {
  std::unordered_map<std::string, std::string> dimension_;
  uint64_t time_point_;
  std::vector<float> values_;
  std::vector<std::string> metric_names_;
  std::vector<uint32_t> metric_types_;

  bridge::unique_ptr<bridge::MapView> ToBridge(bridge::BridgePool& bp) const {
    auto ret = bp.map_view();
    auto dimensions = bp.map_view();
    for (auto& each : dimension_) {
      dimensions->Insert(each.first, bp.data_view(each.second));
    }
    ret->Insert("dimension", std::move(dimensions));
    ret->Insert("timepoint", bp.data_view(time_point_));

    auto name = bp.array();
    for (auto& each : metric_names_) {
      name->Insert(bp.data_view(each));
    }
    ret->Insert("metric_names", std::move(name));

    auto type = bp.array();
    for (auto& each : metric_types_) {
      type->Insert(bp.data_view(each));
    }
    ret->Insert("metric_types", std::move(type));

    auto values = bp.array();
    for (auto& each : values_) {
      values->Insert(bp.data_view(each));
    }
    ret->Insert("values", std::move(values));
    return ret;
  }
};

std::vector<MetricInfo> initMetricInfo() {
  std::vector<MetricInfo> ret;
  std::vector<std::string> tag_key = {"country_and_city", "network_type", "hash_value"};
  std::unordered_map<std::string, std::vector<std::string>> dimen_info;

  for (int i = 0; i < 10; ++i) {
    int v_count = rand() % 10;
    std::vector<std::string> tag_v;
    for (int j = 0; j < v_count; ++j) {
      tag_v.push_back("tag_value000" + std::to_string(j));
    }
    dimen_info["tag_key000" + std::to_string(i)] = tag_v;
  }
  std::vector<std::string> tmp;
  // 假设维度组合共有2w种
  for (int i = 0; i < 20000; ++i) {
    MetricInfo mi;
    for (int i = 0; i < 200; ++i) {
      mi.metric_names_.push_back(std::string("metric") + std::to_string(i));
      mi.metric_types_.push_back(rand() % 6);
      mi.time_point_ = time(nullptr);
      mi.values_.push_back(((rand() % 100) * 17) / 100.0);
      // 为每个维度赋一个值
      for (auto& each : dimen_info) {
        mi.dimension_[each.first] = each.second[rand() % each.second.size()];
      }
    }
    ret.push_back(mi);
  }
  return ret;
}

bridge::unique_ptr<bridge::Array> Construct(const std::vector<MetricInfo>& info, bridge::BridgePool& bp) {
  auto array = bp.array();
  for (auto& each : info) {
    auto map_info = each.ToBridge(bp);
    array->Insert(std::move(map_info));
  }
  return array;
}

using namespace rapidjson;

std::string benchmark_rapidjson(const std::vector<MetricInfo>& info) {
  bridge::Timer timer;
  timer.Start();
  Document d;
  d.SetArray();
  for (auto& each : info) {
    Value tmp(kObjectType);

    Value names(kArrayType);
    for (auto& name : each.metric_names_) {
      names.PushBack(StringRef(&name[0], name.size()), d.GetAllocator());
    }
    tmp.AddMember(StringRef("metric_names"), names, d.GetAllocator());

    Value types(kArrayType);
    for (auto& type : each.metric_types_) {
      types.PushBack(Value(type), d.GetAllocator());
    }
    tmp.AddMember(StringRef("metric_types"), types, d.GetAllocator());

    Value dimens(kObjectType);
    for (auto& kv : each.dimension_) {
      dimens.AddMember(StringRef(&kv.first[0], kv.first.size()), StringRef(&kv.second[0], kv.second.size()),
                       d.GetAllocator());
    }
    tmp.AddMember(StringRef("dimension"), dimens, d.GetAllocator());
    tmp.AddMember(StringRef("timepoint"), Value(each.time_point_), d.GetAllocator());
    Value values(kArrayType);
    for (auto& v : each.values_) {
      values.PushBack(Value(v), d.GetAllocator());
    }
    tmp.AddMember(StringRef("values"), values, d.GetAllocator());
    d.PushBack(tmp, d.GetAllocator());
  }
  auto tmp = timer.End();
  std::cout << "rapidjson construct use " << tmp << " ms" << std::endl;
  timer.Start();
  StringBuffer buf;
  Writer<StringBuffer> w(buf);
  d.Accept(w);
  const char* output = buf.GetString();
  std::string new_buf(output, buf.GetSize());
  tmp = timer.End();
  std::cout << "rapidjson serialize use " << tmp << " ms"
            << ", size == " << new_buf.size() << std::endl;
  return new_buf;
}

int main() {
  bridge::ThreadPool::GetOrConstructInstance(6);
  bridge::Timer timer;
  bridge::BridgePool bp;
  auto info = initMetricInfo();
  timer.Start();
  auto root = Construct(info, bp);
  auto tmp = timer.End();
  std::cout << "construct use " << tmp << " ms" << std::endl;
  timer.Start();
  std::string content = bridge::Serialize<bridge::SeriType::NORMAL>(std::move(root), bp);
  tmp = timer.End();
  std::cout << "serialize use " << tmp << " ms"
            << ", size == " << content.size() << std::endl;

  std::string rcontent = benchmark_rapidjson(info);

  std::cout << "-----begin to parse------" << std::endl;

  bridge::ParseOption po;
  timer.Start();
  po.parse_ref = true;
  po.type = bridge::SchedulerType::Coroutine;
  po.worker_num_ = 4;
  auto root2 = bridge::Parse(content, bp, po);
  bp.Clear();
  tmp = timer.End();
  bridge::ObjectWrapper w(root2.get());
  std::cout << "bridge's coroutine parse use " << tmp << " ms" << std::endl;

  timer.Start();
  po.parse_ref = true;
  po.type = bridge::SchedulerType::Normal;
  auto root1 = bridge::Parse(content, bp, po);
  assert(root1->GetType() == bridge::ObjectType::Array);
  tmp = timer.End();
  std::cout << "bridge's normal parse use " << tmp << " ms" << std::endl;

  timer.Start();
  Document doc;

  doc.Parse(rcontent.c_str());
  tmp = timer.End();
  std::cout << "rapidjson parse use " << tmp << " ms" << std::endl;
}