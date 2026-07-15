#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

constexpr int kThreads = 10;
constexpr int kIncrements = 100000;

// Baseline: no synchronisation (fast, but wrong).
long long run_racy() {
  int counter = 0;
  std::vector<std::thread> ts;
  for (int t = 0; t < kThreads; ++t)
    ts.emplace_back([&counter] { for (int i = 0; i < kIncrements; ++i) ++counter; });
  for (auto& t : ts) t.join();
  return counter;
}

// Version A: a mutex makes the increment a critical section (correct, slower).
long long run_mutex() {
  int counter = 0;
  std::mutex m;
  std::vector<std::thread> ts;
  for (int t = 0; t < kThreads; ++t)
    ts.emplace_back([&] {
      for (int i = 0; i < kIncrements; ++i) {
        std::lock_guard<std::mutex> lock(m);
        ++counter;
      }
    });
  for (auto& t : ts) t.join();
  return counter;
}

// Version B: an atomic increment is a single CPU-level operation (correct, fast).
long long run_atomic() {
  std::atomic<int> counter{0};
  std::vector<std::thread> ts;
  for (int t = 0; t < kThreads; ++t)
    ts.emplace_back([&counter] { for (int i = 0; i < kIncrements; ++i) ++counter; });
  for (auto& t : ts) t.join();
  return counter.load();
}

template <class F>
void bench(const char* label, F f) {
  constexpr int kRuns = 5;
  double total_ms = 0.0;
  long long last = 0;
  for (int r = 0; r < kRuns; ++r) {
    auto start = std::chrono::high_resolution_clock::now();
    last = f();
    auto end = std::chrono::high_resolution_clock::now();
    total_ms += std::chrono::duration<double, std::milli>(end - start).count();
  }
  std::printf("%-18s result=%lld  avg=%.2f ms\n", label, last, total_ms / kRuns);
}

int main() {
  bench("baseline (racy)", run_racy);
  bench("mutex", run_mutex);
  bench("atomic", run_atomic);
  return 0;
}
