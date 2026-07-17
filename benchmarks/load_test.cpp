#include <arpa/inet.h>
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;

struct Options {
  std::string host = "127.0.0.1";
  int port = 9000;
  int clients = 25;
  int requests_per_client = 100;
  std::string workload = "prime";
  std::uint64_t prime_value = 32'416'190'071ULL;
  int sort_count = 10'000;
  int matrix_size = 32;
};

struct Results {
  std::vector<double> latencies_ms;
  std::vector<std::uint64_t> job_ids;
  std::uint64_t successes = 0;
  std::uint64_t failures = 0;
};

template <class T>
T number(std::string_view text, const char* option) {
  T value{};
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
  if (error != std::errc{} || end != text.data() + text.size())
    throw std::invalid_argument(std::string("invalid value for ") + option);
  return value;
}

Options parse_options(int argc, char** argv) {
  Options options;
  for (int i = 1; i < argc; ++i) {
    const std::string name = argv[i];
    if (i + 1 >= argc) throw std::invalid_argument("missing value for " + name);
    const std::string value = argv[++i];
    if (name == "--host") options.host = value;
    else if (name == "--port") options.port = number<int>(value, "--port");
    else if (name == "--clients") options.clients = number<int>(value, "--clients");
    else if (name == "--requests-per-client")
      options.requests_per_client = number<int>(value, "--requests-per-client");
    else if (name == "--workload") options.workload = value;
    else if (name == "--prime-value")
      options.prime_value = number<std::uint64_t>(value, "--prime-value");
    else if (name == "--sort-count") options.sort_count = number<int>(value, "--sort-count");
    else if (name == "--matrix-size")
      options.matrix_size = number<int>(value, "--matrix-size");
    else throw std::invalid_argument("unknown option: " + name);
  }
  if (options.port < 1 || options.port > 65'535 || options.clients < 1 ||
      options.requests_per_client < 1)
    throw std::invalid_argument("port, clients, and requests must be positive and in range");
  if (options.workload != "prime" && options.workload != "sort" &&
      options.workload != "matmul" && options.workload != "mixed")
    throw std::invalid_argument("workload must be prime, sort, matmul, or mixed");
  return options;
}

std::string request_for(const Options& options, int client, int request) {
  std::string workload = options.workload;
  if (workload == "mixed") {
    // Offset each client's deterministic 10-request cycle to avoid synchronized bursts.
    const int slot = (client * 7 + request) % 10;
    workload = slot < 5 ? "prime" : (slot < 8 ? "sort" : "matmul");
  }
  if (workload == "prime") return "PRIME " + std::to_string(options.prime_value) + "\n";
  if (workload == "sort") return "SORT " + std::to_string(options.sort_count) + "\n";
  return "MATMUL " + std::to_string(options.matrix_size) + "\n";
}

bool send_all(int fd, const std::string& request) {
  std::size_t sent = 0;
  while (sent < request.size()) {
#ifdef MSG_NOSIGNAL
    constexpr int flags = MSG_NOSIGNAL;
#else
    constexpr int flags = 0;
#endif
    const ssize_t count = ::send(fd, request.data() + sent, request.size() - sent, flags);
    if (count <= 0) return false;
    sent += static_cast<std::size_t>(count);
  }
  return true;
}

bool receive_line(int fd, std::string& buffered, std::string& line) {
  while (true) {
    const auto newline = buffered.find('\n');
    if (newline != std::string::npos) {
      line = buffered.substr(0, newline);
      buffered.erase(0, newline + 1);
      return true;
    }
    char bytes[512];
    const ssize_t count = ::recv(fd, bytes, sizeof(bytes), 0);
    if (count <= 0) return false;
    buffered.append(bytes, static_cast<std::size_t>(count));
  }
}

void run_client(const Options& options, int client_index, Results& results) {
  const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    results.failures = static_cast<std::uint64_t>(options.requests_per_client);
    return;
  }
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(static_cast<std::uint16_t>(options.port));
  if (::inet_pton(AF_INET, options.host.c_str(), &address.sin_addr) != 1 ||
      ::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
    results.failures = static_cast<std::uint64_t>(options.requests_per_client);
    ::close(fd);
    return;
  }

  std::string buffered;
  for (int request_index = 0; request_index < options.requests_per_client; ++request_index) {
    const std::string request = request_for(options, client_index, request_index);
    const auto start = Clock::now();
    std::string response;
    if (!send_all(fd, request) || !receive_line(fd, buffered, response)) {
      results.failures += static_cast<std::uint64_t>(options.requests_per_client - request_index);
      break;
    }
    results.latencies_ms.push_back(
        std::chrono::duration<double, std::milli>(Clock::now() - start).count());
    const auto first_space = response.find(' ');
    const auto second_space = response.find(' ', first_space + 1);
    std::uint64_t id{};
    if (first_space == std::string::npos || second_space == std::string::npos ||
        !response.starts_with("OK ") ||
        std::from_chars(response.data() + first_space + 1, response.data() + second_space, id).ec !=
            std::errc{}) {
      ++results.failures;
    } else {
      ++results.successes;
      results.job_ids.push_back(id);
    }
  }
  ::close(fd);
}

double percentile(const std::vector<double>& sorted, double fraction) {
  if (sorted.empty()) return 0.0;
  const auto index = static_cast<std::size_t>(
      std::ceil(fraction * static_cast<double>(sorted.size())) - 1.0);
  return sorted.at(std::min(index, sorted.size() - 1));
}
}  // namespace

int main(int argc, char** argv) {
  try {
    const Options options = parse_options(argc, argv);
    std::vector<Results> client_results(static_cast<std::size_t>(options.clients));
    std::vector<std::thread> clients;
    clients.reserve(static_cast<std::size_t>(options.clients));
    const auto start = Clock::now();
    for (int i = 0; i < options.clients; ++i)
      clients.emplace_back([&, i] { run_client(options, i, client_results.at(static_cast<std::size_t>(i))); });
    for (auto& client : clients) client.join();
    const double duration = std::chrono::duration<double>(Clock::now() - start).count();

    Results total;
    for (auto& result : client_results) {
      total.successes += result.successes;
      total.failures += result.failures;
      total.latencies_ms.insert(total.latencies_ms.end(), result.latencies_ms.begin(),
                                result.latencies_ms.end());
      total.job_ids.insert(total.job_ids.end(), result.job_ids.begin(), result.job_ids.end());
    }
    std::sort(total.latencies_ms.begin(), total.latencies_ms.end());
    const std::set<std::uint64_t> unique_ids(total.job_ids.begin(), total.job_ids.end());
    if (unique_ids.size() != total.job_ids.size()) {
      std::cerr << "duplicate response job IDs detected\n";
      return 2;
    }
    double mean = 0.0;
    for (const double latency : total.latencies_ms) mean += latency;
    if (!total.latencies_ms.empty()) mean /= static_cast<double>(total.latencies_ms.size());
    const std::uint64_t requests = static_cast<std::uint64_t>(options.clients) *
                                   static_cast<std::uint64_t>(options.requests_per_client);
    const double throughput = duration > 0.0 ? static_cast<double>(total.successes) / duration : 0.0;

    std::cout << "total_requests=" << requests << " successes=" << total.successes
              << " failures=" << total.failures << " duration_seconds=" << duration
              << " throughput_requests_per_second=" << throughput << '\n';
    std::cout << "mean_latency_ms=" << mean
              << " median_latency_ms=" << percentile(total.latencies_ms, 0.50)
              << " p95_latency_ms=" << percentile(total.latencies_ms, 0.95)
              << " p99_latency_ms=" << percentile(total.latencies_ms, 0.99)
              << " max_latency_ms="
              << (total.latencies_ms.empty() ? 0.0 : total.latencies_ms.back()) << '\n';
    std::cout << "csv=" << requests << ',' << total.successes << ',' << total.failures << ','
              << duration << ',' << throughput << ',' << mean << ','
              << percentile(total.latencies_ms, 0.50) << ','
              << percentile(total.latencies_ms, 0.95) << ','
              << percentile(total.latencies_ms, 0.99) << ','
              << (total.latencies_ms.empty() ? 0.0 : total.latencies_ms.back()) << '\n';
    return total.failures == 0 ? 0 : 2;
  } catch (const std::exception& error) {
    std::cerr << "load_test: " << error.what() << '\n';
    return 1;
  }
}
