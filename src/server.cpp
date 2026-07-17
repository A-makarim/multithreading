#include "server.hpp"

#include <arpa/inet.h>
#include <csignal>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace threadforge {
namespace {
void send_all(int fd, const std::string& s) {
  const char* p = s.data();
  std::size_t left = s.size();
  while (left > 0) {
    ssize_t n = ::send(fd, p, left, MSG_NOSIGNAL);
    if (n <= 0) return;
    p += n;
    left -= static_cast<std::size_t>(n);
  }
}
}  // namespace

TcpJobServer::TcpJobServer(int port, std::size_t worker_count, std::size_t queue_capacity)
    : port_(port), queue_(queue_capacity) {
  for (std::size_t i = 0; i < worker_count; ++i) workers_.emplace_back([this] { worker_loop(); });
}

TcpJobServer::~TcpJobServer() { stop(); }

void TcpJobServer::run() {
  listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd_ < 0) throw std::runtime_error(std::strerror(errno));
  int yes = 1;
  ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(static_cast<uint16_t>(port_));
  if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) throw std::runtime_error(std::strerror(errno));
  if (::listen(listen_fd_, SOMAXCONN) < 0) throw std::runtime_error(std::strerror(errno));
  std::cout << "ThreadForge listening on port " << port_ << '\n';
  accept_loop();
}

void TcpJobServer::stop() {
  bool expected = false;
  if (!stopping_.compare_exchange_strong(expected, true)) return;
  if (listen_fd_ >= 0) { ::shutdown(listen_fd_, SHUT_RDWR); ::close(listen_fd_); listen_fd_ = -1; }
  close_all_clients();
  queue_.close();
  for (auto& t : clients_) if (t.joinable()) t.join();
  for (auto& w : workers_) if (w.joinable()) w.join();
}

void TcpJobServer::accept_loop() {
  while (!stopping_.load()) {
    int fd = ::accept(listen_fd_, nullptr, nullptr);
    if (fd < 0) { if (stopping_.load()) break; continue; }
    stats_.client_connected();
    std::lock_guard<std::mutex> lock(clients_mutex_);
    client_fds_.push_back(fd);
    clients_.emplace_back([this, fd] { client_loop(fd); });
  }
}

void TcpJobServer::client_loop(int client_fd) {
  std::string buffer;
  char tmp[4096];
  while (!stopping_.load()) {
    ssize_t n = ::recv(client_fd, tmp, sizeof(tmp), 0);
    if (n <= 0) break;
    buffer.append(tmp, tmp + n);
    for (std::size_t pos; (pos = buffer.find('\n')) != std::string::npos;) {
      std::string line = buffer.substr(0, pos);
      if (!line.empty() && line.back() == '\r') line.pop_back();
      buffer.erase(0, pos + 1);
      try {
        Job job = parse_job_line(line, client_fd, next_job_id_.fetch_add(1));
        stats_.job_submitted();
        stats_.queue_inc();
        if (!queue_.push(std::move(job))) send_all(client_fd, "ERROR server shutting down\n");
      } catch (const std::exception& e) {
        stats_.job_failed();
        send_all(client_fd, std::string("ERROR ") + e.what() + "\n");
      }
    }
  }
  ::close(client_fd);
  stats_.client_disconnected();
}

void TcpJobServer::worker_loop() {
  while (auto job = queue_.pop()) {
    stats_.queue_dec();
    stats_.worker_active();
    JobResult result = execute_job(*job);
    result.ok ? stats_.job_completed() : stats_.job_failed();
    send_all(job->client_fd, std::string(result.ok ? "OK " : "ERROR ") + result.message + "\n");
    stats_.worker_idle();
  }
}

void TcpJobServer::close_all_clients() {
  std::lock_guard<std::mutex> lock(clients_mutex_);
  // Closing sockets unblocks client recv() threads during Ctrl+C shutdown.
  for (int fd : client_fds_) ::shutdown(fd, SHUT_RDWR);
}

}  // namespace threadforge
