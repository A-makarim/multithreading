#include "statistics.hpp"
namespace threadforge {
StatisticsSnapshot Statistics::snapshot() const noexcept {
  return {connected_clients_.load(std::memory_order_relaxed), submitted_jobs_.load(std::memory_order_relaxed),
          completed_jobs_.load(std::memory_order_relaxed), failed_jobs_.load(std::memory_order_relaxed),
          queue_length_.load(std::memory_order_relaxed), active_workers_.load(std::memory_order_relaxed)};
}
}  // namespace threadforge
