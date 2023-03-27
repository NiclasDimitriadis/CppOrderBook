#define DOCTEST_CONFIG_IMPLEMENT 

#include "Unittest_Includes.hpp"


TEST_CASE("testing Auxil::pow_two_ceil") {
  CHECK(Auxil::pow_two_ceil<std::uint32_t>(0) == 1);
  CHECK(Auxil::pow_two_ceil<std::uint32_t>(50) == 64);
  CHECK(Auxil::pow_two_ceil<std::uint32_t>(100000) == 131072);
}

TEST_CASE("testing Auxil::divisible_or_ceil") {
  CHECK(Auxil::divisible_or_ceil<std::uint32_t>(2, 5) == 5);
  CHECK(Auxil::divisible_or_ceil<std::uint32_t>(3, 10) == 5);
  CHECK(Auxil::divisible_or_ceil<std::uint32_t>(11, 10) == 20);
}

TEST_CASE("testing Auxil::check_parameter_pack") {
  CHECK(Auxil::check_parameter_pack<std::is_arithmetic, int, float,
                                    double>::value == true);
  CHECK(Auxil::check_parameter_pack<std::is_arithmetic, int, float, double,
                                    std::string>::value == false);
}

TEST_CASE("testing Auxil::add_to_tuple") {
  using test_tuple = std::tuple<int, int, int>;
  test_tuple add_to{1, 2, -3};
  test_tuple add{11, -12, 13};
  Auxil::add_to_tuple(add_to, add);
  CHECK(std::get<0>(add_to) == 12);
  CHECK(std::get<1>(add_to) == -10);
  CHECK(std::get<2>(add_to) == 10);
}

TEST_CASE("testing Auxil::is_negative") {
  CHECK(Auxil::is_negative(2) == false);
  CHECK(Auxil::is_negative(-5) == true);
  CHECK(Auxil::is_negative(0) == false);
}

TEST_CASE("testing Auxil::validate_tuple") {
  CHECK(Auxil::check_parameter_pack<std::is_arithmetic, int, float,
                                    double>::value == true);
  CHECK(Auxil::check_parameter_pack<std::is_arithmetic, int, float, double,
                                    std::string>::value == false);
}


int main() {
  doctest::Context context;
  context.run();
}