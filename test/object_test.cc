#include "gtest/gtest.h"

#include "bridge/object.h"

#include <string>
#include <memory>
using namespace bridge;

TEST(object, factory) {
  auto v = ObjectFactory(ObjectType::Data);
  EXPECT_EQ(v->GetType(), ObjectType::Data);
  v = ObjectFactory(ObjectType::Map);
  EXPECT_EQ(v->GetType(), ObjectType::Map);
  v = ObjectFactory(ObjectType::Array);
  EXPECT_EQ(v->GetType(), ObjectType::Array);
  v = ObjectFactory(ObjectType::Invalid);
  EXPECT_EQ(v, nullptr);
}

TEST(object, data) {
  Data data;
  EXPECT_EQ(data.GetDataType(), BRIDGE_INVALID);
  EXPECT_EQ(data.Get<std::string>().has_value(), false);
  EXPECT_EQ(data.Get<uint32_t>().has_value(), false);

  data = std::string("hello world");
  EXPECT_EQ(data.Get<std::string>().value(), "hello world");
  EXPECT_EQ(data.GetDataType(), BRIDGE_STRING);
  data = uint32_t(32);
  EXPECT_EQ(data.Get<uint32_t>().value(), 32);
  EXPECT_EQ(data.GetDataType(), BRIDGE_UINT32);
  char cstr[6] = "world";
  data = cstr;
  EXPECT_EQ(data.GetDataType(), BRIDGE_STRING);
  EXPECT_EQ(data.Get<std::string>().value(), "world");
  const char* ptr = data.GetRaw();
  EXPECT_EQ(*ptr, 'w');
  EXPECT_EQ(*(ptr + 1), 'o');
}