#include "bridge/object_pool.h"

#include "gtest/gtest.h"
using namespace bridge;

struct object_pool_test {
  static int count;
  object_pool_test() { ++count; }

  ~object_pool_test() { --count; }
};

int object_pool_test::count = 0;

TEST(objectpool, all) {
  for (int i = 0; i < 10000; ++i) {
    ObjectPool<object_pool_test>::Instance().Alloc();
  }
  EXPECT_EQ(object_pool_test::count, 10000);
  ObjectPool<object_pool_test>::Instance().Clear();
  EXPECT_EQ(object_pool_test::count, 0);
}
