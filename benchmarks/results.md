# Benchmark Results

Run the server with 1, 2, 4, and 8 workers, then run:

```bash
./build/load_test 127.0.0.1 9000 100
```

| Workers | Clients | Jobs/client | Jobs/sec | Avg latency | Max latency | Notes |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| 1 | 1/5/10/25/50/100 | 100 | TBD | TBD | TBD | Fill on target Linux host |
| 2 | 1/5/10/25/50/100 | 100 | TBD | TBD | TBD | Fill on target Linux host |
| 4 | 1/5/10/25/50/100 | 100 | TBD | TBD | TBD | Fill on target Linux host |
| 8 | 1/5/10/25/50/100 | 100 | TBD | TBD | TBD | Fill on target Linux host |

The harness prints jobs/sec and worst observed latency. Keep CPU governor, compiler, and hardware details with real results.
