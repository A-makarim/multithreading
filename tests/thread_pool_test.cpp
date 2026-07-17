#include "thread_pool.hpp"

#include <atomic>
#include <cassert>
#include <future>
#include <stdexcept>
#include <vector>

int main() {
  ThreadPool pool(4);
  std::atomic<int> executions{0};
  std::vector<std::future<int>> futures;
  for (int i = 0; i < 1'000; ++i) {
    futures.push_back(pool.submit([i, &executions] {
      executions.fetch_add(1);
      return i * i;
    }));
  }
  for (int i = 0; i < 1'000; ++i)
    assert(futures.at(static_cast<std::size_t>(i)).get() == i * i);
  assert(executions.load() == 1'000);

  pool.shutdown();
  bool rejected = false;
  try {
    (void)pool.submit([] {});
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  assert(rejected);
  pool.shutdown();

  bool rejected_zero = false;
  try {
    ThreadPool invalid(0);
  } catch (const std::invalid_argument&) {
    rejected_zero = true;
  }
  assert(rejected_zero);
}
