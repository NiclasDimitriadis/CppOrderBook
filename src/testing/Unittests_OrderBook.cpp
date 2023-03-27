#define DOCTEST_CONFIG_IMPLEMENT 

#include "Unittest_Includes.hpp"


TEST_CASE("testing OrderBook::orderBook") {
  using msgClassVariant = std::variant<FIXmsgClasses::addLimitOrder,
                                       FIXmsgClasses::withdrawLimitOrder,
                                       FIXmsgClasses::marketOrder>;
  using testOrderBookClass =
      OrderBook::orderBook<msgClassVariant, std::int64_t, 1000, false, 0, 1, 2>;

  SUBCASE("testing for correct behavior of bestBidAsk") {
    auto testOrderBook = testOrderBookClass{0};
    auto bestBidAsk = testOrderBook.bestBidAsk();
    CHECK(!std::get<0>(bestBidAsk).has_value());
    CHECK(!std::get<1>(bestBidAsk).has_value());
    msgClassVariant testMsgClassVar;
    testMsgClassVar.emplace<0>(1000, 101);
    testOrderBook.processOrder(testMsgClassVar);
    testMsgClassVar.emplace<0>(-1000, 99);
    testOrderBook.processOrder(testMsgClassVar);
    bestBidAsk = testOrderBook.bestBidAsk();
    CHECK(std::get<0>(bestBidAsk).value() == 99);
    CHECK(std::get<1>(bestBidAsk).value() == 101);
  }

  SUBCASE(
      "testing regular and out of bounds limit order adding and withdrawel") {
    auto testOrderBook = testOrderBookClass{5};
    msgClassVariant testMsgClassVar;
    // adding limit order
    testMsgClassVar.emplace<0>(1000, 100);
    auto orderResp = testOrderBook.processOrder(testMsgClassVar);
    CHECK(std::get<0>(orderResp) == 0);
    CHECK(std::get<1>(orderResp) == 0);
    CHECK(std::get<2>(orderResp) == 0);
    CHECK(std::get<3>(orderResp) == 0);
    CHECK(testOrderBook.volumeAtPrice(100) == 1000);
    // withdrawing limit order
    testMsgClassVar.emplace<1>(1000, 100);
    orderResp = testOrderBook.processOrder(testMsgClassVar);
    CHECK(std::get<0>(orderResp) == 0);
    CHECK(std::get<1>(orderResp) == 1000);
    CHECK(std::get<2>(orderResp) == 0);
    CHECK(std::get<3>(orderResp) == 0);
    CHECK(testOrderBook.volumeAtPrice(100) == 0);
    // repeat with negative volume
    // adding limit order
    testMsgClassVar.emplace<0>(-1000, 100);
    orderResp = testOrderBook.processOrder(testMsgClassVar);
    CHECK(std::get<0>(orderResp) == 0);
    CHECK(std::get<1>(orderResp) == 0);
    CHECK(std::get<2>(orderResp) == 0);
    CHECK(std::get<3>(orderResp) == 0);
    CHECK(testOrderBook.volumeAtPrice(100) == -1000);
    // withdrawing limit order
    testMsgClassVar.emplace<1>(-1000, 100);
    orderResp = testOrderBook.processOrder(testMsgClassVar);
    CHECK(std::get<0>(orderResp) == 0);
    CHECK(std::get<1>(orderResp) == -1000);
    CHECK(std::get<2>(orderResp) == 0);
    CHECK(std::get<3>(orderResp) == 0);
    CHECK(testOrderBook.volumeAtPrice(100) == 0);
    // try adding order out of bounds
    testMsgClassVar.emplace<0>(1000, 2000);
    orderResp = testOrderBook.processOrder(testMsgClassVar);
    CHECK(std::get<0>(orderResp) == 0);
    CHECK(std::get<1>(orderResp) == 0);
    CHECK(std::get<2>(orderResp) == 0);
    CHECK(std::get<3>(orderResp) == 1);
  }

  SUBCASE("testing immediate limit order matching") {
    auto testOrderBook = testOrderBookClass{0};
    msgClassVariant testMsgClassVar;
    testMsgClassVar.emplace<0>(-500, 100);
    testOrderBook.processOrder(testMsgClassVar);
    testMsgClassVar.emplace<0>(-500, 101);
    testOrderBook.processOrder(testMsgClassVar);
    testMsgClassVar.emplace<0>(2000, 98);
    auto orderResp = testOrderBook.processOrder(testMsgClassVar);
    CHECK(std::get<0>(orderResp) == 100);
    CHECK(std::get<1>(orderResp) == 1000);
    CHECK(std::get<2>(orderResp) == 101 * 500 + 100 * 500);
    CHECK(testOrderBook.volumeAtPrice(101) == 0);
    CHECK(testOrderBook.volumeAtPrice(100) == 0);
    CHECK(testOrderBook.volumeAtPrice(98) == 1000);
    // repeat with opposite signs
    testMsgClassVar.emplace<0>(500, 100);
    testOrderBook.processOrder(testMsgClassVar);
    testMsgClassVar.emplace<0>(500, 101);
    testOrderBook.processOrder(testMsgClassVar);
    testMsgClassVar.emplace<0>(-2000, 103);
    orderResp = testOrderBook.processOrder(testMsgClassVar);
    CHECK(std::get<0>(orderResp) == 101);
    CHECK(std::get<1>(orderResp) == -2000);
    CHECK(std::get<2>(orderResp) == 98 * -1000 + 101 * -500 + 100 * -500);
    CHECK(testOrderBook.volumeAtPrice(98) == 0);
    CHECK(testOrderBook.volumeAtPrice(101) == 0);
    CHECK(testOrderBook.volumeAtPrice(100) == 0);
    CHECK(testOrderBook.volumeAtPrice(103) == 0);
  }

  SUBCASE(
      "testing correct execution of market order with sufficient liquidity") {
    auto testOrderBook = testOrderBookClass{0};
    msgClassVariant testMsgClassVar;
    for (int i = 0; i < 10; ++i) {
      testMsgClassVar.emplace<0>(1000, 101 + i);
      testOrderBook.processOrder(testMsgClassVar);
      testMsgClassVar.emplace<0>(-1000, 99 - i);
      testOrderBook.processOrder(testMsgClassVar);
    };
    auto bestBidAsk = testOrderBook.bestBidAsk();
    CHECK(std::get<0>(bestBidAsk).value() == 99);
    CHECK(std::get<1>(bestBidAsk).value() == 101);
    // fill negative volume/demand
    testMsgClassVar.emplace<2>(-3500);
    auto orderResp = testOrderBook.processOrder(testMsgClassVar);
    CHECK(std::get<0>(orderResp) == 104);
    CHECK(std::get<1>(orderResp) == -3500);
    CHECK(std::get<2>(orderResp) ==
          (-1000 * 101 - 1000 * 102 - 1000 * 103 - 500 * 104));
    CHECK(testOrderBook.volumeAtPrice(101) == 0);
    CHECK(testOrderBook.volumeAtPrice(104) == 500);
    bestBidAsk = testOrderBook.bestBidAsk();
    CHECK(std::get<1>(bestBidAsk).value() == 104);
    // fill positive volume/ supply
    testMsgClassVar.emplace<2>(3500);
    orderResp = testOrderBook.processOrder(testMsgClassVar);
    CHECK(std::get<0>(orderResp) == 96);
    CHECK(std::get<1>(orderResp) == 3500);
    CHECK(std::get<2>(orderResp) ==
          (1000 * 99 + 1000 * 98 + 1000 * 97 + 500 * 96));
    CHECK(testOrderBook.volumeAtPrice(99) == 0);
    CHECK(testOrderBook.volumeAtPrice(96) == -500);
    bestBidAsk = testOrderBook.bestBidAsk();
    CHECK(std::get<0>(bestBidAsk).value() == 96);
  }

  SUBCASE(
      "testing correct execution of market order with insufficient liquidity") {
    auto testOrderBook = testOrderBookClass{0};
    msgClassVariant testMsgClassVar;
    for (int i = 0; i < 3; ++i) {
      testMsgClassVar.emplace<0>(1000, 101 + i);
      testOrderBook.processOrder(testMsgClassVar);
      testMsgClassVar.emplace<0>(-1000, 99 - i);
      testOrderBook.processOrder(testMsgClassVar);
    };
    auto bestBidAsk = testOrderBook.bestBidAsk();
    CHECK(std::get<0>(bestBidAsk).value() == 99);
    CHECK(std::get<1>(bestBidAsk).value() == 101);
    // fill negative volume/demand
    testMsgClassVar.emplace<2>(-3500);
    auto orderResp = testOrderBook.processOrder(testMsgClassVar);
    CHECK(std::get<0>(orderResp) == 103);
    CHECK(std::get<1>(orderResp) == -3000);
    CHECK(std::get<2>(orderResp) == (-1000 * 101 - 1000 * 102 - 1000 * 103));
    CHECK(testOrderBook.volumeAtPrice(101) == 0);
    CHECK(testOrderBook.volumeAtPrice(103) == 0);
    bestBidAsk = testOrderBook.bestBidAsk();
    CHECK(!std::get<1>(bestBidAsk).has_value());
    // fill positive volume/ supply
    testMsgClassVar.emplace<2>(3500);
    orderResp = testOrderBook.processOrder(testMsgClassVar);
    CHECK(std::get<0>(orderResp) == 97);
    CHECK(std::get<1>(orderResp) == 3000);
    CHECK(std::get<2>(orderResp) == (1000 * 99 + 1000 * 98 + 1000 * 97));
    CHECK(testOrderBook.volumeAtPrice(99) == 0);
    CHECK(testOrderBook.volumeAtPrice(97) == 00);
    bestBidAsk = testOrderBook.bestBidAsk();
    CHECK(!std::get<0>(bestBidAsk).has_value());
  }

  SUBCASE("testing for correct behavior when shifting the order book") {
    auto testOrderBook = testOrderBookClass{100};
    msgClassVariant testMsgClassVar;
    CHECK(!testOrderBook.shiftBook(-200));
    testMsgClassVar.emplace<0>(-10, 500);
    testOrderBook.processOrder(testMsgClassVar);
    testMsgClassVar.emplace<0>(-10, 1400);
    auto orderResp = testOrderBook.processOrder(testMsgClassVar);
    CHECK(std::get<3>(orderResp) == 1);
    orderResp = testOrderBook.processOrder(testMsgClassVar);
    CHECK(testOrderBook.shiftBook(300));
    CHECK(testOrderBook.volumeAtPrice(500) == -10);
    CHECK(testOrderBook.shiftBook(-300));
    CHECK(testOrderBook.volumeAtPrice(500) == -10);
    testOrderBook.shiftBook(300);
    testMsgClassVar.emplace<0>(-10, 1400);
    orderResp = testOrderBook.processOrder(testMsgClassVar);
    CHECK(std::get<3>(orderResp) == 0);
    testMsgClassVar.emplace<0>(-10, 400);
    testOrderBook.processOrder(testMsgClassVar);
    CHECK(testOrderBook.volumeAtPrice(400) == -10);
    CHECK(testOrderBook.volumeAtPrice(1400) == -10);
    CHECK(!testOrderBook.shiftBook(-1));
    CHECK(!testOrderBook.shiftBook(1));
  }

  SUBCASE("testing concurrent writes to orderbook") {
    auto testOrderBook = testOrderBookClass(0);
    msgClassVariant testMsgClassVar_0;
    testMsgClassVar_0.emplace<0>(-1, 100);
    msgClassVariant testMsgClassVar_1;
    testMsgClassVar_1.emplace<0>(-1, 100);
    std::atomic<bool> startSignal{false};
    std::thread thread_0([&]() {
      while (!startSignal.load(std::memory_order_acquire)) {
      };
      for (int i = 0; i < 10000000; ++i) {
        testOrderBook.processOrder(testMsgClassVar_0);
      }
    });
    std::thread thread_1([&]() {
      while (!startSignal.load(std::memory_order_acquire)) {
      };
      for (int i = 0; i < 10000000; ++i) {
        testOrderBook.processOrder(testMsgClassVar_1);
      }
    });
    startSignal.store(true);
    thread_0.join();
    thread_1.join();
    CHECK(testOrderBook.volumeAtPrice(100) == -20000000);
  }

  SUBCASE(
      "brute-force-testing order book invariants using 1 million randomly "
      "generated erratic yet valid messages") {
    using sockHandler =
        FIXSocketHandler::fixSocketHandler<msgClassVariant,
                                           FIXmockSocket::fixMockSocket>;
    using lineTuple = std::tuple<std::uint8_t, std::int32_t, std::uint32_t>;
    auto testOrderBook1 = testOrderBookClass{1000};
    auto testOrderBook2 = testOrderBookClass{1000};
    auto tupleVec =
        FileToTuples::fileToTuples<lineTuple>("../RandTestDataErratic.csv");
    auto mockSocket = FIXmockSocket::fixMockSocket(tupleVec);
    auto sHandler = sockHandler(&mockSocket);
    std::optional<msgClassVariant> readRet;
    std::uint64_t errCodeSum{0};
    for (int i = 0; i < 1000000; ++i) {
      readRet = sHandler.readNextMessage();
      testOrderBook1.processOrder(readRet.value());
      errCodeSum += testOrderBook1.__invariantsCheck(i);
      testOrderBook2.processOrder(readRet.value());
    }
    CHECK(errCodeSum == 0);
  }
}


int main() {
  doctest::Context context;
  context.run();
} 
