#pragma once
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

// A fixed-size pool of worker threads that pull callables off a shared queue.
// submit() returns a std::future so the caller can retrieve the task's result.
class ThreadPool {
 public:
  explicit ThreadPool(std::size_t num_threads);
  ~ThreadPool();

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

  // Enqueue f(args...) to run on a worker; get a future for its result.
  template <class F, class... Args>
  auto submit(F&& f, Args&&... args)
      -> std::future<std::invoke_result_t<F, Args...>> {
    using R = std::invoke_result_t<F, Args...>;

    // packaged_task ties the call to a future. shared_ptr so the type-erased
    // std::function below can copy it around.
    auto task = std::make_shared<std::packaged_task<R()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    std::future<R> result = task->get_future();

    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (stop_) throw std::runtime_error("submit() on stopped ThreadPool");
      tasks_.emplace([task] { (*task)(); });
    }
    cv_.notify_one();
    return result;
  }

 private:
  void worker_loop();

  std::vector<std::thread> workers_;
  std::queue<std::function<void()>> tasks_;
  std::mutex mutex_;
  std::condition_variable cv_;
  bool stop_ = false;
};
