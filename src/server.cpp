#include "server.hpp"

#include <arpa/inet.h>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace threadforge {
namespace {
class ActiveWorkerGuard {
 public:
  explicit ActiveWorkerGuard(Statistics& stats) : stats_(stats) { stats_.worker_active(); }
  ~ActiveWorkerGuard() { stats_.worker_idle(); }

 private:
  Statistics& stats_;
};

std::string response(bool ok, std::uint64_t id, const std::string& message) {
  return std::string(ok ? "OK " : "ERROR ") + std::to_string(id) + " " + message;
}

void close_fd(int fd) noexcept {
  if (fd >= 0) {
    ::shutdown(fd, SHUT_RDWR);
    ::close(fd);
  }
}
}  // namespace

TcpJobServer::TcpJobServer(int port, std::size_t worker_count, std::size_t queue_capacity)
    : port_(port), queue_(queue_capacity) {
  for (std::size_t i = 0; i < worker_count; ++i) workers_.emplace_back([this] { worker_loop(); });
}

TcpJobServer::~TcpJobServer() { stop(); }

void TcpJobServer::run() {
  if (stopping_.load(std::memory_order_acquire)) return;
  const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) throw std::runtime_error(std::strerror(errno));
  listen_fd_.store(fd, std::memory_order_release);
  if (stopping_.load(std::memory_order_acquire)) {
    close_fd(listen_fd_.exchange(-1));
    return;
  }
  int yes = 1;
  if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
    close_fd(listen_fd_.exchange(-1));
    throw std::runtime_error(std::strerror(errno));
  }
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(static_cast<uint16_t>(port_));
  if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0 ||
      ::listen(fd, SOMAXCONN) < 0) {
    const std::string error = std::strerror(errno);
    close_fd(listen_fd_.exchange(-1));
    throw std::runtime_error(error);
  }
  std::cout << "ThreadForge listening on port " << port_ << '\n';
  accept_loop();
}

void TcpJobServer::stop() {
  bool expected = false;
  if (!stopping_.compare_exchange_strong(expected, true)) return;
  close_fd(listen_fd_.exchange(-1, std::memory_order_acq_rel));
  close_all_clients();
  queue_.close();
  std::vector<std::thread> clients;
  {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    clients.swap(clients_);
  }
  for (auto& thread : clients)
    if (thread.joinable() && thread.get_id() != std::this_thread::get_id()) thread.join();
  for (auto& worker : workers_)
    if (worker.joinable() && worker.get_id() != std::this_thread::get_id()) worker.join();
  {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    connections_.clear();
  }
}

void TcpJobServer::accept_loop() {
  while (!stopping_.load()) {
    const int listen_fd = listen_fd_.load(std::memory_order_acquire);
    if (listen_fd < 0) break;
    const int fd = ::accept(listen_fd, nullptr, nullptr);
    if (fd < 0) {
      if (stopping_.load(std::memory_order_acquire)) break;
      if (errno != EINTR) std::cerr << "accept failed: " << std::strerror(errno) << '\n';
      continue;
    }
    auto connection = std::make_shared<ClientConnection>(fd);
    {
      std::lock_guard<std::mutex> lock(clients_mutex_);
      if (stopping_.load(std::memory_order_acquire)) {
        connection->shutdown_connection();
        break;
      }
      connections_.push_back(connection);
      stats_.client_connected();
      clients_.emplace_back([this, connection] { client_loop(connection); });
    }
  }
}

void TcpJobServer::client_loop(std::shared_ptr<ClientConnection> connection) {
  std::string buffer;
  char tmp[4096];
  while (!stopping_.load()) {
    const ssize_t n = ::recv(connection->fd(), tmp, sizeof(tmp), 0);
    if (n <= 0) break;
    buffer.append(tmp, tmp + n);
    for (std::size_t pos; (pos = buffer.find('\n')) != std::string::npos;) {
      const std::uint64_t id = next_job_id_.fetch_add(1, std::memory_order_relaxed);
      if (pos > kMaxRequestLine) {
        stats_.job_failed();
        connection->send_line(response(false, id, "request too large"));
        buffer.erase(0, pos + 1);
        continue;
      }
      std::string line = buffer.substr(0, pos);
      if (!line.empty() && line.back() == '\r') line.pop_back();
      buffer.erase(0, pos + 1);
      try {
        Job job = parse_job_line(line, connection, id);
        stats_.job_submitted();
        if (!queue_.push(std::move(job), [this] { stats_.queue_inc(); })) {
          stats_.job_failed();
          connection->send_line(response(false, id, "server shutting down"));
        }
      } catch (const std::exception& e) {
        stats_.job_failed();
        connection->send_line(response(false, id, e.what()));
      }
    }
    if (buffer.size() > kMaxRequestLine) {
      const std::uint64_t id = next_job_id_.fetch_add(1, std::memory_order_relaxed);
      stats_.job_failed();
      connection->send_line(response(false, id, "request too large"));
      break;
    }
  }
  connection->shutdown_connection();
  {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    std::erase(connections_, connection);
  }
  stats_.client_disconnected();
}

void TcpJobServer::worker_loop() {
  while (auto job = queue_.pop()) {
    stats_.queue_dec();
    ActiveWorkerGuard active(stats_);
    try {
      const JobResult result = execute_job(*job);
      result.ok ? stats_.job_completed() : stats_.job_failed();
      job->connection->send_line(response(result.ok, job->id, result.message));
    } catch (const std::exception& e) {
      stats_.job_failed();
      std::cerr << "job " << job->id << " failed: " << e.what() << '\n';
      try {
        job->connection->send_line(response(false, job->id, e.what()));
      } catch (const std::exception& send_error) {
        std::cerr << "could not build/send error response for job " << job->id << ": "
                  << send_error.what() << '\n';
      }
    }
  }
}

void TcpJobServer::close_all_clients() {
  std::vector<std::shared_ptr<ClientConnection>> connections;
  {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    connections = connections_;
  }
  for (const auto& connection : connections) connection->shutdown_connection();
}

}  // namespace threadforge
