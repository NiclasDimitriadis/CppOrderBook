#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <tuple>
#include <variant>
#include <vector>

#include "FIXSocketHandler.hpp"
#include "FIXmockSocket.hpp"
#include "FIXmsgClasses.hpp"
#include "FileToTuples.hpp"
#include "OrderBook.hpp"

using lineTuple = std::tuple<std::uint8_t, std::int32_t, std::uint32_t>;
using msgClassVar =
    std::variant<FIXmsgClasses::addLimitOrder,
                 FIXmsgClasses::withdrawLimitOrder, FIXmsgClasses::marketOrder>;
static constexpr std::uint32_t twoPow20 = 1048576;
using ordBook =
    OrderBook::orderBook<msgClassVar, std::int64_t, 6000, false, 0, 1, 2>;
using sockHandler =
    FIXSocketHandler::fixSocketHandler<msgClassVar,
                                       FIXmockSocket::fixMockSocket>;
using timePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;

int main(int argc, char* argv[]) {
  // process and validate command line arguments
  std::string csvPath(argv[1]);
  if (argc == 3) {
    int nCores = std::thread::hardware_concurrency();
    int coreIndex = std::atoi(argv[2]);
    bool invalidIndex = coreIndex < 0 || coreIndex > nCores;
    if (invalidIndex) {
      std::cout << "invalid CPU-index argument. teminating."
                << "\n";
      std::terminate();
    }
    // set up affinity of thread
    cpu_set_t cpuSet;
    CPU_ZERO(&cpuSet);
    CPU_SET(coreIndex, &cpuSet);
    int setAffRet =
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuSet);
    if (setAffRet != 0) {
      std::cout << "setting up CPU-affinity failed. teminating."
                << "\n";
      std::terminate();
    }
  }
  // process path argumement, set up objects
  auto tupleVec = FileToTuples::fileToTuples<lineTuple>(csvPath);
  auto mockSock = FIXmockSocket::fixMockSocket(tupleVec, nullptr);
  auto sHandler = sockHandler(&mockSock);
  auto book = ordBook(7000);
  const int nMsgs = tupleVec.size();
  auto startTime = std::chrono::high_resolution_clock::now();
  auto completionTime = std::chrono::high_resolution_clock::now();
  std::optional<msgClassVar> readRet;

  double latency;

  std::vector<double> latencies(nMsgs);

  for (int i = 0; i < nMsgs; ++i) {
    startTime = std::chrono::high_resolution_clock::now();

    readRet = sHandler.readNextMessage();
    book.processOrder(readRet.value());
    completionTime = std::chrono::high_resolution_clock::now();
    latency = static_cast<double>(
        duration_cast<std::chrono::nanoseconds>(completionTime - startTime)
            .count());
    latencies[i] = latency;
  }

  // use current time as seed for random generator, generate random index
  std::srand(std::time(nullptr));
  int randomPrice = 7000 + (std::rand() % 6000);
  // query volume at random index to prohibit compiler from optimizing away the
  // program logic
  std::cout << "volume at randomly generated price " << randomPrice << ": "
            << book.volumeAtPrice(randomPrice) << "\n";

  // write vectors of all latencies to csv
  {
    std::ofstream csv("latencies_single_threaded.csv");
    for (auto&& elem : latencies) {
      csv << elem << "\n";
    }
  }
}