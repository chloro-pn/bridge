#include "bridge/object.h"

#include <memory>
#include <string>

#include "gtest/gtest.h"
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
  EXPECT_EQ(data.Get<uint32_t>().value(), (int32_t)32);
  EXPECT_EQ(data.GetDataType(), BRIDGE_UINT32);
  char cstr[6] = "world";
  data = cstr;
  EXPECT_EQ(data.GetDataType(), BRIDGE_STRING);
  EXPECT_EQ(data.Get<std::string>().value(), "world");
  auto view = data.GetView();
  EXPECT_EQ(view[0], 'w');
  EXPECT_EQ(view[1], 'o');
}

TEST(object, array) {
  Array arr;
  EXPECT_EQ(arr.GetType(), ObjectType::Array);
  EXPECT_EQ(arr[1], nullptr);
  EXPECT_EQ(arr.Size(), 0);
  arr.Insert(ValueFactory<Data>(int32_t(32)));
  arr.Insert(ValueFactory<Data>("chloro"));
  EXPECT_EQ(arr.Size(), 2);
  auto v = arr[0];
  EXPECT_EQ(v->GetType(), ObjectType::Data);
  EXPECT_EQ(AsData(v)->GetDataType(), BRIDGE_INT32);
  EXPECT_EQ(AsData(arr[1])->GetDataType(), BRIDGE_STRING);
}

TEST(object, map) {
  Map map;
  EXPECT_EQ(map.GetType(), ObjectType::Map);
  EXPECT_EQ(map["not_exist"], nullptr);
  EXPECT_EQ(map.Size(), 0);
  map.Insert("key1", ValueFactory<Data>("value1"));
  map.Insert("key2", ValueFactory<Data>(int32_t(10)));
  EXPECT_EQ(map.Size(), 2);
  auto v = map["key1"];
  EXPECT_EQ(v->GetType(), ObjectType::Data);
  EXPECT_EQ(AsData(v)->GetDataType(), BRIDGE_STRING);
  EXPECT_EQ(AsData(v)->Get<std::string>().value(), "value1");
}

struct TestStruct {
  std::vector<char> SerializeToBridge() const { return {c, 'h', 'e', 'l', 'l', 'o'}; }
  static TestStruct ConstructFromBridge(const std::vector<char>& bytes) {
    TestStruct obj('a');
    assert(bytes.size() == 6 && bytes[1] == 'h' && bytes[5] == 'o');
    obj.c = bytes[0];
    return obj;
  }

  char c;
  TestStruct(char cc) : c(cc) {}
};

TEST(object, custom) {
  Data data{TestStruct('x')};
  EXPECT_EQ(data.GetType(), ObjectType::Data);
  EXPECT_EQ(data.GetDataType(), BRIDGE_CUSTOM);
  std::string_view view = data.GetView();
  EXPECT_EQ(view.size(), 6);
  EXPECT_EQ(view, "xhello");
  auto it = data.Get<TestStruct>();
  EXPECT_EQ(it.has_value(), true);
  EXPECT_EQ(it.value().c, 'x');
}