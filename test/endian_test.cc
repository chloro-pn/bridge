#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "bridge/adaptor.h"
#include "bridge/util.h"
#include "gtest/gtest.h"
using namespace bridge;

TEST(object, util) {
  int i = 1;
  if (*((char*)&i) == 1) {
    EXPECT_EQ(Endian::Instance().GetEndianType(), Endian::Type::Little);
  } else {
    EXPECT_EQ(Endian::Instance().GetEndianType(), Endian::Type::Big);
  }
  int i2 = flipByByte(i);
  EXPECT_EQ(i2, 0x01000000);
}
