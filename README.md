# ThreadForge

ThreadForge is an educational C++20/Linux multithreaded TCP job server. It is
small enough to explain line-by-line while demonstrating production-style
design considerations: explicit socket ownership, bounded back-pressure,
message-safe concurrent writes, exception-safe statistics, and deterministic
shutdown. It is not production-ready.

## Architecture and thread model

```text
main / signal flag
       |
       v
accept loop ---- shared active-connection registry
       |
       +--> one blocking reader thread per client
                    |
                    v
             validated Job + shared_ptr<ClientConnection>
                    |
                    v
             BoundedQueue<Job> (back-pressure)
                    |
                    v
             fixed worker threads
                    |
                    v
           PRIME / SORT / MATMUL / HASH
                    |
                    v
       ClientConnection::send_line() mutex --> client
```

The accept loop creates one reader thread for each client. Readers perform
newline framing and validation but no CPU-heavy work. A fixed set of workers
drains the bounded queue and executes jobs. The separate `ThreadPool` class and
early-stage programs are retained as educational steps; the TCP server uses its
own typed job queue so its lifecycle remains visible.

`ClientConnection` owns one socket and closes it exactly once. It is shared by
the reader, queued jobs, workers, and the active-client registry because any of
those may legitimately outlive another. Its atomic open flag provides cheap
state observation; its mutex protects the compound send/close operations. The
same mutex covers every byte of a response, including partial `send()` retries,
so two workers cannot interleave protocol lines on one TCP stream.

The queue blocks producers at its fixed capacity instead of allowing unbounded
memory growth. Queue statistics are updated inside the successful enqueue's
critical section, preventing a fast consumer from decrementing before the
matching increment.

## Protocol and limits

Requests are one line each (maximum 4096 bytes):

```text
PRIME <positive uint64>
SORT <count from 1 to 2000000>
MATMUL <size from 1 to 512>
HASH <non-empty text>
```

Every parsed line receives a server job ID. Responses are:

```text
OK <job_id> <result>
ERROR <job_id> <message>
```

Jobs from one client can finish out of order because multiple workers execute
them concurrently. Clients must correlate responses by job ID and must not rely
on response order. Invalid integers, unsupported commands, oversized requests,
and out-of-range arguments receive explicit errors. The limits prevent client
input from directly causing unbounded allocations.

## Shutdown

Ctrl+C only sets a `sig_atomic_t` flag. Normal program control then:

1. atomically marks the server stopping (making `stop()` idempotent);
2. shuts down and closes the listener to unblock `accept()`;
3. copies active shared connections under the registry mutex, releases it, and
   shuts them down to unblock `recv()`;
4. closes the bounded queue, rejecting blocked producers and waking workers;
5. joins client readers, lets already queued jobs finish safely, then joins workers;
6. releases registry references and prints final statistics.

No registry lock is held across network operations or joins. `MSG_NOSIGNAL` is
used where available, so a disconnected peer cannot terminate the process with
`SIGPIPE`.

## Build and run

On Linux, or from an Ubuntu WSL shell on Windows:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/threadforge_server 9000 4 1024
```

Arguments are port, worker count, and queue capacity. In another terminal:

```bash
printf 'PRIME 10000019\nHASH hello world\n' |
  ./build/threadforge_client 127.0.0.1 9000
```

## Tests and sanitizers

### Test cases

The project includes four comprehensive test suites:

1. **bounded_queue** (`tests/queue_test.cpp`) - Tests multi-producer/multi-consumer
   integrity, wake-on-close, and queue statistics under concurrent access.

2. **thread_pool** (`tests/thread_pool_test.cpp`) - Tests thread pool futures,
   shutdown behavior, and task execution.

3. **client_connection** (`tests/client_connection_test.cpp`) - Tests concurrent
   connection writes, socket shutdown, and message safety.

4. **server_integration** (`tests/server_test.cpp`) - Multi-client loopback
   integration test covering parser limits, response IDs, and server shutdown.

### Running tests

```bash
ctest --test-dir build --output-on-failure
```

### Running with sanitizers

ASAN includes UndefinedBehaviorSanitizer. ASAN/UBSAN and TSAN cannot be enabled
together. On the benchmark WSL2 environment, the integration test passed under
ASAN/UBSAN. GCC TSAN could compile but could not start because WSL reported
`ThreadSanitizer: unexpected memory mapping`; run the documented TSAN build on
a native Linux host or CI runner for a meaningful race check.

```bash
cmake -S . -B build-asan -DENABLE_ASAN=ON
cmake --build build-asan -j
ctest --test-dir build-asan --output-on-failure

cmake -S . -B build-tsan -DENABLE_TSAN=ON
cmake --build build-tsan -j
ctest --test-dir build-tsan --output-on-failure
```

## Reproducible benchmarks

Run one workload:

```bash
./build/load_test --host 127.0.0.1 --port 9000 --clients 25 \
  --requests-per-client 100 --workload prime --prime-value 32416190071
```

Modes are `prime`, `sort`, `matmul`, and `mixed`. Mixed is a deterministic cycle
of 50% PRIME, 30% SORT, and 20% MATMUL with client offsets. Each client keeps one
request outstanding, measures round-trip latency with `steady_clock`, validates
the returned job ID, and reports successes, failures, duration, throughput,
mean, median, p95, p99, and maximum latency.

The matrix script restarts the server for 1/2/4/8 workers and
1/5/10/25/50/100 clients, then writes `benchmarks/results.csv`:

```bash
BUILD_DIR=build REQUESTS_PER_CLIENT=50 WORKLOAD=prime \
  bash benchmarks/run_benchmarks.sh
```

### Measured summary

Real results were measured using Ubuntu 24.04 under WSL2, GCC 13.3.0 `-O2`,
and an Intel Core i5-13500HX (10 cores / 20 logical CPUs). All 38,200 requests
across the matrix succeeded.

| Workers | Throughput at 100 clients | Mean latency | p99 latency |
| ---: | ---: | ---: | ---: |
| 1 | 2,403 req/s | 40.81 ms | 46.01 ms |
| 2 | 4,505 req/s | 21.62 ms | 35.15 ms |
| 4 | 8,491 req/s | 11.36 ms | 16.70 ms |
| 8 | 13,659 req/s | 6.88 ms | 9.98 ms |

The complete 24-run table and honest interpretation are in
[`benchmarks/results.md`](benchmarks/results.md); raw values are in
[`benchmarks/results.csv`](benchmarks/results.csv). CPU-heavy PRIME work scaled
with workers until synchronization and hardware effects reduced the incremental
gain. High client counts increased queueing and tail latency, especially with
one or two workers. Tiny jobs and one outstanding request do not expose the same
parallel speedup. These numbers are machine-specific single runs, not universal
performance claims.

For additional Linux process measurements:

```bash
/usr/bin/time -v ./build/threadforge_server 9000 4 1024  # elapsed, RSS, switches
pidstat -p <server-pid> 1                                # CPU over time
perf stat -p <server-pid>                                # cycles and switches
```

## Known limitations and future work

- One blocking reader thread per client does not scale like `epoll`.
- Newline framing is text-only; binary data needs length-prefixed framing.
- There is no request cancellation, authentication, TLS, priority queue, or
  structured logging.
- Job IDs are assigned by the server, not supplied by clients; highly pipelined
  clients need their own correlation convention or a future protocol extension.
- `std::hash` is implementation-defined and is not a cryptographic hash.
- The benchmark uses one outstanding request per connection and single runs;
  repeated trials and latency histograms would improve statistical confidence.
- Future work could add `epoll`, request cancellation, priorities, structured
  logs, TLS, NUMA-aware worker placement, and stable client request IDs.
