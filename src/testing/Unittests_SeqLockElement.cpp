#define DOCTEST_CONFIG_IMPLEMENT

#include "Unittest_Includes.hpp"

TEST_CASE("testing SeqLockElement::seqLockElement") {
  using testElementClass = SeqLockElement::seqLockElement<int, 8>;
  testElementClass testElement{};
  CHECK(!std::get<0>(testElement.read(1)).has_value());
  testElement.insert(123);
  auto readRet = testElement.read(1);
  CHECK(std::get<0>(readRet).value() == 123);
  CHECK(std::get<1>(readRet) == 2);
}

int main() {
  doctest::Context context;
  context.run();
}
