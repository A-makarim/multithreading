#pragma once
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>
#include <utility>

namespace threadforge {

// Fixed-capacity blocking queue. The bound is intentional back-pressure: a slow
// worker pool cannot let networking threads allocate unbounded memory under load.
template <class T>
class BoundedQueue {
 public:
  explicit BoundedQueue(std::size_t capacity) : capacity_(capacity) {
    if (capacity_ == 0) throw std::invalid_argument("capacity must be positive");
  }

  bool push(T item) {
    std::unique_lock<std::mutex> lock(mutex_);
    // Predicate waits handle spurious wakeups and sleep without burning CPU.
    not_full_.wait(lock, [this] { return closed_ || queue_.size() < capacity_; });
    if (closed_) return false;
    queue_.push(std::move(item));
    lock.unlock();
    not_empty_.notify_one();  // one item can wake one consumer; avoids thundering herd.
    return true;
  }

  std::optional<T> pop() {
    std::unique_lock<std::mutex> lock(mutex_);
    not_empty_.wait(lock, [this] { return closed_ || !queue_.empty(); });
    if (queue_.empty()) return std::nullopt;
    T item = std::move(queue_.front());
    queue_.pop();
    lock.unlock();
    not_full_.notify_one();
    return item;
  }

  void close() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      closed_ = true;
    }
    // Shutdown is global: wake every producer/consumer so all can observe closed_.
    not_empty_.notify_all();
    not_full_.notify_all();
  }

  std::size_t size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
  }

 private:
  std::size_t capacity_;
  std::queue<T> queue_;
  mutable std::mutex mutex_;
  std::condition_variable not_full_;
  std::condition_variable not_empty_;
  bool closed_ = false;
};

}  // namespace threadforge
