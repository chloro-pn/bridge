#include "async_simple/coro/Lazy.h"
#include "async_simple/coro/SyncAwait.h"
#include "gtest/gtest.h"

using namespace async_simple::coro;

Lazy<int> Add() { co_return 1 + 2; }

TEST(async_simple, basic) {
  int num = syncAwait(Add());
  EXPECT_EQ(num, 3);
}
