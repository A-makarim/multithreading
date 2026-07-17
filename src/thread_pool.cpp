#include "thread_pool.hpp"

ThreadPool::ThreadPool(std::size_t num_threads) {
  if (num_threads == 0) throw std::invalid_argument("thread count must be positive");
  for (std::size_t i = 0; i < num_threads; ++i)
    workers_.emplace_back([this] { worker_loop(); });
}

ThreadPool::~ThreadPool() { shutdown(); }

void ThreadPool::shutdown() noexcept {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stop_) return;
    stop_ = true;
  }
  cv_.notify_all();  // wake every worker so none stays blocked on wait()
  for (auto& w : workers_)
    if (w.joinable()) w.join();
}

// Each worker loops: wait for work (or stop), take one task, run it, repeat.
void ThreadPool::worker_loop() {
  for (;;) {
    std::function<void()> task;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
      if (stop_ && tasks_.empty()) return;  // drain remaining tasks, then exit
      task = std::move(tasks_.front());
      tasks_.pop();
    }
    task();  // run outside the lock so other workers can grab tasks meanwhile
  }
}
