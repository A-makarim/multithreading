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

### Stage 2 — two correct fixes, and their cost

Same workload, built `-O2`, averaged over 5 runs:

| Version | Result | Avg time |
| ------- | ------ | -------- |
| baseline (racy) | 1,000,000\* | ~3.4 ms |
| mutex | 1,000,000 | ~80 ms |
| atomic | 1,000,000 | ~15 ms |

\* Fast but **not safe** — at `-O2` the compiler folds the loop into one add per
thread, so it *happens* to come out right; Stage 1 (`-O0`) shows it genuinely wrong.

**Why atomic wins:** `++` on a `std::atomic<int>` compiles to a single lock-free CPU
instruction (an atomic add / compare-and-swap). A `std::mutex` must lock and unlock
around every increment, and under contention that can drop into the kernel and cause
context switches — far more expensive than one CPU instruction.

**When to reach for a mutex instead:** atomics only make a *single* operation atomic.
Use a mutex when you must protect **more than one variable**, or a **critical section
of several steps** that has to happen as one indivisible unit (e.g. push to a queue
*and* update a size counter *and* signal a condition variable together).

### Stage 3 — thread-safe bounded queue

`BoundedQueue<T>`: `push()` blocks while full, `pop()` blocks while empty, using one
mutex and two condition variables (`not_full`, `not_empty`). Test: 2 producers +
3 consumers move 10,000 items; the consumed sum/count must match expected.

```
items: expected=10000 consumed=10000
sum:   expected=25005000 consumed=25005000  -> OK   (every run)
```

Key points: `pop`/`push` wait on a **predicate** (`wait(lock, pred)`), which loops
internally to survive *spurious wakeups*; the mutex is released while waiting and
re-acquired on wake; and we `notify` after unlocking to avoid waking a thread that
would immediately block on the mutex.

### Stage 4 — the thread pool

`ThreadPool(N)` starts N workers that pull callables off a shared queue.
`submit(f, args...)` wraps the call in a `std::packaged_task` and returns a
`std::future` for the result. The destructor sets a stop flag, `notify_all()`s so
no worker is left blocked, and joins every thread.

Demo: 1,000 `i*i` tasks on 4 threads, results collected via futures:

```
tasks=1000  sum=332833500  expected=332833500  -> OK   (every run)
```
