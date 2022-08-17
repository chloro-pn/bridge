#include "async_simple/coro/Lazy.h"
#include "async_simple/coro/SyncAwait.h"
#include "bridge/async_executor/executor.h"
#include "bridge/async_executor/task.h"
#include "gtest/gtest.h"

using namespace async_simple::coro;
using namespace bridge;

Lazy<int> Add() { co_return 1 + 2; }

TEST(async_simple, basic) {
  int num = syncAwait(Add());
  EXPECT_EQ(num, 3);
}

TEST(async_simple, task) {
  bridge::BridgeExecutor e(4);
  EXPECT_EQ(e.name(), "bridge_simple_threadpool");
  SplitInfo si;
  StringMap sm;
  Map arr;
  auto a1 = array();
  for (size_t i = 0; i < 10000; ++i) {
    a1->Insert(data("hello" + std::to_string(i)));
  }
  arr.Insert("arr1", std::move(a1));

  auto a2 = array();
  for (size_t i = 0; i < 10000; ++i) {
    a2->Insert(data(uint32_t(i)));
  }
  arr.Insert("arr2", std::move(a2));

  std::string ret;
  arr.valueSeri(ret, &sm, &si);
  async_simple::coro::RescheduleLazy Scheduled = ParseMap(ret, si, &sm, 0, true).via(&e);
  auto new_arr = syncAwait(Scheduled);
  ObjectWrapper w(new_arr.v.get());
  EXPECT_EQ(w.GetType(), ObjectType::Map);
  EXPECT_EQ(w.Size(), 2);
  EXPECT_EQ(w["arr1"].GetType(), ObjectType::Array);
  EXPECT_EQ(w["arr1"].Size(), 10000);
  EXPECT_EQ(w["arr2"].GetType(), ObjectType::Array);
  for (int i = 0; i < 10000; ++i) {
    EXPECT_EQ(w["arr1"][i].Get<std::string>().value(), "hello" + std::to_string(i));
    EXPECT_EQ(w["arr2"][i].Get<uint32_t>().value(), i);
  }
}