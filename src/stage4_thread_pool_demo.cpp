#include <cstdio>
#include <future>
#include <vector>
#include "thread_pool.hpp"

// Submit 1,000 tasks (each computes i*i) to a pool of 4 threads, collect every
// future, sum the results, and check against the known answer.
int main() {
  constexpr int kTasks = 1000;
  ThreadPool pool(4);

  std::vector<std::future<long long>> futures;
  futures.reserve(kTasks);
  for (int i = 0; i < kTasks; ++i)
    futures.push_back(
        pool.submit([](int x) -> long long { return static_cast<long long>(x) * x; }, i));

  long long sum = 0;
  for (auto& f : futures) sum += f.get();  // blocks until each task is done

  long long expected = 0;
  for (int i = 0; i < kTasks; ++i) expected += static_cast<long long>(i) * i;

  std::printf("tasks=%d  sum=%lld  expected=%lld  -> %s\n", kTasks, sum, expected,
              sum == expected ? "OK" : "MISMATCH");
  return 0;
}
