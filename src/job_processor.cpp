#include "job.hpp"

#include <algorithm>
#include <charconv>
#include <functional>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace threadforge {
namespace {
long long parse_ll(const std::string& s) {
  long long v{};
  auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
  if (ec != std::errc{} || ptr != s.data() + s.size()) throw std::invalid_argument("bad integer");
  return v;
}

bool is_prime(long long n) {
  if (n < 2) return false;
  if (n % 2 == 0) return n == 2;
  for (long long d = 3; d <= n / d; d += 2) if (n % d == 0) return false;
  return true;
}
}  // namespace

Job parse_job_line(const std::string& line, int client_fd, std::uint64_t id) {
  std::istringstream in(line);
  std::string cmd;
  in >> cmd;
  std::string rest;
  std::getline(in, rest);
  if (!rest.empty() && rest.front() == ' ') rest.erase(rest.begin());
  Job job{};
  job.client_fd = client_fd;
  job.id = id;
  if (cmd == "PRIME") job.type = JobType::Prime;
  else if (cmd == "SORT") job.type = JobType::Sort;
  else if (cmd == "MATMUL") job.type = JobType::Matmul;
  else if (cmd == "HASH") job.type = JobType::Hash;
  else throw std::invalid_argument("unknown command");
  if (rest.empty()) throw std::invalid_argument("missing argument");
  job.argument = rest;
  return job;
}

JobResult execute_job(const Job& job) {
  try {
    if (job.type == JobType::Prime) return {true, is_prime(parse_ll(job.argument)) ? "prime" : "composite"};
    if (job.type == JobType::Hash) return {true, std::to_string(std::hash<std::string>{}(job.argument))};
    const int n = static_cast<int>(parse_ll(job.argument));
    if (n <= 0 || n > 2000000) throw std::invalid_argument("size out of range");
    if (job.type == JobType::Sort) {
      std::vector<int> v(n);
      std::mt19937 rng(42);
      std::generate(v.begin(), v.end(), rng);
      std::sort(v.begin(), v.end());
      return {true, "sorted " + std::to_string(v.size()) + " checksum " + std::to_string(v.front() ^ v.back())};
    }
    if (job.type == JobType::Matmul) {
      if (n > 512) throw std::invalid_argument("matrix too large");
      std::vector<double> a(n * n, 1.5), b(n * n, 2.0), c(n * n);
      for (int i = 0; i < n; ++i) for (int k = 0; k < n; ++k) for (int j = 0; j < n; ++j) c[i*n+j] += a[i*n+k] * b[k*n+j];
      return {true, "matmul " + std::to_string(n) + " checksum " + std::to_string(c[(n*n)/2])};
    }
  } catch (const std::exception& e) { return {false, e.what()}; }
  return {false, "invalid job"};
}

}  // namespace threadforge
