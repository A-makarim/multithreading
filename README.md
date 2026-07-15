# threadpool-cpp

A hand-rolled C++ thread pool, built from first principles with only the standard
library (`std::thread`, `std::mutex`, `std::condition_variable`, `std::atomic`,
`std::future`). The journey goes from *proving* a race condition exists, to fixing
it, to a reusable `ThreadPool` that runs arbitrary work and returns results.

## Stages

1. **Prove the problem** — an unsynchronised shared counter gives wrong, varying results.
2. **Fix it two ways** — `std::mutex` vs `std::atomic`, with timings.
3. **Bounded queue** — a thread-safe producer-consumer `BoundedQueue<T>`.
4. **Thread pool** — `submit()` returning `std::future`, clean shutdown.
5. **Break it on purpose** — a deadlock/hang, diagnosed live with `gdb`.
6. **Benchmark** (optional) — throughput vs `std::async`.

## Build

```bash
g++ -std=c++17 -pthread src/<file>.cpp -o <name>
```
