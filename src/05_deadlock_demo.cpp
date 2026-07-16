#include <chrono>
#include <cstdio>
#include <mutex>
#include <thread>

// Deliberate AB-BA deadlock. worker_ab locks a then b; worker_ba locks b then a.
// The sleeps guarantee each thread grabs its first lock before trying the second,
// so each ends up waiting forever for the lock the other holds.
std::mutex mutex_a;
std::mutex mutex_b;

void worker_ab() {
  std::lock_guard<std::mutex> la(mutex_a);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  std::lock_guard<std::mutex> lb(mutex_b);  // stuck: mutex_b held by worker_ba
  std::printf("worker_ab acquired both\n");
}

void worker_ba() {
  std::lock_guard<std::mutex> lb(mutex_b);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  std::lock_guard<std::mutex> la(mutex_a);  // stuck: mutex_a held by worker_ab
  std::printf("worker_ba acquired both\n");
}

int main() {
  std::printf("starting deadlock demo (will hang)...\n");
  std::fflush(stdout);
  std::thread t1(worker_ab);
  std::thread t2(worker_ba);
  t1.join();
  t2.join();
  std::printf("done\n");
  return 0;
}
