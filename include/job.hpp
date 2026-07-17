#pragma once

#include <cstdint>
#include <string>

namespace threadforge {

enum class JobType { Prime, Sort, Matmul, Hash };

struct Job {
  JobType type{};
  std::string argument;
  int client_fd = -1;
  std::uint64_t id = 0;
};

struct JobResult {
  bool ok = false;
  std::string message;
};

Job parse_job_line(const std::string& line, int client_fd, std::uint64_t id);
JobResult execute_job(const Job& job);

}  // namespace threadforge
