#pragma once
// wait for n nanoseconds without context switch

#include <chrono>
#include <cstdint>

namespace BusyWait {
void busyWait(std::uint64_t n) {
  auto startTime = std::chrono::high_resolution_clock::now();
  while (duration_cast<std::chrono::nanoseconds>(
             std::chrono::high_resolution_clock::now() - startTime)
             .count() < n) {
  };
}
}  // namespace BusyWait