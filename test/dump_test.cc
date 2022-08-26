#include <string>

#include "bridge/object.h"
#include "gtest/gtest.h"
using namespace bridge;

TEST(dump, data) {
  Data data("hello world");
  std::string buf;
  data.dump(buf, 0);
  EXPECT_EQ(buf, "Data[ string ]");
  buf.clear();

  data = int32_t(12138);
  data.dump(buf, 1);
  EXPECT_EQ(buf, " Data[ int32 ]");
}

TEST(dump, array) {
  BridgePool bp;
  Array arr(bp);
  arr.Insert(bp.data("test_str"));
  arr.Insert(bp.data(int32_t(124)));
  arr.Insert(bp.data(std::vector<char>{'a', 'b'}));
  std::string buf;
  arr.dump(buf, 0);
  EXPECT_EQ(buf, R"(Array[ 3 ]
 Data[ bytes ]
 Data[ int32 ]
 Data[ string ])");
}

TEST(dump, map) {
  BridgePool bp;
  Map map(bp);
  map.Insert("key1", bp.data("hello"));
  map.Insert("key2", bp.data(int32_t(25)));
  auto arr = bp.array();
  arr->Insert(bp.data("world"));
  map.Insert("key3", std::move(arr));
  std::string buf;
  map.dump(buf, 0);
}