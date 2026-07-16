#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>
#include "03_bounded_queue.hpp"

// 2 producers push 1..5000 each (10,000 items total); 3 consumers pop and sum.
// If nothing is lost or duplicated, the consumed sum and count match expected.
// A value of 0 is used as a "stop" sentinel (producers only push 1..5000).
int main() {
  constexpr int kProducers = 2;
  constexpr int kConsumers = 3;
  constexpr int kPerProducer = 5000;

  BoundedQueue<int> queue(128);
  std::atomic<long long> consumed_sum{0};
  std::atomic<int> consumed_count{0};

  std::vector<std::thread> consumers;
  for (int c = 0; c < kConsumers; ++c) {
    consumers.emplace_back([&] {
      for (;;) {
        int v = queue.pop();
        if (v == 0) break;  // sentinel -> this consumer stops
        consumed_sum += v;
        consumed_count += 1;
      }
    });
  }

  std::vector<std::thread> producers;
  for (int p = 0; p < kProducers; ++p) {
    producers.emplace_back([&] {
      for (int i = 1; i <= kPerProducer; ++i) queue.push(i);
    });
  }

  for (auto& p : producers) p.join();
  for (int c = 0; c < kConsumers; ++c) queue.push(0);  // one stop per consumer
  for (auto& c : consumers) c.join();

  const int expected_count = kProducers * kPerProducer;
  const long long expected_sum =
      static_cast<long long>(kProducers) * kPerProducer * (kPerProducer + 1) / 2;

  std::printf("items: expected=%d consumed=%d\n", expected_count, consumed_count.load());
  std::printf("sum:   expected=%lld consumed=%lld  -> %s\n", expected_sum,
              consumed_sum.load(),
              (expected_sum == consumed_sum.load() &&
               expected_count == consumed_count.load())
                  ? "OK"
                  : "MISMATCH");
  return 0;
}
