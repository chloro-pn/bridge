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
  auto content = Serialize<SeriType::REPLACE>(std::move(v));

  auto root = Parse(content);
  ObjectWrapper wrapper(root.get());
  EXPECT_EQ(wrapper.GetType(), ObjectType::Map);
  EXPECT_EQ(wrapper.Size(), 2);
  EXPECT_EQ(wrapper["v1"][0].Get<int32_t>().value(), 1);
}
