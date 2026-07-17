#include <arpa/inet.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char** argv) {
  const char* host = argc > 1 ? argv[1] : "127.0.0.1";
  int port = argc > 2 ? std::atoi(argv[2]) : 9000;
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons(port);
  if (::inet_pton(AF_INET, host, &addr.sin_addr) != 1 || ::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    std::cerr << "connect failed: " << std::strerror(errno) << "\n"; return 1;
  }
  std::string line;
  char buf[4096];
  while (std::getline(std::cin, line)) {
    line.push_back('\n'); ::send(fd, line.data(), line.size(), MSG_NOSIGNAL);
    ssize_t n = ::recv(fd, buf, sizeof(buf)-1, 0); if (n <= 0) break;
    buf[n] = 0; std::cout << buf;
  }
  ::close(fd);
}
