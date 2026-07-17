#include "server.hpp"

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <thread>

volatile std::sig_atomic_t g_stop = 0;
void on_signal(int) { g_stop = 1; }

int main(int argc, char** argv) {
  const int port = argc > 1 ? std::atoi(argv[1]) : 9000;
  const std::size_t workers = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : std::thread::hardware_concurrency();
  const std::size_t capacity = argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 1024;
  std::signal(SIGINT, on_signal);
  threadforge::TcpJobServer server(port, workers == 0 ? 4 : workers, capacity);
  std::exception_ptr run_error;
  std::atomic<bool> run_finished{false};
  std::thread runner([&] {
    try {
      server.run();
    } catch (...) {
      run_error = std::current_exception();
    }
    run_finished.store(true, std::memory_order_release);
  });
  while (!g_stop && !run_finished.load(std::memory_order_acquire))
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  server.stop();
  runner.join();
  if (run_error) {
    try {
      std::rethrow_exception(run_error);
    } catch (const std::exception& error) {
      std::cerr << "server failed: " << error.what() << '\n';
      return 1;
    }
  }
  auto s = server.statistics().snapshot();
  std::cout << "submitted=" << s.submitted_jobs << " completed=" << s.completed_jobs << " failed=" << s.failed_jobs << '\n';
}
