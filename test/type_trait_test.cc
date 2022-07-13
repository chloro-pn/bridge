#include "bridge/type_trait.h"

#include "gtest/gtest.h"

using namespace bridge;

template <typename T>
struct bridge_integral_test {
  static constexpr bool value = false;
};

template <typename T>
requires bridge_integral<T> struct bridge_integral_test<T> {
  static constexpr bool value = true;
};

TEST(type_trait, all) {
  EXPECT_EQ(NoRefNoPointer<int>::value, true);
  EXPECT_EQ(NoRefNoPointer<int&>::value, false);
  EXPECT_EQ(NoRefNoPointer<int&&>::value, false);
  EXPECT_EQ(NoRefNoPointer<const int&>::value, false);
  EXPECT_EQ(NoRefNoPointer<const int&&>::value, false);
  EXPECT_EQ(NoRefNoPointer<int*>::value, false);
  EXPECT_EQ(NoRefNoPointer<const int*>::value, false);
  EXPECT_EQ(NoRefNoPointer<int* const>::value, false);

  EXPECT_EQ(bridge_integral_test<uint8_t>::value, false);
  EXPECT_EQ(bridge_integral_test<uint32_t>::value, true);
}