#include "bridge/pattern.h"

#include <cstdint>
#include <memory>

#include "bridge/object.h"
#include "gtest/gtest.h"

using namespace bridge;

/*
 * pattern :
 * {
 *   "key" : [
 *     [!type : string]
 *     [!type : string]
 *     [!type : uint32]
 *   ]
 * }
 *
 */
std::unique_ptr<MapPattern> CreatePattern() {
  auto orp = std::make_unique<OrPattern>();
  orp->PushPattern(std::make_unique<DataPattern>(BRIDGE_STRING));
  orp->PushPattern(std::make_unique<DataPattern>(BRIDGE_UINT32));
  auto pp2 = std::make_unique<DataPattern>(BRIDGE_STRING);
  auto pp3 = std::make_unique<DataPattern>(BRIDGE_UINT32);
  auto pp4 = std::make_unique<ArrayPattern>(3);
  pp4->setNthPattern(0, std::move(orp));
  pp4->setNthPattern(1, std::move(pp2));
  pp4->setNthPattern(2, std::move(pp3));
  auto pp5 = std::make_unique<MapPattern>();
  pp5->setPattern("key", std::move(pp4));
  return pp5;
}

TEST(object, pattern) {
  BridgePool bp;
  auto pattern_root = CreatePattern();
  auto map = bp.map();
  auto arr = bp.array();
  auto v1 = bp.data("hello");
  auto v2 = bp.data("world");
  auto v3 = bp.data(uint32_t(0));
  arr->Insert(std::move(v1));
  arr->Insert(std::move(v2));
  arr->Insert(std::move(v3));
  map->Insert("key", std::move(arr));
  EXPECT_EQ(pattern_root->Match(map.get()), true);

  pattern_root->setPattern("not_exist_key", std::make_unique<DataPattern>(BRIDGE_INT32));
  EXPECT_EQ(pattern_root->Match(map.get()), false);
  pattern_root->removePattern("not_exist_key");

  ObjectWrapper wrapper(map.get());
  EXPECT_EQ(wrapper.Empty(), false);
  EXPECT_EQ(wrapper["key"][0].Get<std::string>(), "hello");
  EXPECT_EQ(wrapper["not_exist_key"][0].Empty(), true);
  EXPECT_EQ(wrapper["key"][3].Empty(), true);
  EXPECT_EQ(wrapper["not_exist_key"][3].Empty(), true);
  EXPECT_EQ(wrapper.Size(), 1);
  EXPECT_EQ(wrapper["key"].Size(), 3);

  std::string content = Serialize(std::move(map), bp);
  EXPECT_TRUE(content[0] == 0x00 || content[0] == 0x01);

  ParseOption po;
  po.parse_ref = true;
  auto new_root = Parse(content, bp, po);
  ObjectWrapper new_wrapper(new_root.get());
  EXPECT_EQ(new_wrapper.Empty(), false);
  EXPECT_EQ(new_wrapper.GetType().value(), ObjectType::Map);
  auto sv = new_wrapper["key"][0].GetView<std::string>();
  EXPECT_EQ(sv, "hello");
  EXPECT_EQ(new_wrapper["not_exist_key"].Empty(), true);
  EXPECT_EQ(new_wrapper["key"][3].Empty(), true);
  
  std::string_view v = new_wrapper["key"][0].GetView<std::string>().value();
  std::intptr_t p1 = reinterpret_cast<std::intptr_t>(&content[0]);
  std::intptr_t p2 = p1 + content.size();
  std::intptr_t p3 = reinterpret_cast<std::intptr_t>(&v[0]);
  EXPECT_TRUE(p3 >= p1 && p2 >= p3);
  EXPECT_EQ(pattern_root->Match(new_root.get()), true);
}