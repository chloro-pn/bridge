#include "bridge/type_trait.h"

#include <array>
#include <map>
#include <set>
#include <unordered_map>
#include <vector>

#include "gtest/gtest.h"

using namespace bridge;

template <typename T>
struct bridge_integral_test {
  static constexpr bool value = false;
};

template <typename T>
requires bridge_integral<T>
struct bridge_integral_test<T> {
  static constexpr bool value = true;
};

template <typename T>
struct bridge_adaptor_test {
  static constexpr bool value = false;
  using type = void;
};

template <typename T>
requires bridge_adaptor_type<T>
struct bridge_adaptor_test<T> {
  static constexpr bool value = true;
  using type = typename AdaptorTrait<T>::type;
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

  EXPECT_EQ(bridge_adaptor_test<uint32_t>::value, false);
  EXPECT_EQ(bridge_adaptor_test<std::vector<uint32_t>>::value, true);
  auto tmp = bridge_adaptor_test<std::unordered_map<std::string, uint32_t>>::value;
  using type1 = std::vector<uint32_t>;
  using type2 = std::unordered_map<std::string, type1>;
  using type3 = std::vector<type2>;
  using type4 = std::unordered_map<std::string, type3>;
  using type5 = std::unordered_map<std::string, type4>;

  EXPECT_EQ(bridge_adaptor_test<type5>::value, true);

  using type6 = std::array<type1, 10>;
  using type7 = std::map<int, type2>;
  using type8 = std::set<type4>;
  EXPECT_EQ(bridge_adaptor_test<type6>::value, false);
  EXPECT_EQ(bridge_adaptor_test<type7>::value, false);
  EXPECT_EQ(bridge_adaptor_test<type8>::value, false);
  EXPECT_EQ(tmp, true);
}