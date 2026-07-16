#pragma once
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <queue>
#include <utility>

// A thread-safe, fixed-capacity queue. push() blocks while full, pop() blocks
// while empty. One mutex guards the data; two condition variables let threads
// sleep (not spin) until there's room / an item.
template <class T>
class BoundedQueue {
 public:
  explicit BoundedQueue(std::size_t capacity) : capacity_(capacity) {}

  void push(T item) {
    std::unique_lock<std::mutex> lock(mutex_);
    not_full_.wait(lock, [this] { return queue_.size() < capacity_; });
    queue_.push(std::move(item));
    lock.unlock();
    not_empty_.notify_one();
  }

  T pop() {
    std::unique_lock<std::mutex> lock(mutex_);
    not_empty_.wait(lock, [this] { return !queue_.empty(); });
    T item = std::move(queue_.front());
    queue_.pop();
    lock.unlock();
    not_full_.notify_one();
    return item;
  }

 private:
  std::size_t capacity_;
  std::queue<T> queue_;
  std::mutex mutex_;
  std::condition_variable not_full_;
  std::condition_variable not_empty_;
};
