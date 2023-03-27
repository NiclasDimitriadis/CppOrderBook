#define DOCTEST_CONFIG_IMPLEMENT 

#include "Unittest_Includes.hpp"


TEST_CASE("testing FIXmockSocket::fixMockSocket") {
  using lineTuple = std::tuple<std::uint8_t, std::int32_t, std::uint32_t>;
  auto msgVec = FileToTuples::fileToTuples<lineTuple>("../testMsgCsv.csv");
  std::atomic_flag emptyBuffer{false};
  auto mockSocket = FIXmockSocket::fixMockSocket(msgVec, &emptyBuffer);

  SUBCASE("testing reading new limit order message from mock socket") {
    std::uint8_t destBuffer[20];
    std::span<std::uint8_t, 20> dbs(destBuffer, 20);
    auto recvRet = mockSocket.recv(destBuffer, 6);
    CHECK(recvRet == 6);
    CHECK(*reinterpret_cast<std::uint32_t*>(&destBuffer[0]) == 17);
    CHECK(*reinterpret_cast<std::uint16_t*>(&destBuffer[4]) == 0xEB50);
    recvRet = mockSocket.recv(&destBuffer[6], 11);
    CHECK(recvRet == 11);
    CHECK(*reinterpret_cast<std::int32_t*>(&destBuffer[6]) == -11);
    CHECK(*reinterpret_cast<std::uint32_t*>(&destBuffer[10]) == 111);
    std::uint8_t byteSum{0};
    for (int i = 0; i < 14; ++i) {
      byteSum += destBuffer[i];
    };
    std::uint8_t fstDigit = (byteSum / 100);
    std::uint8_t scndDigit = (byteSum - (fstDigit * 100)) / 10;
    std::uint8_t thrdDigit = byteSum - (fstDigit * 100 + scndDigit * 10);
    CHECK(destBuffer[14] == fstDigit + 48);
    CHECK(destBuffer[15] == scndDigit + 48);
    CHECK(destBuffer[16] == thrdDigit + 48);
  }

  SUBCASE("testing reading withdraw limit order message from mock socket") {
    std::uint8_t destBuffer[20];
    // read first message to remove it from socket
    auto recvRet = mockSocket.recv(destBuffer, 17);
    recvRet = mockSocket.recv(destBuffer, 6);
    CHECK(recvRet == 6);
    CHECK(*reinterpret_cast<std::uint32_t*>(&destBuffer[0]) == 18);
    CHECK(*reinterpret_cast<std::uint16_t*>(&destBuffer[4]) == 0xEB50);
    recvRet = mockSocket.recv(&destBuffer[6], 12);
    CHECK(recvRet == 12);
    CHECK(*reinterpret_cast<std::int32_t*>(&destBuffer[6]) == 22);
    CHECK(*reinterpret_cast<std::uint32_t*>(&destBuffer[10]) == 222);
    CHECK(*reinterpret_cast<std::uint8_t*>(&destBuffer[14]) == 0xFF);
    std::uint8_t byteSum{0};
    for (int i = 0; i < 15; ++i) {
      byteSum += destBuffer[i];
    };
    std::uint8_t fstDigit = (byteSum / 100);
    std::uint8_t scndDigit = (byteSum - (fstDigit * 100)) / 10;
    std::uint8_t thrdDigit = byteSum - (fstDigit * 100 + scndDigit * 10);
    CHECK(destBuffer[15] == fstDigit + 48);
    CHECK(destBuffer[16] == scndDigit + 48);
    CHECK(destBuffer[17] == thrdDigit + 48);
  }

  SUBCASE("testing reading market order message from mock socket") {
    std::uint8_t destBuffer[20];
    // read first two messages to remove them from socket
    auto recvRet = mockSocket.recv(destBuffer, 17);
    recvRet = mockSocket.recv(destBuffer, 18);
    recvRet = mockSocket.recv(destBuffer, 6);
    CHECK(recvRet == 6);
    CHECK(*reinterpret_cast<std::uint32_t*>(&destBuffer[0]) == 13);
    CHECK(*reinterpret_cast<std::uint16_t*>(&destBuffer[4]) == 0xEB50);
    recvRet = mockSocket.recv(&destBuffer[6], 7);
    CHECK(recvRet == 7);
    CHECK(*reinterpret_cast<std::int32_t*>(&destBuffer[6]) == 33);
    std::uint8_t byteSum{0};
    for (int i = 0; i < 10; ++i) {
      byteSum += destBuffer[i];
    };
    std::uint8_t fstDigit = (byteSum / 100);
    std::uint8_t scndDigit = (byteSum - (fstDigit * 100)) / 10;
    std::uint8_t thrdDigit = byteSum - (fstDigit * 100 + scndDigit * 10);
    CHECK(destBuffer[10] == fstDigit + 48);
    CHECK(destBuffer[11] == scndDigit + 48);
    CHECK(destBuffer[12] == thrdDigit + 48);
  }

  SUBCASE("testing correct setting of flag indicating empty buffer") {
    std::uint8_t destBuffer[100];
    mockSocket.recv(&destBuffer[0], 100);
    CHECK(!emptyBuffer.test());
    mockSocket.recv(&destBuffer[0], 56);
    CHECK(emptyBuffer.test());
  }
}


int main() {
  doctest::Context context;
  context.run();
} 
