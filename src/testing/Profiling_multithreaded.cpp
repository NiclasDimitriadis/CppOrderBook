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
#include "Queue.hpp"
#include "busyWait.hpp"

using lineTuple = std::tuple<std::uint8_t, std::int32_t, std::uint32_t>;
using msgClassVar =
    std::variant<FIXmsgClasses::addLimitOrder,
                 FIXmsgClasses::withdrawLimitOrder, FIXmsgClasses::marketOrder>;
static constexpr std::uint32_t twoPow20 = 1048576;
using ordBook =
    OrderBook::orderBook<msgClassVar, std::int64_t, 6000, false, 0, 1, 2>;
using seqLockQueue = Queue::SeqLockQueue<msgClassVar, twoPow20, false, false>;
using sockHandler =
    FIXSocketHandler::fixSocketHandler<msgClassVar,
                                       FIXmockSocket::fixMockSocket>;
using timePoint = decltype(std::chrono::high_resolution_clock::now());

FIXmockSocket::fixMockSocket createMockSocket(const std::string& csvPath,
                                              std::atomic_flag* flagPtr) {
  auto tupleVec = FileToTuples::fileToTuples<lineTuple>(csvPath);
  return FIXmockSocket::fixMockSocket(tupleVec, flagPtr);
}

void enqueueLogic(seqLockQueue* queuePtr, const std::string& csvPath,
                  std::atomic_flag* endOfBufferFlag, int coreIndex,
                  std::atomic_flag* signalFlag,
                  std::vector<timePoint>* startTimes) noexcept {
  // set up CPU-affinity for enqueueing thread
  if (coreIndex > -1) {
    cpu_set_t cpuSet;
    CPU_ZERO(&cpuSet);
    CPU_SET(coreIndex, &cpuSet);
    int setAffRet =
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuSet);
    if (setAffRet != 0) {
      std::cout
          << "setting up CPU-affinity to enqueueing thread failed. teminating."
          << "\n";
      std::terminate();
    }
  }

  // set up objects for reading messages
  auto mockSock = createMockSocket(csvPath, endOfBufferFlag);
  auto sHandler = sockHandler(&mockSock);
  std::optional<msgClassVar> readRet;

  // coordinate with dequeueing thread
  signalFlag->test_and_set();
  while (signalFlag->test()) {
  };

  // read all messages for socket and enqueue them to be processed by other
  // thread
  // auto sinceEp =
  // std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
  int index{0};
  timePoint startTime;

  while (!endOfBufferFlag->test(std::memory_order_acquire)) {
    startTime = std::chrono::high_resolution_clock::now();
    readRet = sHandler.readNextMessage();
    queuePtr->enqueue(readRet.value());
    (*startTimes)[index] = startTime;
    ++index;
    BusyWait::busyWait(50);
  }
}

int main(int argc, char* argv[]) {
  // process and validate command line arguments
  std::string csvPath(argv[1]);
  int enqCoreIndex = -1;
  int deqCoreIndex = -1;
  if (argc == 4) {
    int nCores = std::thread::hardware_concurrency();
    enqCoreIndex = std::atoi(argv[2]);
    deqCoreIndex = std::atoi(argv[3]);
    bool invalidIndex = enqCoreIndex < 0 || enqCoreIndex > nCores ||
                        deqCoreIndex < 0 || deqCoreIndex > nCores;
    if (invalidIndex) {
      std::cout << "invalid CPU-index argument. teminating."
                << "\n";
      std::terminate();
    }

    // set up affinity for dequeueing thread
    cpu_set_t cpuSet;
    CPU_ZERO(&cpuSet);
    CPU_SET(deqCoreIndex, &cpuSet);
    int setAffRet =
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuSet);
    if (setAffRet != 0) {
      std::cout
          << "setting up CPU-affinity to dequeueing thread failed. teminating."
          << "\n";
      std::terminate();
    }
  }
  int nMsgs(FileToTuples::fileToTuples<lineTuple>(csvPath).size());

  // give entire cacheline to end-of-buffer flag to avoid false sharing
  alignas(64) std::atomic_flag endOfBuffer{false};
  std::atomic_flag signalFlag{false};
  std::vector<timePoint> startTimes(nMsgs);

  // set up objects/variables to dequeueing messages and inserting them into
  // order book
  alignas(64) auto book = ordBook(7000);
  seqLockQueue slq;
  auto reader = slq.get_reader();
  std::vector<timePoint> completionTimes(nMsgs);
  std::optional<msgClassVar> deqRet;

  // launch enqueueing thread
  std::thread enqThread(enqueueLogic, &slq, csvPath, &endOfBuffer, enqCoreIndex,
                        &signalFlag, &startTimes);

  while (!signalFlag.test()) {
  };
  int index{0};
  signalFlag.clear();
  timePoint completionTime;

  // dequeue messages and insert them into order book
  do {
    deqRet = reader.read_next_entry();
    if (deqRet.has_value()) {
      book.processOrder(deqRet.value());
      completionTime = std::chrono::high_resolution_clock::now();
      completionTimes[index] = completionTime;
      ++index;
    };
  } while (
      !(endOfBuffer.test(std::memory_order_acquire) && !deqRet.has_value()));

  enqThread.join();

  // use current time as seed for random generator, generate random index
  std::srand(std::time(nullptr));
  int randomPrice = 7000 + (std::rand() % 6000);

  // query volume at random index to prohibit compiler from optimizing away the
  // program logic
  std::cout << "volume at randomly generated price " << randomPrice << ": "
            << book.volumeAtPrice(randomPrice) << "\n";

  // compute latencies
  std::vector<double> latencies(nMsgs);
  for (int i = 0; i < nMsgs; ++i) {
    latencies[i] = static_cast<double>(duration_cast<std::chrono::nanoseconds>(
                                           completionTimes[i] - startTimes[i])
                                           .count());
  }

  // export latencies to csv
  {
    std::ofstream csv("latencies_multithreaded.csv");
    for (auto&& elem : latencies) {
      csv << elem << "\n";
    }
  }
}
