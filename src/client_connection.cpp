#include "client_connection.hpp"

#include <cerrno>
#include <sys/socket.h>
#include <unistd.h>

namespace threadforge {

ClientConnection::ClientConnection(int socket_fd) : fd_(socket_fd) {}

ClientConnection::~ClientConnection() { shutdown_connection(); }

bool ClientConnection::send_line(const std::string& response) {
  std::lock_guard<std::mutex> lock(send_mutex_);
  if (!open_.load(std::memory_order_relaxed)) return false;

  std::string line = response;
  line.push_back('\n');
  std::size_t sent = 0;
  while (sent < line.size()) {
#ifdef MSG_NOSIGNAL
    constexpr int flags = MSG_NOSIGNAL;
#else
    constexpr int flags = 0;
#endif
    const ssize_t count = ::send(fd_, line.data() + sent, line.size() - sent, flags);
    if (count > 0) {
      sent += static_cast<std::size_t>(count);
      continue;
    }
    if (count < 0 && errno == EINTR) continue;
    close_locked();
    return false;
  }
  return true;
}

void ClientConnection::shutdown_connection() noexcept {
  std::lock_guard<std::mutex> lock(send_mutex_);
  close_locked();
}

void ClientConnection::close_locked() noexcept {
  if (!open_.exchange(false, std::memory_order_acq_rel)) return;
  ::shutdown(fd_, SHUT_RDWR);
  ::close(fd_);
}

}  // namespace threadforge
