#define DOCTEST_CONFIG_IMPLEMENT

#include "Unittest_Includes.hpp"

TEST_CASE("testing AtomicGuards::AtomicFlagGuard") {
  std::atomic_flag testFlag{false};
  AtomicGuards::AtomicFlagGuard testGuard =
      AtomicGuards::AtomicFlagGuard(&testFlag);

  SUBCASE("binds to flag") { CHECK(&testFlag == testGuard.return_flag_ptr()); }

  SUBCASE("locks correctly") {
    CHECK(testGuard.is_locked() == false);
    testGuard.lock();
    CHECK(testFlag.test() == true);
    CHECK(testGuard.is_locked() == true);
  }

  SUBCASE("unlocks correctly") {
    testGuard.lock();
    testGuard.unlock();
    CHECK(testGuard.is_locked() == false);
    CHECK(testFlag.test() == false);
  }

  SUBCASE("rebinds correctly") {
    auto otherFlag = std::atomic_flag{false};
    testGuard.lock();
    testGuard.rebind(&otherFlag);
    CHECK(testFlag.test() == false);
    CHECK(&otherFlag == testGuard.return_flag_ptr());
  }

  SUBCASE("move assigns correctly") {
    testGuard.lock();
    auto otherGuard = AtomicGuards::AtomicFlagGuard(nullptr);
    otherGuard = std::move(testGuard);
    CHECK(&testFlag == otherGuard.return_flag_ptr());
    CHECK(otherGuard.is_locked() == true);
    CHECK(nullptr == testGuard.return_flag_ptr());
  }

  SUBCASE("move constructs correctly") {
    auto otherGuard = AtomicGuards::AtomicFlagGuard(std::move(testGuard));
    CHECK(&testFlag == otherGuard.return_flag_ptr());
    CHECK(otherGuard.is_locked() == false);
    CHECK(nullptr == testGuard.return_flag_ptr());
  }
}

int main() {
  doctest::Context context;
  context.run();
}
