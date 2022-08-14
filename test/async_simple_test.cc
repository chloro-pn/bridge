#include "async_simple/coro/Lazy.h"
#include "async_simple/coro/SyncAwait.h"
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
  SplitInfo si;
  Array arr;
  for (size_t i = 0; i < 10000; ++i) {
    arr.Insert(data("hello" + std::to_string(i)));
    arr.Insert(data(uint32_t(i)));
  }
  std::string ret;
  arr.valueSeri(ret, nullptr, &si);
  auto new_arr = syncAwait(ParseArray(ret, si));
  EXPECT_EQ(new_arr->GetType(), ObjectType::Array);
  EXPECT_EQ(new_arr->Size(), 20000);
}