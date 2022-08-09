#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "bridge/adaptor.h"
#include "bridge/object.h"
#include "gtest/gtest.h"
using namespace bridge;

TEST(parse_and_seri, string_map) {
  std::vector<int32_t> ints{1, 2, 4, 3};
  std::vector<int32_t> int2{4, 5, 6, 7};
  std::unordered_map<std::string, std::vector<int32_t>> maps;
  maps["v1"] = ints;
  maps["v2"] = int2;
  auto v = adaptor(maps);
  AsMap(v)->Insert("tags", ValueFactory<Data>("hello world"));
  auto content = Serialize<SeriType::REPLACE>(std::move(v));

  auto root = Parse(content, true);
  ObjectWrapper wrapper(root.get());
  EXPECT_EQ(wrapper.GetType(), ObjectType::Map);
  EXPECT_EQ(wrapper.Size(), 3);
  EXPECT_EQ(wrapper["tags"].Get<std::string_view>().value(), "hello world");

  auto root2 = Parse(content);
  ObjectWrapper wrapper2(root2.get());
  EXPECT_EQ(wrapper2.GetType(), ObjectType::Map);
  EXPECT_EQ(wrapper2.Size(), 3);
  EXPECT_EQ(wrapper2["tags"].Get<std::string>().value(), "hello world");
  EXPECT_EQ(wrapper2["v1"][0].Get<int32_t>().value(), 1);
}
