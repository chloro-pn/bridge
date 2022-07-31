#include "bridge/inner.h"

#include <string>

#include "gtest/gtest.h"
using namespace bridge;

TEST(inner, all) {
  std::string str("hello world");
  InnerWrapper wrapper(str);
  EXPECT_EQ(wrapper.curAddr(), &str[0]);
  EXPECT_EQ(wrapper.outOfRange(), false);
  wrapper.skip(2);
  EXPECT_EQ(wrapper.curAddr(), &str[2]);
  wrapper.skip(10);
  EXPECT_EQ(wrapper.outOfRange(), true);
}
