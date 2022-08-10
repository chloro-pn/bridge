#include "bridge/variant.h"

#include <variant>

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

TEST(variant, not_exist) {
  auto v = NotExist<int, int>::value;
  EXPECT_EQ(v, false);
  v = NotExist<uint8_t, short, int, double>::value;
  EXPECT_EQ(v, true);
  using my_type = int;
  v = NotExist<int, my_type>::value;
  EXPECT_EQ(v, false);
}

TEST(variant, unique) {
  auto v = Unique<int, double, std::string>::value;
  EXPECT_EQ(v, true);
  v = Unique<std::string, std::string, double>::value;
  EXPECT_EQ(v, false);
  v = Unique<std::string>::value;
  EXPECT_EQ(v, true);
}

// 内存对齐+type-flag导致variant占用过多padding，在对象池/内存池场景下可以将type-flag统一存储在varint外，优化内存使用。
TEST(variant, std_variant) {
  std::variant<std::string> v;
  EXPECT_EQ(sizeof(v), sizeof(std::string) + alignof(std::string));
  std::variant<double> v2;
  EXPECT_EQ(sizeof(v2), 2 * sizeof(double));
}

TEST(variant, variant) {
  using variant_type = variant<tmp, uint8_t, uint16_t, double, std::string>;
  variant_type v;
  EXPECT_EQ(sizeof(v), std::max(sizeof(tmp), sizeof(std::string)));
  EXPECT_EQ(alignof(variant_type), alignof(double));
  v.construct<uint8_t>(23);
  auto value = v.get<uint8_t>();
  EXPECT_EQ(value, 23);
  v.construct<std::string>("hello world");
  std::string& str = v.get<std::string>();
  EXPECT_EQ(str, "hello world");
  std::string new_str(v.move<std::string>());
  v.destruct<std::string>();
  EXPECT_EQ(new_str, "hello world");
  v.construct<tmp>();
  EXPECT_EQ(tmp::count, 1);
  v.destruct<tmp>();
  EXPECT_EQ(tmp::count, 0);

  variant<uint16_t, tmp> v2;
  EXPECT_EQ(sizeof(v2), sizeof(tmp));
  EXPECT_EQ(alignof(variant<uint16_t, tmp>), alignof(uint16_t));
}