# Benchmark Results

Measured in Ubuntu 24.04 under WSL2 (Linux 6.6.87.2-microsoft-standard-WSL2),
compiled with GCC 13.3.0 using C++20 and `-O2`. The host CPU was a 13th Gen
Intel Core i5-13500HX exposed to WSL as 10 cores / 20 logical CPUs. Each cell
used queue capacity 1024, 50 requests per client, and `PRIME 32416190071`. The
server was restarted for every cell. These are single-run observations, not
confidence intervals.

| Workers | Clients | Requests | Success | Failure | Req/s | Mean ms | Median ms | p95 ms | p99 ms | Max ms |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 1 | 50 | 50 | 0 | 1607.47 | 0.605 | 0.571 | 0.829 | 0.933 | 0.933 |
| 1 | 5 | 250 | 250 | 0 | 2343.38 | 2.100 | 2.100 | 2.530 | 2.685 | 2.732 |
| 1 | 10 | 500 | 500 | 0 | 2456.45 | 4.005 | 3.980 | 4.291 | 4.634 | 4.722 |
| 1 | 25 | 1250 | 1250 | 0 | 2415.89 | 10.175 | 10.203 | 11.049 | 11.203 | 11.507 |
| 1 | 50 | 2500 | 2500 | 0 | 2473.72 | 19.835 | 19.927 | 21.054 | 21.650 | 22.226 |
| 1 | 100 | 5000 | 5000 | 0 | 2403.29 | 40.813 | 41.173 | 44.674 | 46.011 | 46.954 |
| 2 | 1 | 50 | 50 | 0 | 1742.97 | 0.563 | 0.540 | 0.696 | 1.007 | 1.007 |
| 2 | 5 | 250 | 250 | 0 | 3697.57 | 1.286 | 1.087 | 2.431 | 3.625 | 4.007 |
| 2 | 10 | 500 | 500 | 0 | 4887.45 | 2.000 | 1.971 | 2.238 | 3.113 | 3.679 |
| 2 | 25 | 1250 | 1250 | 0 | 4510.00 | 5.405 | 5.256 | 6.873 | 8.507 | 9.562 |
| 2 | 50 | 2500 | 2500 | 0 | 4774.38 | 10.222 | 10.117 | 11.784 | 16.367 | 18.302 |
| 2 | 100 | 5000 | 5000 | 0 | 4504.53 | 21.616 | 20.912 | 27.720 | 35.150 | 37.492 |
| 4 | 1 | 50 | 50 | 0 | 1555.43 | 0.629 | 0.597 | 0.878 | 0.984 | 0.984 |
| 4 | 5 | 250 | 250 | 0 | 7298.50 | 0.650 | 0.623 | 0.868 | 1.144 | 1.256 |
| 4 | 10 | 500 | 500 | 0 | 7821.94 | 1.224 | 1.206 | 1.581 | 1.907 | 2.062 |
| 4 | 25 | 1250 | 1250 | 0 | 8220.01 | 2.945 | 2.942 | 3.886 | 4.470 | 5.168 |
| 4 | 50 | 2500 | 2500 | 0 | 8467.07 | 5.666 | 5.475 | 7.208 | 9.169 | 9.416 |
| 4 | 100 | 5000 | 5000 | 0 | 8491.00 | 11.357 | 11.205 | 14.364 | 16.701 | 17.858 |
| 8 | 1 | 50 | 50 | 0 | 1589.24 | 0.615 | 0.576 | 0.887 | 0.912 | 0.912 |
| 8 | 5 | 250 | 250 | 0 | 6744.77 | 0.699 | 0.648 | 0.966 | 1.309 | 1.851 |
| 8 | 10 | 500 | 500 | 0 | 9897.09 | 0.902 | 0.769 | 1.477 | 5.117 | 7.935 |
| 8 | 25 | 1250 | 1250 | 0 | 13618.90 | 1.723 | 1.687 | 2.384 | 3.770 | 6.037 |
| 8 | 50 | 2500 | 2500 | 0 | 13315.30 | 3.527 | 3.479 | 4.752 | 7.089 | 13.101 |
| 8 | 100 | 5000 | 5000 | 0 | 13659.00 | 6.882 | 7.025 | 8.335 | 9.978 | 16.863 |

The raw, machine-readable output is in `results.csv`. At saturation, throughput
scaled from roughly 2.4k req/s with one worker to 4.5k, 8.5k, and 13.7k with
two, four, and eight workers. That is useful scaling but not linear: scheduling,
queue synchronization, connection writes, and CPU/cache effects consume part of
the added capacity. One-client runs do not benefit from extra workers because
the benchmark deliberately keeps one request outstanding per connection.

At 100 clients, p99 latency fell from 46.0 ms at one worker to 10.0 ms at eight
workers as the queue drained faster. Tail latency was noisier at low request
counts (for example, the 8-worker/10-client p99), which is why these single runs
should not be treated as stable microbenchmark claims. Results are specific to
this CPU, WSL2, compiler, workload, and background system activity.
