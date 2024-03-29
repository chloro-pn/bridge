#include "bridge/adaptor.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "bridge/object.h"
#include "gtest/gtest.h"
using namespace bridge;

TEST(object, adaptor) {
  BridgePool bp;

  std::vector<int32_t> ints{1, 2, 4, 3};
  auto v = adaptor(ints, bp);
  EXPECT_EQ(v->GetType(), ObjectType::Array);
  auto arr = AsArray(v);
  EXPECT_EQ(arr->Size(), 4);
  auto v1 = AsData((*arr)[0]);
  EXPECT_EQ(v1->GetDataType(), BRIDGE_INT32);
  EXPECT_EQ(v1->Get<int32_t>().value(), 1);

  std::unordered_map<std::string, int32_t> maps;
  maps["key1"] = 2;
  maps["key2"] = 1;
  v = adaptor(maps, bp);
  EXPECT_EQ(v->GetType(), ObjectType::Map);
  auto map = AsMap(v);
  EXPECT_EQ(map->Size(), 2);
  v1 = AsData((*map)["key1"]);
  EXPECT_EQ(v1->GetDataType(), BRIDGE_INT32);
  EXPECT_EQ(v1->Get<int32_t>().value(), 2);

  std::unordered_map<std::string_view, std::vector<std::unordered_map<std::string, uint32_t>>> cc;
  std::unordered_map<std::string, uint32_t> tmp;
  tmp.insert({"key2", 24});
  cc["key1"].push_back(tmp);
  v = adaptor(cc, bp);
  ObjectWrapper wrapper(v.get());
  EXPECT_EQ(wrapper.GetType(), ObjectType::Map);

  auto it = wrapper.GetIteraotr().value();
  EXPECT_EQ(it.GetType(), bridge::ObjectWrapperIterator::IteratorType::MapView);
  EXPECT_EQ(wrapper["key1"][0]["key2"].Get<uint32_t>().value(), 24);
}
