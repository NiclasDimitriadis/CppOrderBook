#define DOCTEST_CONFIG_IMPLEMENT

#include "Unittest_Includes.hpp"

TEST_CASE("testing SeqLockQueue::seqLockQueue") {
  SUBCASE("testing enqueueing and dequeueing with shared cachline") {
    using slqClass = SeqLockQueue::seqLockQueue<int, 8, true>;
    slqClass testSlq;
    CHECK(!testSlq.dequeue().has_value());
    int enqSum = 0;
    for (int i = 0; i < 8; ++i) {
      testSlq.enqueue(i);
      enqSum += i;
    };
    auto deqRes = testSlq.dequeue();
    CHECK(deqRes.value() == 0);
    int deqSum = 0;
    for (int i = 0; i < 7; ++i) {
      deqSum += testSlq.dequeue().value();
    };
    CHECK(enqSum == deqSum);
    testSlq.enqueue(123);
    CHECK(testSlq.dequeue().value() == 123);
  }

  SUBCASE("testing enqueueing and dequeueing with shared cachline") {
    using slqClass = SeqLockQueue::seqLockQueue<int, 4, false>;
    slqClass testSlq{};
    CHECK(!testSlq.dequeue().has_value());
    int enqSum = 0;
    for (int i = 0; i < 4; ++i) {
      testSlq.enqueue(i);
      enqSum += i;
    };
    auto deqRes = testSlq.dequeue();
    CHECK(deqRes.value() == 0);
    int deqSum = 0;
    for (int i = 0; i < 3; ++i) {
      deqSum += testSlq.dequeue().value();
    };
    CHECK(enqSum == deqSum);
    testSlq.enqueue(123);
    CHECK(testSlq.dequeue().value() == 123);
  }

  SUBCASE(
      "testing for correct behavior under concurrent enqueueing and "
      "dequeueing") {
    static constexpr std::uint32_t nElements = 128 * 1048576;
    using slqClass = SeqLockQueue::seqLockQueue<int, nElements, true>;
    slqClass testSlq;
    std::int64_t enqSum{0}, deqSum{0};
    std::atomic_flag startSignal{false};

    std::thread enqThread([&]() {
      std::srand(std::time(nullptr));
      int randInt{0};
      while (!startSignal.test())
        ;
      for (int i = 0; i < nElements; ++i) {
        randInt = std::rand();
        testSlq.enqueue(randInt);
        enqSum += randInt;
      }
    });

    std::thread deqThread([&]() {
      std::optional<int> deqRes;
      int nIter{0};
      while (!startSignal.test())
        ;
      while (nIter < nElements) {
        deqRes = testSlq.dequeue();
        if (deqRes.has_value()) {
          deqSum += deqRes.value();
          ++nIter;
        }
      }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    startSignal.test_and_set();
    enqThread.join();
    deqThread.join();

    CHECK(enqSum == deqSum);
  }
}

int main() {
  doctest::Context context;
  context.run();
}
