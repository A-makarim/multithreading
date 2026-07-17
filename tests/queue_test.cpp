#include "bounded_queue.hpp"

#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <thread>
#include <vector>

int main() {
  constexpr int producers = 4;
  constexpr int consumers = 4;
  constexpr int per_producer = 2'000;
  constexpr int total = producers * per_producer;
  threadforge::BoundedQueue<int> queue(17);
  std::array<std::atomic<int>, total> seen{};

  std::vector<std::thread> consumer_threads;
  for (int i = 0; i < consumers; ++i) {
    consumer_threads.emplace_back([&] {
      while (auto value = queue.pop()) seen.at(static_cast<std::size_t>(*value)).fetch_add(1);
    });
  }
  std::vector<std::thread> producer_threads;
  for (int producer = 0; producer < producers; ++producer) {
    producer_threads.emplace_back([&, producer] {
      for (int i = 0; i < per_producer; ++i)
        assert(queue.push(producer * per_producer + i));
    });
  }
  for (auto& thread : producer_threads) thread.join();
  queue.close();
  for (auto& thread : consumer_threads) thread.join();
  for (const auto& count : seen) assert(count.load() == 1);

  threadforge::BoundedQueue<int> blocked_consumer(1);
  std::atomic<bool> consumer_woke{false};
  std::thread waiting_consumer([&] {
    assert(!blocked_consumer.pop());
    consumer_woke.store(true);
  });
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  blocked_consumer.close();
  waiting_consumer.join();
  assert(consumer_woke.load());

  threadforge::BoundedQueue<int> blocked_producer(1);
  assert(blocked_producer.push(1));
  std::atomic<bool> producer_woke{false};
  std::thread waiting_producer([&] {
    assert(!blocked_producer.push(2));
    producer_woke.store(true);
  });
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  blocked_producer.close();
  waiting_producer.join();
  assert(producer_woke.load());
}
