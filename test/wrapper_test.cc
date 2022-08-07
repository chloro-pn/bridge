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

  std::string content = Serialize(std::move(map));
  EXPECT_TRUE(content[0] == 0x00 || content[0] == 0x01);

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

TEST(object, wrapper_iter) {
  auto map = ValueFactory<Map>();
  auto arr = ValueFactory<Array>();
  auto v1 = ValueFactory<Data>("hello");
  auto v2 = ValueFactory<Data>("world");
  auto v3 = ValueFactory<Data>(uint32_t(0));
  arr->Insert(std::move(v1));
  arr->Insert(std::move(v2));
  arr->Insert(std::move(v3));
  map->Insert("key", std::move(arr));
  map->Insert("key2", ValueFactory<Data>(uint32_t(15)));
  ObjectWrapper wrapper(map.get());
  ObjectWrapperIterator iter = wrapper.GetIteraotr().value();
  bool find_key = false;
  bool find_key2 = false;
  for (; iter.Valid(); ++iter) {
    if (iter.GetKey() == "key") {
      EXPECT_EQ(find_key, false);
      find_key = true;
      ObjectWrapper v = iter.GetValue();
      EXPECT_EQ(v.GetType(), ObjectType::Array);
      EXPECT_EQ(v.Size(), 3);
      EXPECT_EQ(v[0].Get<std::string>().value(), "hello");
    } else if (iter.GetKey() == "key2") {
      EXPECT_EQ(find_key2, false);
      find_key2 = true;
      ObjectWrapper v = iter.GetValue();
      EXPECT_EQ(v.GetType(), ObjectType::Data);
      EXPECT_EQ(v.Get<uint32_t>().value(), 15);
    }
  }
  EXPECT_EQ(find_key2 && find_key, true);
}