#include "job.hpp"
#include "server.hpp"

#include <arpa/inet.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace {
bool throws_with(const std::string& line, const std::string& message) {
  try {
    (void)threadforge::parse_job_line(line, nullptr, 1);
  } catch (const std::exception& error) {
    return error.what() == message;
  }
  return false;
}

int reserve_port() {
  const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  assert(fd >= 0);
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = 0;
  assert(::bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
  socklen_t size = sizeof(address);
  assert(::getsockname(fd, reinterpret_cast<sockaddr*>(&address), &size) == 0);
  const int port = ntohs(address.sin_port);
  ::close(fd);
  return port;
}

int connect_to(int port) {
  for (int attempt = 0; attempt < 100; ++attempt) {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(static_cast<std::uint16_t>(port));
    if (::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0) return fd;
    ::close(fd);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return -1;
}
}  // namespace

int main() {
  auto prime = threadforge::parse_job_line("PRIME 7", nullptr, 9);
  assert(prime.id == 9 && threadforge::execute_job(prime).message == "prime");
  assert(throws_with("PRIME nope", "invalid integer"));
  assert(throws_with("PRIME 0", "argument out of range"));
  assert(throws_with("SORT 2000001", "argument out of range"));
  assert(throws_with("MATMUL 513", "argument out of range"));
  assert(throws_with("NOPE 1", "unsupported command"));
  assert(throws_with(std::string(threadforge::kMaxRequestLine + 1, 'x'), "request too large"));

  const int port = reserve_port();
  threadforge::TcpJobServer server(port, 4, 32);
  std::thread server_thread([&] { server.run(); });
  std::array<std::thread, 4> clients;
  for (auto& client : clients) {
    client = std::thread([&] {
      const int fd = connect_to(port);
      assert(fd >= 0);
      const std::string requests = "PRIME 10000019\nHASH hello\nSORT 1000\nMATMUL 8\n";
      assert(::send(fd, requests.data(), requests.size(), 0) ==
             static_cast<ssize_t>(requests.size()));
      std::string responses;
      char buffer[256];
      while (std::count(responses.begin(), responses.end(), '\n') < 4) {
        const ssize_t count = ::recv(fd, buffer, sizeof(buffer), 0);
        assert(count > 0);
        responses.append(buffer, static_cast<std::size_t>(count));
      }
      std::size_t start = 0;
      for (int i = 0; i < 4; ++i) {
        const auto end = responses.find('\n', start);
        assert(end != std::string::npos);
        const std::string response = responses.substr(start, end - start);
        assert(response.starts_with("OK "));
        assert(response.find(' ', 3) != std::string::npos);
        start = end + 1;
      }
      ::close(fd);
    });
  }
  for (auto& client : clients) client.join();
  server.stop();
  server.stop();
  server_thread.join();
  const auto stats = server.statistics().snapshot();
  assert(stats.submitted_jobs == 16);
  assert(stats.completed_jobs == 16);
  assert(stats.queue_length == 0);
  assert(stats.active_workers == 0);

  const int load_port = reserve_port();
  threadforge::TcpJobServer loaded_server(load_port, 2, 4);
  std::thread loaded_server_thread([&] { loaded_server.run(); });
  const int loaded_fd = connect_to(load_port);
  assert(loaded_fd >= 0);
  std::string expensive_requests;
  for (int i = 0; i < 100; ++i) expensive_requests += "MATMUL 64\n";
  assert(::send(loaded_fd, expensive_requests.data(), expensive_requests.size(), 0) ==
         static_cast<ssize_t>(expensive_requests.size()));
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  const auto stop_started = std::chrono::steady_clock::now();
  loaded_server.stop();
  loaded_server_thread.join();
  assert(std::chrono::steady_clock::now() - stop_started < std::chrono::seconds(5));
  ::close(loaded_fd);
}
