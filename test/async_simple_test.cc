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
  auto a1 = array();
  for (size_t i = 0; i < 10000; ++i) {
    a1->Insert(data("hello" + std::to_string(i)));
  }
  arr.Insert(std::move(a1));

  auto a2 = array();
  for (size_t i = 0; i < 10000; ++i) {
    a2->Insert(data(uint32_t(i)));
  }
  arr.Insert(std::move(a2));
  std::string ret;
  arr.valueSeri(ret, nullptr, &si);
  auto new_arr = syncAwait(ParseArray(ret, si, 0));
  ObjectWrapper w(new_arr.v.get());
  EXPECT_EQ(w.GetType(), ObjectType::Array);
  EXPECT_EQ(w.Size(), 2);
  EXPECT_EQ(w[0].GetType(), ObjectType::Array);
  EXPECT_EQ(w[0].Size(), 10000);
  EXPECT_EQ(w[1].GetType(), ObjectType::Array);
  for(int i = 0; i < 10000; ++i) {
    EXPECT_EQ(w[1][i].Get<uint32_t>().value(), i);
  }
}