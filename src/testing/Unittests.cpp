#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <future>
#include <iostream>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

#include "Allocator.hpp"
#include "AtomicGuards.hpp"
#include "Auxil.hpp"
#include "RingBuffer.hpp"

namespace unittest_helpers{
	struct One{
		static constexpr int value = 1;
		};
		
	struct Ten{
		static constexpr int value = 10;
		};
		
	struct Onehundred{
		static constexpr int value = 100;
	};

	template <typename T>
	void val_to_ptr(int* ptr){
		*ptr += T::value;
	};

	template <typename T, typename Var>
	void init_var(Var* const  var_ptr, const int& compare_to){
		if(T::value == compare_to){
			var_ptr->template emplace<T>();
		}
	};

	template <typename Tpl, std::size_t... Is>
	void visit_tuple(int* ptr) {
		(val_to_ptr<std::tuple_element_t<Is,Tpl>>(ptr),...);
	};

	template <typename Var, std::size_t... Is>
	void visit_var_types(Var* const var_ptr, const int&  compare_to, std::index_sequence<Is...>){
		(init_var<std::variant_alternative_t<Is, Var>>(var_ptr, compare_to),...);
	};
};

namespace uh = unittest_helpers;
	
TEST_CASE("testing Auxil::pow_two_ceil"){
	CHECK(Auxil::pow_two_ceil<std::uint32_t>(0) == 1);
	CHECK(Auxil::pow_two_ceil<std::uint32_t>(50) == 64);
	CHECK(Auxil::pow_two_ceil<std::uint32_t>(100000) == 131072);
}

TEST_CASE("testing Auxil::divisible_or_ceil"){
	CHECK(Auxil::divisible_or_ceil<std::uint32_t>(2,5) == 5);
	CHECK(Auxil::divisible_or_ceil<std::uint32_t>(3,10) == 5);
	CHECK(Auxil::divisible_or_ceil<std::uint32_t>(11,10) == 20);
}

TEST_CASE("testing Auxil::check_parameter_pack"){
	CHECK(Auxil::check_parameter_pack<std::is_arithmetic, int, float, double>::value == true);
	CHECK(Auxil::check_parameter_pack<std::is_arithmetic, int, float, double, std::string>::value == false);
}

TEST_CASE("testing Auxil::add_to_tuple"){
	using test_tuple = std::tuple<int, int, int>;
	test_tuple add_to{1,2,-3};
	test_tuple add{11, -12, 13};
	Auxil::add_to_tuple(add_to, add);
	CHECK(std::get<0>(add_to) == 12);
	CHECK(std::get<1>(add_to) == -10);
	CHECK(std::get<2>(add_to) == 10);
}
	
TEST_CASE("testing Auxil::is_negative"){
	CHECK(Auxil::is_negative(2) == false);
	CHECK(Auxil::is_negative(-5) == true);
	CHECK(Auxil::is_negative(0) == false);
	CHECK(Auxil::is_negative<double>(0) == false);
}

TEST_CASE("testing Auxil::validate_tuple"){
	CHECK(Auxil::check_parameter_pack<std::is_arithmetic, std::tuple<int, float, double>>::value == true);
	CHECK(Auxil::check_parameter_pack<std::is_arithmetic, std::tuple<int, float, double, std::string>>::value == false);
}

TEST_CASE("testing Auxil::strtoAirth"){
	
}
	

TEST_CASE("testing AtomicGuards::AtomicFlagGuard")
{
    std::atomic_flag testFlag{false};
    AtomicGuards::AtomicFlagGuard testGuard = AtomicGuards::AtomicFlagGuard(&testFlag);

	SUBCASE("binds to flag")
	{
		CHECK(&testFlag == testGuard.return_flag_ptr());
	}

	SUBCASE("locks correctly")
	{
		CHECK(testGuard.is_locked() == false);
		testGuard.lock();
		CHECK(testFlag.test() == true);
		CHECK(testGuard.is_locked() == true);
	}

	SUBCASE("unlocks correctly")
	{
		testGuard.lock();
		testGuard.unlock();
		CHECK(testGuard.is_locked() == false);
		CHECK(testFlag.test() == false);
	}

	SUBCASE("rebinds correctly")
	{
		auto otherFlag = std::atomic_flag{false};
		testGuard.lock();
		testGuard.rebind(&otherFlag);
		CHECK(testFlag.test() == false);
		CHECK(&otherFlag == testGuard.return_flag_ptr());
	}

	SUBCASE("move assigns correctly")
	{
		testGuard.lock();
		auto otherGuard = AtomicGuards::AtomicFlagGuard(nullptr);
		otherGuard = std::move(testGuard);
		CHECK(&testFlag == otherGuard.return_flag_ptr());
		CHECK(otherGuard.is_locked() == true);
		CHECK(nullptr == testGuard.return_flag_ptr());
	}

	SUBCASE("move constructs correctly")
	{
		auto otherGuard = AtomicGuards::AtomicFlagGuard(std::move(testGuard));
		CHECK(&testFlag == otherGuard.return_flag_ptr());
		CHECK(otherGuard.is_locked() == false);
		CHECK(nullptr == testGuard.return_flag_ptr());
	}
}
