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
  Array arr;
  arr.Insert(data("test_str"));
  arr.Insert(data(int32_t(124)));
  arr.Insert(data(std::vector<char>{'a', 'b'}));
  std::string buf;
  arr.dump(buf, 0);
  EXPECT_EQ(buf, R"(Array[ 3 ]
 Data[ string ]
 Data[ int32 ]
 Data[ bytes ])");
}

TEST(dump, map) {
  Map map;
  map.Insert("key1", data("hello"));
  map.Insert("key2", data(int32_t(25)));
  auto arr = array();
  arr->Insert(data("world"));
  map.Insert("key3", std::move(arr));
  std::string buf;
  map.dump(buf, 0);
  EXPECT_EQ(buf, R"(Map[ 3 ]
 < key1 > : 
  Data[ string ]
 < key2 > : 
  Data[ int32 ]
 < key3 > : 
  Array[ 1 ]
   Data[ string ])");
}