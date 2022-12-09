#include "async_simple/coro/Lazy.h"
#include "async_simple/coro/Collect.h"
#include "async_simple/coro/SyncAwait.h"
#include "bridge/async_executor/executor.h"
#include "bridge/object.h"
#include "gtest/gtest.h"

#include <thread>
#include <iostream>

using namespace async_simple::coro;
using namespace bridge;

Lazy<int> Add() { co_return 1 + 2; }

TEST(async_simple, basic) {
  int num = syncAwait(Add());
  EXPECT_EQ(num, 3);
}

Lazy<size_t> cor(size_t sec) {
  std::this_thread::sleep_for(std::chrono::seconds(sec));
  std::cout << " cor " << sec << " return" << std::endl;
  co_return sec;
}

Lazy<void> AnyTest(size_t& idx, size_t& res, BridgeExecutor& e) {
  std::vector<RescheduleLazy<size_t>> cors;
  cors.push_back(std::move(cor(5).via(&e)));
  cors.push_back(std::move(cor(1).via(&e)));
  cors.push_back(std::move(cor(3).via(&e)));

  auto ret = co_await collectAny(std::move(cors));
  idx = ret._idx;
  res = ret._value.value();
  co_return;
}

TEST(async_simple, collect_any) {
  BridgeExecutor e(3);
  size_t idx = 100;
  size_t res = 0;
  auto h = AnyTest(idx, res, e).via(&e);
  syncAwait(std::move(h));
  // bug，总是等待第一个协程执行完毕，没有将协程交给executor调度
  EXPECT_EQ(idx, 1);
  EXPECT_EQ(res, 1);
}

TEST(async_simple, task) {
  BridgePool bp;
  auto arr = bp.map();
  auto a1 = bp.array();
  for (size_t i = 0; i < 10000; ++i) {
    a1->Insert(bp.data("hello" + std::to_string(i)));
  }
  arr->Insert("arr1", std::move(a1));

  auto a2 = bp.array();
  for (size_t i = 0; i < 10000; ++i) {
    a2->Insert(bp.data(uint32_t(i)));
  }
  ObjectWrapper w0(a2.get());
  for (size_t i = 0; i < 10000; ++i) {
    EXPECT_EQ(w0[i].Get<uint32_t>().value(), i);
  }
  arr->Insert("arr2", std::move(a2));
  auto ret = Serialize<SeriType::REPLACE>(std::move(arr), bp);

  ParseOption op;
  op.parse_ref = false;
  op.worker_num_ = 4;
  op.type = SchedulerType::Coroutine;
  auto new_arr = Parse(ret, bp, op);
  ObjectWrapper w(new_arr.get());
  EXPECT_EQ(w.GetType(), ObjectType::Map);
  EXPECT_EQ(w.Size(), 2);
  EXPECT_EQ(w["arr1"].GetType(), ObjectType::Array);
  EXPECT_EQ(w["arr1"].Size(), 10000);
  EXPECT_EQ(w["arr2"].GetType(), ObjectType::Array);

  for (int i = 0; i < 10000; ++i) {
    EXPECT_EQ(w["arr1"][i].Get<std::string>().has_value(), true) << i;
    EXPECT_EQ(w["arr2"][i].Get<uint32_t>().value(), i);
  }
}