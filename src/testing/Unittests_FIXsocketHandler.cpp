#define DOCTEST_CONFIG_IMPLEMENT

#include "Unittest_Includes.hpp"

TEST_CASE("testing FIXsocketHandler::fixSocketHandler") {
  using msgClassVariant = std::variant<FIXmsgClasses::addLimitOrder,
                                       FIXmsgClasses::withdrawLimitOrder,
                                       FIXmsgClasses::marketOrder>;
  using sockHandlerClass =
      FIXSocketHandler::fixSocketHandler<msgClassVariant,
                                         FIXmockSocket::fixMockSocket>;
  using lineTuple = std::tuple<std::uint8_t, std::int32_t, std::uint32_t>;
  auto msgTuples = FileToTuples::fileToTuples<lineTuple>("../testMsgCsv.csv");
  auto mockSocket = FIXmockSocket::fixMockSocket(msgTuples);
  auto mockSocketPtr = &mockSocket;
  auto sockHandler = sockHandlerClass(mockSocketPtr);
  auto recvRes = sockHandler.readNextMessage();
  CHECK(recvRes.has_value());
  msgClassVariant recvContent = recvRes.value();
  CHECK(recvContent.index() == 0);
  CHECK(std::get<0>(recvContent).orderVolume == -11);
  CHECK(std::get<0>(recvContent).orderPrice == 111);

  recvRes = sockHandler.readNextMessage();
  CHECK(recvRes.has_value());
  recvContent = recvRes.value();
  CHECK(recvContent.index() == 1);
  CHECK(std::get<1>(recvContent).orderVolume == 22);
  CHECK(std::get<1>(recvContent).orderPrice == 222);

  recvRes = sockHandler.readNextMessage();
  CHECK(recvRes.has_value());
  recvContent = recvRes.value();
  CHECK(recvContent.index() == 2);
  CHECK(std::get<2>(recvContent).orderVolume == 33);

  recvRes = sockHandler.readNextMessage();
  CHECK(recvRes.has_value());
  recvContent = recvRes.value();
  CHECK(recvContent.index() == 0);
  CHECK(std::get<0>(recvContent).orderVolume == 1000);
  CHECK(std::get<0>(recvContent).orderPrice == 20);

  recvRes = sockHandler.readNextMessage();
  CHECK(recvRes.has_value());
  recvContent = recvRes.value();
  CHECK(recvContent.index() == 1);
  CHECK(std::get<1>(recvContent).orderVolume == -2000);
  CHECK(std::get<1>(recvContent).orderPrice == 50);

  recvRes = sockHandler.readNextMessage();
  CHECK(!recvRes.has_value());

  recvRes = sockHandler.readNextMessage();
  CHECK(recvRes.has_value());
  recvContent = recvRes.value();
  CHECK(recvContent.index() == 2);
  CHECK(std::get<2>(recvContent).orderVolume == 3000);
}

TEST_CASE("testing OrderBookBucket::orderBookBucket") {
  using testBucketClass = OrderBookBucket::orderBookBucket<std::int32_t, 8>;
  testBucketClass testBucket{};
  CHECK(testBucket.getVolume() == 0);
  testBucket.addLiquidity(1000);
  CHECK(testBucket.getVolume() == 1000);
  CHECK(testBucket.consumeLiquidity(2000) == 1000);
  CHECK(testBucket.getVolume() == 0);
  testBucket.addLiquidity(1000);
  testBucket.addLiquidity(-500);
  CHECK(testBucket.getVolume() == 500);
  testBucket.addLiquidity(-1000);
  CHECK(testBucket.getVolume() == -500);
  CHECK(testBucket.consumeLiquidity(1000) == 0);
  CHECK(testBucket.consumeLiquidity(-1000) == -500);
  CHECK(testBucket.getVolume() == 0);
}

int main() {
  doctest::Context context;
  context.run();
}
