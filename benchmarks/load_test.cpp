#include <arpa/inet.h>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

static double one_client(const char* host, int port, int requests) {
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons(port); ::inet_pton(AF_INET, host, &addr.sin_addr);
  if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) return -1;
  char buf[4096]; double max_us = 0;
  for (int i = 0; i < requests; ++i) {
    std::string req = (i % 2 == 0) ? "PRIME 10000019\n" : "HASH hello world\n";
    auto a = std::chrono::steady_clock::now(); ::send(fd, req.data(), req.size(), MSG_NOSIGNAL); ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
    auto b = std::chrono::steady_clock::now(); if (n <= 0) break;
    max_us = std::max(max_us, std::chrono::duration<double, std::micro>(b-a).count());
  }
  ::close(fd); return max_us;
}

int main(int argc, char** argv) {
  const char* host = argc > 1 ? argv[1] : "127.0.0.1"; int port = argc > 2 ? std::atoi(argv[2]) : 9000; int per = argc > 3 ? std::atoi(argv[3]) : 100;
  for (int clients : {1,5,10,25,50,100}) {
    std::vector<std::thread> threads; std::vector<double> maxes(clients);
    auto start = std::chrono::steady_clock::now();
    for (int i=0;i<clients;++i) threads.emplace_back([&,i]{ maxes[i]=one_client(host,port,per); });
    for (auto& t : threads) t.join();
    auto end = std::chrono::steady_clock::now();
    double sec = std::chrono::duration<double>(end-start).count();
    std::cout << clients << " clients: " << (clients*per/sec) << " jobs/sec max_latency_us=" << *std::max_element(maxes.begin(), maxes.end()) << "\n";
  }
}
