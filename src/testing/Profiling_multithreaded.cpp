#include <pthread.h>
#include <sched.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <optional>
#include <string>
#include <thread>
#include <variant>
#include <vector>

#include "FIXSocketHandler.hpp"
#include "FIXmockSocket.hpp"
#include "FIXmsgClasses.hpp"
#include "FileToTuples.hpp"
#include "OrderBook.hpp"
#include "SeqLockQueue.hpp"

using lineTuple = std::tuple<std::uint8_t, std::int32_t, std::uint32_t>;
using msgClassVar =
    std::variant<FIXmsgClasses::addLimitOrder,
                 FIXmsgClasses::withdrawLimitOrder, FIXmsgClasses::marketOrder>;
static constexpr std::uint32_t twoPow20 = 1048576;
using ordBook =
    OrderBook::orderBook<msgClassVar, std::int64_t, 6000, false, 0, 1, 2>;
using seqLockQueue = SeqLockQueue::seqLockQueue<msgClassVar, twoPow20, false>;
using sockHandler =
    FIXSocketHandler::fixSocketHandler<msgClassVar,
                                       FIXmockSocket::fixMockSocket>;
using timePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;

FIXmockSocket::fixMockSocket createMockSocket(const std::string& csvPath,
                                              std::atomic_flag* flagPtr) {
  auto tupleVec = FileToTuples::fileToTuples<lineTuple>(csvPath);
  return FIXmockSocket::fixMockSocket(tupleVec, flagPtr);
}

void enqueueLogic(seqLockQueue* queuePtr, const std::string& csvPath,
                  std::atomic_flag* endOfBufferFlag,
                  std::atomic_flag* signalFlag,
                  timePoint* startTimePtr) noexcept {
  // set up objects for reading messages
  auto mockSock = createMockSocket(csvPath, endOfBufferFlag);
  auto sHandler = sockHandler(&mockSock);
  std::optional<msgClassVar> readRet;

  // coordinate with dequeueing thread
  signalFlag->test_and_set();
  while (signalFlag->test()) {
  };

  // take start time
  auto startTime = std::chrono::high_resolution_clock::now();

  // read all messages for socket and enqueue them to be processed by other
  // thread
  while (!endOfBufferFlag->test(std::memory_order_acquire)) {
    readRet = sHandler.readNextMessage();
    if (readRet.has_value()) {
      queuePtr->enqueue(readRet.value());
    };
  }

  //"return" start time
  *startTimePtr = startTime;
}

int main(int argc, char* argv[]) {
  // process path argumement
  std::string csvPath(argv[1]);

  // give entire cacheline to end-of-buffer flag to avoid false sharing
  alignas(64) std::atomic_flag endOfBuffer{false};
  std::atomic_flag signalFlag{false};
  timePoint startTime;

  // set up objects/variables to dequeueing messages and inserting them into
  // order book
  alignas(64) auto book = ordBook(7000);
  seqLockQueue slq;
  std::optional<msgClassVar> deqRet;

  // launch enqueueing thread
  std::thread enqThread(enqueueLogic, &slq, csvPath, &endOfBuffer, &signalFlag,
                        &startTime);
  // enqueueLogic(&slq, csvPath, &endOfBuffer, enqCoreIndex, &signalFlag,
  // &startTime);

  while (!signalFlag.test()) {
  };
  signalFlag.clear();

  // dequeue messages and insert them into order book
  do {
    deqRet = slq.dequeue();
    if (deqRet.has_value()) {
      book.processOrder(deqRet.value());
    };
  } while (
      !(endOfBuffer.test(std::memory_order_acquire) && !deqRet.has_value()));

  // take finishing time
  auto completionTime = std::chrono::high_resolution_clock::now();

  enqThread.join();

  // use current time as seed for random generator, generate random index
  std::srand(std::time(nullptr));
  int randomPrice = 7000 + (std::rand() % 6000);

  // query volume at random index to prohibit compiler from optimizing away the
  // program logic
  std::cout << "volume at randomly generated price " << randomPrice << ": " 
     << book.volumeAtPrice(randomPrice) << "\n";

  std::cout << "execution took " << 
     duration_cast<std::chrono::microseconds>(completionTime - startTime).count() << 
     " microseconds" << "\n";


  return 0;
}