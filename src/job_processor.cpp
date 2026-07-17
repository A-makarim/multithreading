#include "job.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <functional>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace threadforge {
namespace {
std::uint64_t parse_uint64(const std::string& s) {
  std::uint64_t v{};
  auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
  if (ec == std::errc::result_out_of_range) throw std::out_of_range("argument out of range");
  if (ec != std::errc{} || ptr != s.data() + s.size())
    throw std::invalid_argument("invalid integer");
  return v;
}

bool is_prime(std::uint64_t n) {
  if (n < 2) return false;
  if (n % 2 == 0) return n == 2;
  for (std::uint64_t d = 3; d <= n / d; d += 2)
    if (n % d == 0) return false;
  return true;
}
}  // namespace

Job parse_job_line(const std::string& line, std::shared_ptr<ClientConnection> connection,
                   std::uint64_t id) {
  if (line.size() > kMaxRequestLine) throw std::length_error("request too large");
  std::istringstream in(line);
  std::string cmd;
  in >> cmd;
  std::string rest;
  std::getline(in, rest);
  if (!rest.empty() && rest.front() == ' ') rest.erase(rest.begin());
  Job job{};
  job.connection = std::move(connection);
  job.id = id;
  if (cmd == "PRIME") job.type = JobType::Prime;
  else if (cmd == "SORT") job.type = JobType::Sort;
  else if (cmd == "MATMUL") job.type = JobType::Matmul;
  else if (cmd == "HASH") job.type = JobType::Hash;
  else throw std::invalid_argument("unsupported command");
  if (rest.empty()) throw std::invalid_argument("missing argument");

  if (job.type == JobType::Prime) {
    if (parse_uint64(rest) == 0) throw std::out_of_range("argument out of range");
  } else if (job.type == JobType::Sort) {
    const auto count = parse_uint64(rest);
    if (count < 1 || count > 2'000'000) throw std::out_of_range("argument out of range");
  } else if (job.type == JobType::Matmul) {
    const auto size = parse_uint64(rest);
    if (size < 1 || size > 512) throw std::out_of_range("argument out of range");
  }
  job.argument = rest;
  return job;
}

JobResult execute_job(const Job& job) {
    if (job.type == JobType::Prime) return {true, is_prime(parse_uint64(job.argument)) ? "prime" : "composite"};
    if (job.type == JobType::Hash) return {true, std::to_string(std::hash<std::string>{}(job.argument))};
    const int n = static_cast<int>(parse_uint64(job.argument));
    if (job.type == JobType::Sort) {
      std::vector<int> v(n);
      std::mt19937 rng(42);
      std::generate(v.begin(), v.end(), rng);
      std::sort(v.begin(), v.end());
      return {true, "sorted " + std::to_string(v.size()) + " checksum " + std::to_string(v.front() ^ v.back())};
    }
    if (job.type == JobType::Matmul) {
      const auto elements = static_cast<std::size_t>(n) * static_cast<std::size_t>(n);
      std::vector<double> a(elements, 1.5), b(elements, 2.0), c(elements);
      for (int i = 0; i < n; ++i) for (int k = 0; k < n; ++k) for (int j = 0; j < n; ++j) c[i*n+j] += a[i*n+k] * b[k*n+j];
      return {true, "matmul " + std::to_string(n) + " checksum " + std::to_string(c[elements / 2])};
    }
  return {false, "invalid job"};
}

}  // namespace threadforge
