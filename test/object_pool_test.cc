#include "bridge/object_pool.h"

#include "gtest/gtest.h"
using namespace bridge;

struct object_pool_test : public RequireDestruct {
  static int count;
  object_pool_test() { ++count; }

  ~object_pool_test() { --count; }
};

int object_pool_test::count = 0;

TEST(objectpool, all) {
  ObjectPool<object_pool_test> op_test;
  for (int i = 0; i < 10000; ++i) {
    op_test.Alloc();
  }
  EXPECT_EQ(object_pool_test::count, 10000);
  op_test.Clear();
  EXPECT_EQ(object_pool_test::count, 0);
}
