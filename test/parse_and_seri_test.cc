#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "bridge/adaptor.h"
#include "bridge/object.h"
#include "gtest/gtest.h"
using namespace bridge;

TEST(parse_and_seri, replace) {
  BridgePool bp;
  std::vector<int32_t> ints{1, 2, 4, 3};
  std::vector<int32_t> int2{4, 5, 6, 7};
  std::unordered_map<std::string, std::vector<int32_t>> maps;
  maps["v1"] = ints;
  maps["v2"] = int2;
  auto v = adaptor(maps, bp);
  AsMap(v)->Insert("tags", bp.data("hello world"));
  auto content = Serialize<SeriType::REPLACE>(std::move(v), bp);

  ParseOption po;
  po.parse_ref = true;
  auto root = Parse(content, bp, po);
  ObjectWrapper wrapper(root.get());
  EXPECT_EQ(wrapper.GetType(), ObjectType::Map);
  EXPECT_EQ(wrapper.Size(), 3);
  EXPECT_EQ(wrapper["tags"].Get<std::string_view>().value(), "hello world");

  auto root2 = Parse(content, bp);
  ObjectWrapper wrapper2(root2.get());
  EXPECT_EQ(wrapper2.GetType(), ObjectType::Map);
  EXPECT_EQ(wrapper2.Size(), 3);
  EXPECT_EQ(wrapper2["tags"].Get<std::string>().value(), "hello world");
  EXPECT_EQ(wrapper2["v1"][0].Get<int32_t>().value(), 1);
}

TEST(parse_and_seri, exception) {
  BridgePool bp;
  std::string strs;
  EXPECT_THROW(Parse(strs, bp, ParseOption()), std::runtime_error);
  strs.resize(8);
  EXPECT_THROW(Parse(strs, bp, ParseOption()), std::runtime_error);
  strs.resize(1024, 'a');
  EXPECT_THROW(Parse(strs, bp, ParseOption()), std::runtime_error);
}