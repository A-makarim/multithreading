# ThreadForge: High-Performance Multithreaded TCP Job Server

ThreadForge is a C++20/Linux systems programming project that grows from small concurrency exercises into a realistic multithreaded TCP job server. The early stages remain in the repository because they explain the engineering path: first observe a data race, then fix it with mutexes and atomics, build a bounded producer-consumer queue, wrap work in a fixed thread pool, debug a deadlock, and finally compose those pieces into a network service.

## Architecture

```text
Clients
   |
   v
TCP Listener / accept thread
   |
   v
Per-client reader + Request Parser
   |
   v
Bounded Blocking Queue<Job>
   |
   v
Fixed Worker Thread Pool
   |
   v
Job Execution: PRIME / SORT / MATMUL / HASH
   |
   v
Client Response: OK result / ERROR message
```

The networking path never performs CPU-heavy work. It accepts sockets, reads one-line commands, validates them into `Job` objects, and applies back-pressure by pushing into the bounded queue. Worker threads are the only threads that execute jobs and send the corresponding response to the client socket.

## Thread model

* **Listener thread** owns `accept(2)` and creates one lightweight client-reader thread per connection.
* **Client-reader threads** call `recv(2)`, parse newline-delimited commands, and enqueue jobs.
* **Worker threads** block on the queue, execute jobs, update atomics, and write responses.
* **Main thread** handles Ctrl+C and coordinates graceful shutdown.

This is intentionally not an event-loop framework. The goal is to make POSIX sockets, blocking synchronization, ownership, shutdown, and contention visible enough to discuss in an interview.

## Job protocol

Send one command per line:

```text
PRIME 10000019
SORT 500000
MATMUL 256
HASH hello world
```

Responses are newline-delimited:

```text
OK prime
OK sorted 500000 checksum ...
ERROR unknown command
```

## Synchronization strategy

* `BoundedQueue<T>` uses one mutex to protect the queue and two condition variables: `not_empty` for consumers and `not_full` for producers.
* Predicate waits are used because condition variables can wake spuriously; every wake re-checks the real state.
* `notify_one` is used after normal push/pop because one new slot or one new item can satisfy exactly one opposite-side waiter.
* `notify_all` is used only for shutdown because every sleeping producer and consumer must observe that the queue is closed.
* Runtime counters use `std::atomic<std::uint64_t>` because they are independent scalar measurements; a mutex would serialize unrelated statistics reads/writes on a hot path.
* The worker count is fixed to avoid unbounded thread creation under load. Queue capacity is fixed so overload becomes back-pressure instead of memory growth.

## Build

```bash
cmake -S . -B build
cmake --build build -j
```

## Run

Start the server:

```bash
./build/threadforge_server 9000 4 1024
```

Use the interactive client:

```bash
printf 'PRIME 10000019\nHASH hello world\n' | ./build/threadforge_client 127.0.0.1 9000
```

Run the preserved learning stages:

```bash
./build/stage1_race_condition
./build/stage2_synchronised
./build/stage3_queue_demo
./build/stage4_thread_pool_demo
./build/stage5_deadlock_demo   # intentionally hangs
```

## Tests and benchmarks

```bash
./build/queue_test
./build/thread_pool_test
./build/server_test
./build/load_test 127.0.0.1 9000 100
```

The benchmark harness runs 1, 5, 10, 25, 50, and 100 clients. To compare worker counts, restart the server with 1, 2, 4, and 8 workers and record the output in `benchmarks/results.md`.

## Repository map

* `include/` contains reusable project interfaces: queue, job model, statistics, server, and thread pool.
* `src/` contains the server implementation and preserved educational stages.
* `client/` contains a minimal TCP client for manual testing.
* `benchmarks/` contains the load generator and results template.
* `tests/` contains small executable tests that avoid third-party frameworks.
* `docs/` documents each learning stage.

## Lessons learned

A concurrent server is mostly about ownership and failure modes. The interesting parts are not only the happy-path algorithm; they are bounded memory, wakeup discipline, clean shutdown, broken client sockets, and keeping CPU work out of networking threads. ThreadForge keeps the code small enough to explain line-by-line while still exercising real Linux APIs and production-style concurrency decisions.

## Future work

* Add length-prefixed protocol framing for binary-safe requests.
* Replace per-client reader threads with `epoll` while keeping the same worker queue.
* Add latency histograms and percentile reporting to the benchmark harness.
* Add structured logging and configurable job mixes.
