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