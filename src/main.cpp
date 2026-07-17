#include "server.hpp"

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <thread>

std::atomic<bool> g_stop{false};
void on_signal(int) { g_stop.store(true); }

int main(int argc, char** argv) {
  const int port = argc > 1 ? std::atoi(argv[1]) : 9000;
  const std::size_t workers = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : std::thread::hardware_concurrency();
  const std::size_t capacity = argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 1024;
  std::signal(SIGINT, on_signal);
  threadforge::TcpJobServer server(port, workers == 0 ? 4 : workers, capacity);
  std::thread runner([&] { server.run(); });
  while (!g_stop.load()) std::this_thread::sleep_for(std::chrono::milliseconds(100));
  server.stop();
  runner.join();
  auto s = server.statistics().snapshot();
  std::cout << "submitted=" << s.submitted_jobs << " completed=" << s.completed_jobs << " failed=" << s.failed_jobs << '\n';
}
