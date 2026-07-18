# Load Testing Guide

This document provides comprehensive guidance on using ThreadForge's load testing tool to measure performance and identify bottlenecks.

## Quick Start

### Basic Usage

```bash
# Start the server
./build/threadforge_server 9000 4 1024 &

# Run a simple load test
./build/load_test --host 127.0.0.1 --port 9000 --clients 10 \
  --requests-per-client 100 --workload prime --prime-value 10000019
```

### Output Example

```
Workload: prime (value=10000019)
Clients: 10, Requests per client: 100
Total requests: 1000

Results:
  Successes: 1000
  Failures: 0
  Duration: 2.345 seconds
  Throughput: 426.4 req/s
  
  Latency (ms):
    Mean: 23.45
    Median: 22.10
    p95: 35.20
    p99: 42.50
    Max: 48.30
```

## Workload Types

### PRIME Workload

Tests CPU-intensive prime number checking.

**Usage**:
```bash
./build/load_test --workload prime --prime-value <value>
```

**Parameters**:
- `--prime-value`: The number to check for primality (positive uint64)

**Characteristics**:
- CPU-bound
- Scales with worker threads
- Good for testing CPU performance
- Deterministic results

**Examples**:
```bash
# Quick test (small prime)
./build/load_test --workload prime --prime-value 1000019

# Medium test
./build/load_test --workload prime --prime-value 10000019

# Heavy test (large prime)
./build/load_test --workload prime --prime-value 100000007
```

### SORT Workload

Tests memory-intensive sorting.

**Usage**:
```bash
./build/load_test --workload sort --sort-count <count>
```

**Parameters**:
- `--sort-count`: Number of elements to sort (1 to 2,000,000)

**Characteristics**:
- Memory-bound
- Scales with available memory
- Good for testing memory performance
- Deterministic results

**Examples**:
```bash
# Quick test (small sort)
./build/load_test --workload sort --sort-count 1000

# Medium test
./build/load_test --workload sort --sort-count 100000

# Heavy test (large sort)
./build/load_test --workload sort --sort-count 1000000
```

### MATMUL Workload

Tests matrix multiplication (CPU and memory).

**Usage**:
```bash
./build/load_test --workload matmul --matmul-size <size>
```

**Parameters**:
- `--matmul-size`: Matrix size (1 to 512)

**Characteristics**:
- CPU and memory bound
- Scales with worker threads and memory
- Good for testing balanced workloads
- Deterministic results

**Examples**:
```bash
# Quick test (small matrix)
./build/load_test --workload matmul --matmul-size 64

# Medium test
./build/load_test --workload matmul --matmul-size 256

# Heavy test (large matrix)
./build/load_test --workload matmul --matmul-size 512
```

### MIXED Workload

Tests a mix of different job types.

**Usage**:
```bash
./build/load_test --workload mixed
```

**Characteristics**:
- Deterministic cycle: 50% PRIME, 30% SORT, 20% MATMUL
- Client offsets ensure variety
- Good for realistic workloads
- Tests server's ability to handle diverse jobs

**Example**:
```bash
./build/load_test --host 127.0.0.1 --port 9000 --clients 25 \
  --requests-per-client 100 --workload mixed
```

## Output Interpretation

### Throughput

**Definition**: Requests processed per second

**Formula**: `Total Requests / Duration (seconds)`

**Interpretation**:
- Higher is better
- Scales with worker threads (up to a point)
- Affected by job complexity
- Affected by client concurrency

**Example**:
```
Throughput: 426.4 req/s
```

This means the server processed 426.4 requests per second on average.

### Latency Metrics

**Mean Latency**: Average response time
- Useful for overall performance
- Can be skewed by outliers

**Median Latency**: 50th percentile
- Represents typical response time
- Less affected by outliers
- Good for user experience

**p95 Latency**: 95th percentile
- 95% of requests are faster than this
- Indicates tail latency
- Important for SLAs

**p99 Latency**: 99th percentile
- 99% of requests are faster than this
- Indicates worst-case latency
- Critical for user experience

**Max Latency**: Maximum response time
- Worst-case scenario
- Can indicate outliers or GC pauses
- Use with caution (single data point)

**Example**:
```
Latency (ms):
  Mean: 23.45
  Median: 22.10
  p95: 35.20
  p99: 42.50
  Max: 48.30
```

Interpretation:
- Typical response: ~22 ms
- 95% of requests: < 35 ms
- 99% of requests: < 42 ms
- Worst case: 48 ms

### Success/Failure Rates

**Successes**: Number of successful requests

**Failures**: Number of failed requests

**Interpretation**:
- Should be 100% success rate
- Failures indicate:
  - Server errors
  - Connection issues
  - Queue overflow
  - Invalid arguments

**Example**:
```
Successes: 1000
Failures: 0
```

All requests succeeded.

## Load Testing Scenarios

### Scenario 1: Baseline Testing

**Goal**: Establish baseline performance

**Procedure**:

```bash
# Test with default configuration
./build/threadforge_server 9000 4 1024 &
SERVER_PID=$!

# Run baseline test
./build/load_test --host 127.0.0.1 --port 9000 --clients 10 \
  --requests-per-client 100 --workload prime --prime-value 10000019

# Kill server
kill $SERVER_PID
```

**Record**:
- Throughput
- Mean/median/p95/p99 latency
- Success rate
- Server configuration (workers, queue size)
- Hardware specs
- Date/time

### Scenario 2: Scaling Tests

**Goal**: Understand how performance scales with workers

**Procedure**:

```bash
# Test with 1 worker
./build/threadforge_server 9000 1 1024 &
SERVER_PID=$!
./build/load_test --clients 25 --requests-per-client 100 --workload prime --prime-value 10000019
kill $SERVER_PID
sleep 2

# Test with 2 workers
./build/threadforge_server 9000 2 1024 &
SERVER_PID=$!
./build/load_test --clients 25 --requests-per-client 100 --workload prime --prime-value 10000019
kill $SERVER_PID
sleep 2

# Test with 4 workers
./build/threadforge_server 9000 4 1024 &
SERVER_PID=$!
./build/load_test --clients 25 --requests-per-client 100 --workload prime --prime-value 10000019
kill $SERVER_PID
sleep 2

# Test with 8 workers
./build/threadforge_server 9000 8 1024 &
SERVER_PID=$!
./build/load_test --clients 25 --requests-per-client 100 --workload prime --prime-value 10000019
kill $SERVER_PID
```

**Analysis**:
- Plot throughput vs worker count
- Identify scaling efficiency
- Find optimal worker count
- Identify bottlenecks

### Scenario 3: Workload Comparison

**Goal**: Compare performance across different workloads

**Procedure**:

```bash
# Start server
./build/threadforge_server 9000 4 1024 &
SERVER_PID=$!

# Test PRIME
echo "Testing PRIME..."
./build/load_test --clients 10 --requests-per-client 100 --workload prime --prime-value 10000019

# Test SORT
echo "Testing SORT..."
./build/load_test --clients 10 --requests-per-client 100 --workload sort --sort-count 100000

# Test MATMUL
echo "Testing MATMUL..."
./build/load_test --clients 10 --requests-per-client 100 --workload matmul --matmul-size 256

# Test MIXED
echo "Testing MIXED..."
./build/load_test --clients 10 --requests-per-client 100 --workload mixed

kill $SERVER_PID
```

**Analysis**:
- Compare throughput across workloads
- Compare latency across workloads
- Identify which workload is most demanding
- Understand server behavior under different loads

### Scenario 4: Regression Detection

**Goal**: Detect performance regressions

**Procedure**:

1. **Establish baseline** (see Scenario 1)
2. **Make code changes**
3. **Run same test** with same parameters
4. **Compare results**

**Example**:

```bash
# Baseline (before changes)
echo "Baseline test..."
./build/load_test --clients 25 --requests-per-client 100 --workload prime --prime-value 10000019 > baseline.txt

# After changes
echo "After changes..."
./build/load_test --clients 25 --requests-per-client 100 --workload prime --prime-value 10000019 > after.txt

# Compare
diff baseline.txt after.txt
```

**Thresholds**:
- Throughput regression > 5%: Investigate
- Latency increase > 10%: Investigate
- Failure rate > 0%: Critical

## Best Practices

### 1. Multiple Runs for Statistical Significance

**Why**: Single runs can be affected by system noise

**How**:

```bash
# Run test 3 times
for i in 1 2 3; do
  echo "Run $i"
  ./build/load_test --clients 25 --requests-per-client 100 --workload prime --prime-value 10000019
  sleep 2
done
```

**Analysis**:
- Calculate mean and standard deviation
- Identify outliers
- Establish confidence intervals

### 2. Warm-up Runs

**Why**: First run may have cache misses or initialization overhead

**How**:

```bash
# Warm-up run (discard results)
./build/load_test --clients 5 --requests-per-client 10 --workload prime --prime-value 10000019

# Actual test
./build/load_test --clients 25 --requests-per-client 100 --workload prime --prime-value 10000019
```

### 3. Consistent Test Environment

**Why**: Different environments produce different results

**How**:
- Use same hardware
- Use same OS/compiler
- Minimize background processes
- Use Docker for reproducibility
- Document environment details

### 4. Vary Client Count

**Why**: Performance varies with concurrency

**How**:

```bash
# Test with different client counts
for clients in 1 5 10 25 50 100; do
  echo "Testing with $clients clients..."
  ./build/load_test --clients $clients --requests-per-client 100 --workload prime --prime-value 10000019
  sleep 2
done
```

### 5. Document Results

**Why**: Need to track performance over time

**How**:

```bash
# Save results with metadata
echo "Date: $(date)" > results.txt
echo "Hardware: $(uname -a)" >> results.txt
echo "Compiler: $(g++ --version | head -1)" >> results.txt
echo "Server config: 4 workers, 1024 queue" >> results.txt
echo "" >> results.txt
./build/load_test --clients 25 --requests-per-client 100 --workload prime --prime-value 10000019 >> results.txt
```

## Advanced Topics

### Custom Workloads

To add custom workloads, modify the load_test source code:

1. Add new workload type in `client/load_test.cpp`
2. Implement workload logic
3. Add command-line option
4. Rebuild: `cmake --build build -j`

### Automated Benchmark Scripts

See `benchmarks/run_benchmarks.sh` for an example of automated benchmarking:

```bash
BUILD_DIR=build REQUESTS_PER_CLIENT=50 WORKLOAD=prime \
  bash benchmarks/run_benchmarks.sh
```

This script:
- Tests multiple worker counts (1, 2, 4, 8)
- Tests multiple client counts (1, 5, 10, 25, 50, 100)
- Generates CSV output
- Produces summary statistics

### Performance Tuning

Based on load test results:

1. **Low throughput**:
   - Increase worker threads
   - Reduce job complexity
   - Increase queue capacity

2. **High latency**:
   - Increase worker threads
   - Reduce client concurrency
   - Reduce job complexity

3. **High CPU usage**:
   - Reduce job complexity
   - Reduce client concurrency
   - Profile with perf

4. **High memory usage**:
   - Reduce queue capacity
   - Reduce client concurrency
   - Use smaller job sizes

## Comparing Results

### Manual Comparison

```bash
# Run two tests and compare
echo "Test 1: 4 workers"
./build/load_test --clients 25 --requests-per-client 100 --workload prime --prime-value 10000019 > test1.txt

echo "Test 2: 8 workers"
./build/load_test --clients 25 --requests-per-client 100 --workload prime --prime-value 10000019 > test2.txt

# Compare key metrics
grep "Throughput" test1.txt test2.txt
grep "Mean:" test1.txt test2.txt
```

### CSV Analysis

The benchmark script produces CSV output:

```bash
# View results
cat benchmarks/results.csv

# Analyze with tools
awk -F',' '{print $1, $2, $3}' benchmarks/results.csv | column -t
```

## References

- [README.md](../README.md#reproducible-benchmarks)
- [Error Handling Guide](error-handling.md)
- [Platform Support Matrix](platform-support.md)
- [Profiling Guide](profiling.md)
- [Troubleshooting Guide](troubleshooting.md)
