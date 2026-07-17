#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "bounded_queue.hpp"
#include "job.hpp"
#include "statistics.hpp"

namespace threadforge {

class TcpJobServer {
 public:
  TcpJobServer(int port, std::size_t worker_count, std::size_t queue_capacity);
  ~TcpJobServer();
  TcpJobServer(const TcpJobServer&) = delete;
  TcpJobServer& operator=(const TcpJobServer&) = delete;

  void run();
  void stop();
  const Statistics& statistics() const noexcept { return stats_; }

 private:
  void accept_loop();
  void client_loop(std::shared_ptr<ClientConnection> connection);
  void worker_loop();
  void close_all_clients();

  int port_;
  std::atomic<int> listen_fd_{-1};
  BoundedQueue<Job> queue_;
  Statistics stats_;
  std::atomic<bool> stopping_{false};
  std::atomic<std::uint64_t> next_job_id_{1};
  std::vector<std::thread> workers_;
  std::vector<std::thread> clients_;
  std::vector<std::shared_ptr<ClientConnection>> connections_;
  std::mutex clients_mutex_;
};

}  // namespace threadforge
