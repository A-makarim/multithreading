#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "client_connection.hpp"

namespace threadforge {

enum class JobType { Prime, Sort, Matmul, Hash };

struct Job {
  JobType type{};
  std::string argument;
  std::shared_ptr<ClientConnection> connection;
  std::uint64_t id = 0;
};

struct JobResult {
  bool ok = false;
  std::string message;
};

inline constexpr std::size_t kMaxRequestLine = 4096;

Job parse_job_line(const std::string& line, std::shared_ptr<ClientConnection> connection,
                   std::uint64_t id);
JobResult execute_job(const Job& job);

}  // namespace threadforge
