#include "bridge/object.h"

#include <memory>
#include <string>

#include "gtest/gtest.h"
using namespace bridge;

TEST(object, factory) {
  BridgePool bp;
  auto v = bp.data();
  EXPECT_EQ(v->GetType(), ObjectType::Data);
  auto v2 = bp.map();
  EXPECT_EQ(v2->GetType(), ObjectType::Map);
  auto v3 = bp.array();
  EXPECT_EQ(v3->GetType(), ObjectType::Array);
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

TEST(object, data_str) {
  Data d;
  EXPECT_EQ(d.GetPtr<double>(), nullptr);
  d = 2.15f;
  EXPECT_FLOAT_EQ(*d.GetPtr<float>(), 2.15f);
  EXPECT_EQ(d.GetPtr<uint32_t>(), nullptr);
}

TEST(object, data_view) {
  char ptr[] = "hello world";
  DataView v(ptr);
  EXPECT_EQ(v.GetDataType(), BRIDGE_STRING);
  EXPECT_EQ(v.Get<std::string>(), "hello world");
  EXPECT_EQ(&v.Get<std::string>().value()[0], &ptr[0]);

  std::string_view view("this is bridge");
  v = view;
  EXPECT_EQ(v.GetDataType(), BRIDGE_STRING);
  EXPECT_EQ(v.Get<std::string>(), "this is bridge");

  v = uint32_t(24);
  EXPECT_EQ(v.GetDataType(), BRIDGE_UINT32);
  EXPECT_EQ(v.Get<uint32_t>().value(), 24);

  // 错误做法！ v = std::vector<char> { 0x00, 0x01, 0x02, 0x0A };
  // todo: 编译期避免给DataView传递右值的string和vector<char>
  std::vector<char> tmp{0x00, 0x01, 0x02, 0x0A};
  v = tmp;
  EXPECT_EQ(v.GetDataType(), BRIDGE_BYTES);
  char buf[] = {0x00, 0x01, 0x02, 0x0A};
  EXPECT_EQ(v.Get<bridge::bridge_binary_type>().value(), std::string_view(&buf[0], 4));
}

TEST(object, array) {
  BridgePool bp;
  Array arr(bp);
  EXPECT_EQ(arr.GetType(), ObjectType::Array);
  EXPECT_EQ(arr[1], nullptr);
  EXPECT_EQ(arr.Size(), 0);
  arr.Insert(bp.data(int32_t(32)));
  arr.Insert(bp.data("chloro"));
  EXPECT_EQ(arr.Size(), 2);
  auto v = arr[0];
  EXPECT_EQ(v->GetType(), ObjectType::Data);
  EXPECT_EQ(AsData(v)->GetDataType(), BRIDGE_INT32);
  EXPECT_EQ(AsData(arr[1])->GetDataType(), BRIDGE_STRING);
}

TEST(object, map) {
  BridgePool bp;
  Map map(bp);
  EXPECT_EQ(map.GetType(), ObjectType::Map);
  EXPECT_EQ(map["not_exist"], nullptr);
  EXPECT_EQ(map.Size(), 0);
  map.Insert("key1", bp.data("value1"));
  map.Insert("key2", bp.data(int32_t(10)));
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
  auto it = data.Get<TestStruct>();
  EXPECT_EQ(it.has_value(), true);
  EXPECT_EQ(it.value().c, 'x');
}