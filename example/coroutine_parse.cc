#include <chrono>
#include <cmath>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "bridge/async_executor/executor.h"
#include "bridge/async_executor/task.h"
#include "bridge/async_executor/thread_pool.h"
#include "bridge/object.h"
#include "bridge/util.h"

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

template <typename T, typename T2>
bridge::unique_ptr<bridge::Array> Construct(const T& info, const T2& info2) {
  auto array = bridge::array();
  for (auto& each_record : info) {
    auto each_msg = bridge::map_view();
    for (auto& each : each_record) {
      const std::string& key = each.first;
      const std::string& value = each.second;
      each_msg->Insert(key, bridge::data_view(value));
      // array->Insert(bridge::data_view(key));
    }
    array->Insert(std::move(each_msg));
  }
  for (auto& each_record : info2) {
    auto each_msg = bridge::map_view();
    for (auto& each : each_record) {
      const std::string& key = each.first;
      uint64_t id = each.second;
      each_msg->Insert(key, bridge::data_view(id));
      // array->Insert(bridge::data_view(id));
    }
    array->Insert(std::move(each_msg));
  }
  return array;
}

int main() {
  bridge::ThreadPool::GetOrConstructInstance(3);
  bridge::Timer timer;
  auto info = initInfo();
  auto info2 = initInfo2();
  timer.Start();
  auto root = Construct(info, info2);
  auto tmp = timer.End();
  std::cout << "construct use " << tmp << " ms" << std::endl;
  timer.Start();
  std::string content = bridge::Serialize<bridge::SeriType::NORMAL>(std::move(root));
  tmp = timer.End();
  std::cout << "serialize use " << tmp << " ms" << std::endl;

  bridge::ParseOption po;
  timer.Start();
  po.parse_ref = true;
  po.type = bridge::SchedulerType::Coroutine;
  po.worker_num_ = 4;
  auto root2 = bridge::Parse(content, po);
  tmp = timer.End();
  bridge::ObjectWrapper w(root2.get());
  std::cout << "bridge's coroutine parse use " << tmp << " ms" << std::endl;

  timer.Start();
  po.parse_ref = true;
  po.type = bridge::SchedulerType::Normal;
  auto root1 = bridge::Parse(content, po);
  assert(root1->GetType() == bridge::ObjectType::Array);
  tmp = timer.End();
  std::cout << "bridge's normal parse use " << tmp << " ms" << std::endl;
}