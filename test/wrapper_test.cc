#include <cstdint>
#include <memory>

#include "bridge/object.h"
#include "gtest/gtest.h"

using namespace bridge;

TEST(object, wrapper) {
  auto map = ValueFactory<Map>();
  auto arr = ValueFactory<Array>();
  auto v1 = ValueFactory<Data>("hello");
  auto v2 = ValueFactory<Data>("world");
  auto v3 = ValueFactory<Data>(uint32_t(0));
  arr->Insert(std::move(v1));
  arr->Insert(std::move(v2));
  arr->Insert(std::move(v3));
  map->Insert("key", std::move(arr));

  ObjectWrapper wrapper(map.get());
  EXPECT_EQ(wrapper.Empty(), false);
  EXPECT_EQ(wrapper["key"][0].Get<std::string>(), "hello");
  EXPECT_EQ(wrapper["not_exist_key"][0].Empty(), true);
  EXPECT_EQ(wrapper["key"][3].Empty(), true);
  EXPECT_EQ(wrapper["not_exist_key"][3].Empty(), true);
  EXPECT_EQ(wrapper.Size(), 1);
  EXPECT_EQ(wrapper["key"].Size(), 3);
  auto view = wrapper["key"][1].GetView();
  EXPECT_EQ(view, "world");

  std::string content = Serialize(std::move(map));

  auto new_root = Parse(content, true);
  ObjectWrapper new_wrapper(new_root.get());
  EXPECT_EQ(new_wrapper.Empty(), false);
  EXPECT_EQ(new_wrapper.GetType().value(), ObjectType::Map);
  EXPECT_EQ(new_wrapper["key"][0].GetView(), "hello");
  EXPECT_EQ(new_wrapper["not_exist_key"].Empty(), true);
  EXPECT_EQ(new_wrapper["key"][3].Empty(), true);
  std::string_view v = new_wrapper["key"][0].GetView().value();
  std::intptr_t p1 = reinterpret_cast<std::intptr_t>(&content[0]);
  std::intptr_t p2 = p1 + content.size();
  std::intptr_t p3 = reinterpret_cast<std::intptr_t>(&v[0]);
  EXPECT_TRUE(p3 >= p1 && p2 >= p3);
}