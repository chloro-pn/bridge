
#include <string>

#include "bridge/object.h"
#include "bridge/split_info.h"
#include "bridge/string_map.h"
#include "gtest/gtest.h"

using namespace bridge;

TEST(secondary, string_map) {
  StringMap map;
  uint32_t id1 = map.RegisterIdFromString("hello");
  uint32_t id2 = map.RegisterIdFromString("world");
  uint32_t id3 = map.RegisterIdFromString("hello");
  EXPECT_EQ(id1, id3);
  std::string str;
  SuffixSecondary(str, map);
  size_t total_size = 0;
  auto sm = ParseSecondary<StringMap>(str, total_size);
  EXPECT_EQ(sm.GetStr(id1), "hello");
  EXPECT_EQ(sm.GetStr(id2), "world");
}

TEST(secondary, split_info) {
  SplitInfo si;
  uint32_t id1 = si.RequestId();
  std::vector<uint32_t> b1{100, 200, 300};
  std::vector<uint32_t> b2{1, 2, 3};
  si.RegisterSplitInfo(id1, b1);
  uint32_t id2 = si.RequestId();
  EXPECT_NE(id1, id2);
  si.RegisterSplitInfo(id2, b2);
  std::string str;
  SuffixSecondary(str, si);
  size_t total_size = 0;
  auto new_si = ParseSecondary<SplitInfo>(str, total_size);
  EXPECT_EQ(new_si.GetBlockInfoFromId(id1), b1);
  EXPECT_EQ(new_si.GetBlockInfoFromId(id2), b2);
}