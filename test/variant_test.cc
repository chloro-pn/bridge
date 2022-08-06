#include "bridge/variant.h"

#include "gtest/gtest.h"
using namespace bridge;

struct tmp {
  char buf[128];
  static size_t count;
  tmp() { ++count; }
  ~tmp() { --count; }
};

size_t tmp::count = 0;

TEST(variant, size) {
  EXPECT_EQ(MaxSize<uint8_t>::value, 1);
  size_t v = MaxSize<uint8_t, short>::value;
  EXPECT_EQ(v, 2);
  v = MaxSize<double, uint32_t, tmp>::value;
  EXPECT_EQ(v, 128);
}

TEST(variant, align) {
  EXPECT_EQ(MaxAlign<uint8_t>::value, 1);
  size_t v = MaxAlign<uint8_t, short>::value;
  EXPECT_EQ(v, 2);
  v = MaxAlign<double, uint32_t, tmp>::value;
  EXPECT_EQ(v, 8);
}

TEST(variant, count) {
  EXPECT_EQ(Count<uint8_t>::count, 1);
  size_t c = Count<double, uint8_t, uint32_t>::count;
  EXPECT_EQ(c, 3);
}

TEST(variant, index) {
  size_t i = Index<uint8_t, uint8_t, uint16_t, double>::index;
  EXPECT_EQ(i, 0);
  i = Index<uint16_t, uint8_t, uint16_t, double>::index;
  EXPECT_EQ(i, 1);
  i = Index<double, uint8_t, uint16_t, double>::index;
  EXPECT_EQ(i, 2);
  i = Index<float, uint8_t, uint16_t, double>::index;
  EXPECT_EQ(i, 3);
  size_t c = Count<uint8_t, uint16_t, double>::count;
  EXPECT_EQ(i, c);
}

TEST(variant, variant) {
  variant<tmp, uint8_t, uint16_t, double, std::string> v;
  EXPECT_EQ(sizeof(v), std::max(sizeof(tmp), sizeof(std::string)));
  EXPECT_EQ(alignof(v), alignof(double));
  v.construct<uint8_t>(23);
  auto value = v.get<uint8_t>();
  EXPECT_EQ(value, 23);
  v.construct<std::string>("hello world");
  std::string& str = v.get<std::string>();
  EXPECT_EQ(str, "hello world");
  v.destruct<std::string>();
  v.construct<tmp>();
  EXPECT_EQ(tmp::count, 1);
  v.destruct<tmp>();
  EXPECT_EQ(tmp::count, 0);

  variant<uint16_t, tmp> v2;
  EXPECT_EQ(sizeof(v2), sizeof(tmp));
  EXPECT_EQ(alignof(v2), alignof(uint16_t));
}