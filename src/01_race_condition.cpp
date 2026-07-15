#include <cstdio>
#include <thread>
#include <vector>

// Stage 1 — prove the race. 10 threads each do ++counter 100,000 times with NO
// synchronisation. ++counter is really load-add-store; threads interleave and
// clobber each other's writes, so the total comes out too low (and different
// every run). Build at -O0 so the compiler doesn't fold the loop into one add.
int main() {
  constexpr int kThreads = 10;
  constexpr int kIncrements = 100000;

  int counter = 0;  // shared, deliberately unsynchronised

  std::vector<std::thread> threads;
  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([&counter] {
      for (int i = 0; i < kIncrements; ++i) ++counter;  // data race
    });
  }
  for (auto& th : threads) th.join();

  std::printf("expected=%d  actual=%d\n", kThreads * kIncrements, counter);
  return 0;
}
