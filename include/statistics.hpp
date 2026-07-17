#pragma once

#include <atomic>
#include <cstdint>

namespace threadforge {

struct StatisticsSnapshot {
  std::uint64_t connected_clients;
  std::uint64_t submitted_jobs;
  std::uint64_t completed_jobs;
  std::uint64_t failed_jobs;
  std::uint64_t queue_length;
  std::uint64_t active_workers;
};

class Statistics {
 public:
  void client_connected() noexcept { connected_clients_.fetch_add(1, std::memory_order_relaxed); }
  void client_disconnected() noexcept { connected_clients_.fetch_sub(1, std::memory_order_relaxed); }
  void job_submitted() noexcept { submitted_jobs_.fetch_add(1, std::memory_order_relaxed); }
  void job_completed() noexcept { completed_jobs_.fetch_add(1, std::memory_order_relaxed); }
  void job_failed() noexcept { failed_jobs_.fetch_add(1, std::memory_order_relaxed); }
  void queue_inc() noexcept { queue_length_.fetch_add(1, std::memory_order_relaxed); }
  void queue_dec() noexcept { queue_length_.fetch_sub(1, std::memory_order_relaxed); }
  void worker_active() noexcept { active_workers_.fetch_add(1, std::memory_order_relaxed); }
  void worker_idle() noexcept { active_workers_.fetch_sub(1, std::memory_order_relaxed); }
  StatisticsSnapshot snapshot() const noexcept;

 private:
  // These are independent counters, so atomics avoid one hot statistics mutex.
  std::atomic<std::uint64_t> connected_clients_{0};
  std::atomic<std::uint64_t> submitted_jobs_{0};
  std::atomic<std::uint64_t> completed_jobs_{0};
  std::atomic<std::uint64_t> failed_jobs_{0};
  std::atomic<std::uint64_t> queue_length_{0};
  std::atomic<std::uint64_t> active_workers_{0};
};

}  // namespace threadforge
