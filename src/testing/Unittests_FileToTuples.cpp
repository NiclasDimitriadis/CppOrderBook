#define DOCTEST_CONFIG_IMPLEMENT

#include "Unittest_Includes.hpp"

TEST_CASE("testing FileToTuples::fileToTuples") {
  using lineTuple = std::tuple<std::uint8_t, std::int32_t, std::uint32_t>;
  auto msgTuples = FileToTuples::fileToTuples<lineTuple>("../testMsgCsv.csv");
  CHECK(std::get<0>(msgTuples[0]) == 0);
  CHECK(std::get<0>(msgTuples[1]) == 1);
  CHECK(std::get<0>(msgTuples[2]) == 2);
  CHECK(std::get<1>(msgTuples[1]) == 22);
  CHECK(std::get<2>(msgTuples[2]) == 333);
}

int main() {
  doctest::Context context;
  context.run();
}
