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

## Results

### Stage 1 — the race is real

10 threads × 100,000 increments should give **1,000,000**. With no synchronisation
(built `-O0`), it never does, and differs every run:

At `-O0`, we see a clear effect. `-O2` may fold whole loop as counter += 100000, hence NOT a real load-add-store, 3 instruction per loop iteration

| Run | 1 | 2 | 3 | 4 | 5 |
| --- | - | - | - | - | - |
| actual | 615584 | 491773 | 892655 | 872566 | 993514 |

`++counter` is a load-add-store; threads read the same value, increment, and write
back, silently overwriting each other's updates. The lost updates are the missing
count.
