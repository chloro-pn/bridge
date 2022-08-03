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
}

TEST(object, data_string) {
  Data data;
  data = std::string("hello world");
  EXPECT_EQ(data.GetDataType(), BRIDGE_STRING);
  EXPECT_EQ(data.Get<std::string>().value(), "hello world");
  char buf[] = "value2";
  data = buf;
  EXPECT_EQ(data.GetDataType(), BRIDGE_STRING);
  EXPECT_EQ(data.Get<std::string>().value(), "value2");
}

TEST(object, data_int) {
  Data data;
  data = (int32_t)2143;
  EXPECT_EQ(data.GetDataType(), BRIDGE_INT32);
  EXPECT_EQ(data.Get<int32_t>().value(), 2143);
  data = (uint32_t)12138;
  EXPECT_EQ(data.GetDataType(), BRIDGE_UINT32);
  EXPECT_EQ(data.Get<uint32_t>().value(), 12138);
}

TEST(object, data_float) {
  Data data;
  data = 2.15f;
  EXPECT_EQ(data.GetDataType(), BRIDGE_FLOAT);
  EXPECT_FLOAT_EQ(data.Get<float>().value(), 2.15);
  data = 3.33;
  EXPECT_EQ(data.GetDataType(), BRIDGE_DOUBLE);
  EXPECT_DOUBLE_EQ(data.Get<double>().value(), 3.33);
}

TEST(object, data_view) {
  std::string str("hello world");
  DataView view(std::string_view(str), BRIDGE_STRING);
  EXPECT_EQ(view.GetDataType(), BRIDGE_STRING);
  EXPECT_EQ(view.GetView(), str);
  EXPECT_EQ(&view.GetView()[0], &str[0]);
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