#include "bridge/async_executor/thread_pool.h"

#include <atomic>

#include "gtest/gtest.h"

TEST(executor, thread_pool) {
  auto& pool = bridge::ThreadPool::GetOrConstructInstance(4);
  std::atomic<int> count(0);
  for (int i = 0; i < 100000; ++i) {
    pool.PushTask([&]() { count.fetch_add(1); });
  }
  pool.Stop();
  EXPECT_EQ(count.load(), 100000);
}