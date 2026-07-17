#include "client_connection.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

int main() {
  std::array<int, 2> sockets{};
  assert(::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets.data()) == 0);
  threadforge::ClientConnection connection(sockets[0]);
  constexpr int senders = 8;
  constexpr int lines_per_sender = 100;
  std::vector<std::thread> threads;
  for (int sender = 0; sender < senders; ++sender) {
    threads.emplace_back([&, sender] {
      for (int line = 0; line < lines_per_sender; ++line)
        assert(connection.send_line("sender=" + std::to_string(sender) +
                                    " line=" + std::to_string(line)));
    });
  }

  std::string received;
  std::thread reader([&] {
    char buffer[127];
    while (std::count(received.begin(), received.end(), '\n') < senders * lines_per_sender) {
      const ssize_t count = ::recv(sockets[1], buffer, sizeof(buffer), 0);
      assert(count > 0);
      received.append(buffer, static_cast<std::size_t>(count));
    }
  });
  for (auto& thread : threads) thread.join();
  reader.join();

  std::size_t start = 0;
  int lines = 0;
  for (;;) {
    const auto end = received.find('\n', start);
    if (end == std::string::npos) break;
    const std::string line = received.substr(start, end - start);
    assert(line.starts_with("sender=") && line.find(" line=") != std::string::npos);
    start = end + 1;
    ++lines;
  }
  assert(lines == senders * lines_per_sender);
  connection.shutdown_connection();
  connection.shutdown_connection();
  assert(!connection.is_open());
  assert(!connection.send_line("closed"));
  ::close(sockets[1]);
}
