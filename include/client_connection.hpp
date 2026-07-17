#pragma once

#include <atomic>
#include <mutex>
#include <string>

namespace threadforge {

class ClientConnection {
 public:
  explicit ClientConnection(int socket_fd);
  ~ClientConnection();

  ClientConnection(const ClientConnection&) = delete;
  ClientConnection& operator=(const ClientConnection&) = delete;

  int fd() const noexcept { return fd_; }
  bool send_line(const std::string& response);
  void shutdown_connection() noexcept;
  bool is_open() const noexcept { return open_.load(std::memory_order_acquire); }

 private:
  void close_locked() noexcept;

  const int fd_;
  std::mutex send_mutex_;
  std::atomic<bool> open_{true};
};

}  // namespace threadforge
