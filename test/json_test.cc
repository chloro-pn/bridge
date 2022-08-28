#include "gtest/gtest.h"
#include "rapidjson/document.h"

TEST(json, rapidjson) {
  rapidjson::Document d;
  d.SetObject();
  for (int i = 0; i < 10; ++i) {
    rapidjson::Value v(i);
    d.AddMember("key", v, d.GetAllocator());
  }
  auto v = d["key"].GetUint();
  // wtf ???, 顿时优化有了新思路
  EXPECT_EQ(v, 0);
}