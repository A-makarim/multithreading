# Performance Profiling Guide

This document provides comprehensive guidance on profiling ThreadForge to identify performance bottlenecks and optimize performance.

## Quick Start

### Basic Resource Usage

```bash
# Measure elapsed time, memory usage, and context switches
/usr/bin/time -v ./build/threadforge_server 9000 4 1024
```

**Output Example**:
```
        Elapsed (wall clock) time (h:mm:ss or m:ss): 0:05.23
        Maximum resident set size (kbytes): 12345
        Voluntary context switches: 1234
        Involuntary context switches: 567
```

### Per-Process Monitoring

```bash
# Monitor CPU and I/O over time
pidstat -p <server-pid> 1
```

**Output Example**:
```
Linux 5.15.0 (hostname)  07/18/2026  _x86_64_

02:30:45 PM   UID       PID  %usr %sys %guest   %wait %CPU CPU minflt majflt
02:30:46 PM  1000     12345  45.0  5.0    0.0   10.0 50.0   0   1234    0
02:30:47 PM  1000     12345  48.0  4.5    0.0    8.0 52.5   0   1200    0
```

### CPU Profiling

```bash
# Collect CPU cycles and context switches
perf stat -p <server-pid>
```

**Output Example**:
```
 Performance counter stats for process id '12345':

      5,234.45 msec task-clock                #    1.000 CPUs utilized
           123 context-switches              #   23.5 /sec
            45 cpu-migrations                #    8.6 /sec
         1,234 page-faults                   #  235.6 /sec
   18,234,567,890 cycles                     #    3.48 GHz
   12,345,678,901 instructions               #    0.68 insn per cycle
    2,345,678,901 branches                   #  448.1 M/sec
      123,456,789 branch-misses              #    5.26% of all branches
```

## Profiling Tools

### 1. /usr/bin/time - Resource Usage

**Purpose**: Measure overall resource consumption

**Usage**:
```bash
/usr/bin/time -v ./build/threadforge_server 9000 4 1024
```

**Key Metrics**:
- **Elapsed time**: Total wall-clock time
- **User time**: CPU time in user space
- **System time**: CPU time in kernel space
- **Maximum resident set size**: Peak memory usage
- **Voluntary context switches**: Thread yields
- **Involuntary context switches**: Preemptions
- **Page faults**: Memory access patterns

**Interpretation**:
- High user time: CPU-bound workload
- High system time: I/O or synchronization overhead
- High memory: Memory-intensive workload
- High context switches: Contention or many threads

**Example**:
```bash
# Profile server startup and shutdown
/usr/bin/time -v ./build/threadforge_server 9000 4 1024 &
SERVER_PID=$!
sleep 2
kill $SERVER_PID
wait
```

### 2. pidstat - Per-Process Monitoring

**Purpose**: Monitor process activity over time

**Usage**:
```bash
# Monitor every 1 second
pidstat -p <server-pid> 1

# Monitor CPU and I/O
pidstat -p <server-pid> -u -d 1

# Monitor for 10 samples
pidstat -p <server-pid> 1 10
```

**Key Metrics**:
- **%usr**: User CPU percentage
- **%sys**: System CPU percentage
- **%CPU**: Total CPU percentage
- **%wait**: I/O wait percentage
- **minflt**: Minor page faults
- **majflt**: Major page faults
- **kB_rd/s**: Kilobytes read per second
- **kB_wr/s**: Kilobytes written per second

**Interpretation**:
- High %usr: CPU-bound workload
- High %sys: Synchronization or I/O overhead
- High %wait: I/O-bound workload
- High majflt: Memory pressure

**Example**:
```bash
# Monitor server during load test
./build/threadforge_server 9000 4 1024 &
SERVER_PID=$!
sleep 1
pidstat -p $SERVER_PID -u -d 1 30 &
sleep 2
./build/load_test --clients 25 --requests-per-client 100 --workload prime --prime-value 10000019
wait
kill $SERVER_PID
```

### 3. perf stat - CPU Cycles and Events

**Purpose**: Collect detailed CPU performance counters

**Usage**:
```bash
# Basic CPU profiling
perf stat -p <server-pid>

# Collect specific events
perf stat -e cycles,instructions,cache-references,cache-misses -p <server-pid>

# Collect for 10 seconds
perf stat -p <server-pid> sleep 10
```

**Key Metrics**:
- **cycles**: CPU cycles executed
- **instructions**: Instructions executed
- **insn per cycle**: Instruction throughput
- **context-switches**: Context switches
- **cache-references**: Cache accesses
- **cache-misses**: Cache misses
- **branch-misses**: Branch prediction misses

**Interpretation**:
- Low insn per cycle: Stalls or cache misses
- High cache-misses**: Memory access patterns
- High branch-misses**: Unpredictable branches
- High context-switches**: Contention

**Example**:
```bash
# Profile server during load test
./build/threadforge_server 9000 4 1024 &
SERVER_PID=$!
sleep 1
perf stat -p $SERVER_PID -e cycles,instructions,cache-references,cache-misses,context-switches &
PERF_PID=$!
sleep 2
./build/load_test --clients 25 --requests-per-client 100 --workload prime --prime-value 10000019
kill $PERF_PID
kill $SERVER_PID
```

### 4. perf record and Flame Graphs

**Purpose**: Detailed call stack profiling

**Installation**:
```bash
# Install perf-tools
sudo apt-get install -y linux-tools-generic

# Install flamegraph tools
git clone https://github.com/brendangregg/FlameGraph.git
cd FlameGraph
```

**Usage**:
```bash
# Record call stacks
perf record -p <server-pid> -g -- sleep 30

# Generate flame graph
perf script | ./FlameGraph/stackcollapse-perf.pl | ./FlameGraph/flamegraph.pl > profile.svg

# View in browser
firefox profile.svg
```

**Interpretation**:
- Width of bar: Time spent in function
- Height of stack: Call depth
- Identify hot functions
- Identify unexpected call paths

**Example**:
```bash
# Profile server during load test
./build/threadforge_server 9000 4 1024 &
SERVER_PID=$!
sleep 1
perf record -p $SERVER_PID -g -- sleep 30 &
PERF_PID=$!
sleep 2
./build/load_test --clients 25 --requests-per-client 100 --workload prime --prime-value 10000019
wait $PERF_PID
kill $SERVER_PID

# Generate flame graph
perf script | stackcollapse-perf.pl | flamegraph.pl > profile.svg
```

### 5. Valgrind - Memory Profiling

**Purpose**: Detailed memory usage analysis

**Installation**:
```bash
sudo apt-get install -y valgrind
```

**Usage**:
```bash
# Memory profiling with Massif
valgrind --tool=massif --massif-out-file=massif.out ./build/threadforge_server 9000 4 1024

# View results
ms_print massif.out
```

**Interpretation**:
- Peak memory usage
- Memory allocation patterns
- Memory leaks
- Allocation hotspots

**Note**: Valgrind adds significant overhead. Use for debugging, not production profiling.

### 6. GDB - Interactive Debugging

**Purpose**: Interactive debugging and profiling

**Usage**:
```bash
# Start debugger
gdb ./build/threadforge_server

# Set breakpoint
(gdb) break main

# Run with arguments
(gdb) run 9000 4 1024

# Continue execution
(gdb) continue

# Print backtrace
(gdb) bt

# Print variable
(gdb) print variable_name
```

**Useful Commands**:
- `break <function>`: Set breakpoint
- `continue`: Continue execution
- `step`: Step into function
- `next`: Step over function
- `bt`: Print backtrace
- `print <var>`: Print variable
- `info threads`: List threads
- `thread <id>`: Switch thread

## Profiling Scenarios

### Scenario 1: Baseline Profiling

**Goal**: Establish baseline performance characteristics

**Procedure**:

```bash
# Start server
./build/threadforge_server 9000 4 1024 &
SERVER_PID=$!
sleep 1

# Collect baseline metrics
echo "=== Resource Usage ==="
/usr/bin/time -v sleep 5

echo "=== CPU Profiling ==="
perf stat -p $SERVER_PID sleep 5

echo "=== Per-Process Monitoring ==="
pidstat -p $SERVER_PID 1 5

kill $SERVER_PID
```

### Scenario 2: Bottleneck Identification

**Goal**: Identify performance bottlenecks

**Procedure**:

```bash
# Start server
./build/threadforge_server 9000 4 1024 &
SERVER_PID=$!
sleep 1

# Start profiling
perf record -p $SERVER_PID -g -- sleep 30 &
PERF_PID=$!
sleep 2

# Run load test
./build/load_test --clients 25 --requests-per-client 100 --workload prime --prime-value 10000019

wait $PERF_PID
kill $SERVER_PID

# Generate flame graph
perf script | stackcollapse-perf.pl | flamegraph.pl > profile.svg

# Analyze results
echo "Flame graph saved to profile.svg"
```

### Scenario 3: Memory Profiling

**Goal**: Understand memory usage patterns

**Procedure**:

```bash
# Profile with Valgrind
valgrind --tool=massif --massif-out-file=massif.out \
  ./build/threadforge_server 9000 4 1024 &
SERVER_PID=$!
sleep 2

# Run load test
./build/load_test --clients 10 --requests-per-client 50 --workload sort --sort-count 100000

kill $SERVER_PID
wait

# View results
ms_print massif.out
```

### Scenario 4: Concurrency Profiling

**Goal**: Understand thread behavior and contention

**Procedure**:

```bash
# Start server
./build/threadforge_server 9000 4 1024 &
SERVER_PID=$!
sleep 1

# Monitor context switches
echo "=== Context Switches ==="
perf stat -e context-switches,cpu-migrations -p $SERVER_PID sleep 10 &
PERF_PID=$!
sleep 2

# Run load test
./build/load_test --clients 50 --requests-per-client 100 --workload prime --prime-value 10000019

wait $PERF_PID
kill $SERVER_PID
```

## Result Interpretation

### CPU Cycles

**Definition**: Number of CPU cycles executed

**Interpretation**:
- Higher cycles = more work done
- Compare across runs to identify regressions
- Normalize by instructions for efficiency

### Instructions Per Cycle (IPC)

**Definition**: Average instructions executed per CPU cycle

**Interpretation**:
- Higher IPC = better CPU utilization
- Typical range: 0.5 - 2.0
- Low IPC indicates stalls or cache misses
- High IPC indicates good pipeline efficiency

### Cache Misses

**Definition**: Number of cache accesses that miss

**Interpretation**:
- Higher miss rate = memory access patterns
- Typical L1 miss rate: 1-5%
- Typical L2 miss rate: 5-20%
- Optimize data layout to reduce misses

### Context Switches

**Definition**: Number of times scheduler switches threads

**Interpretation**:
- Voluntary: Thread yields (I/O, locks)
- Involuntary: Preemption by scheduler
- High switches indicate contention
- Reduce by improving synchronization

## Best Practices

### 1. Profile in Release Mode

**Why**: Debug mode has different performance characteristics

**How**:
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### 2. Warm Up Before Profiling

**Why**: First run may have cache misses or initialization overhead

**How**:
```bash
# Warm-up run
./build/load_test --clients 5 --requests-per-client 10 --workload prime --prime-value 10000019

# Actual profiling
perf stat -p <server-pid> -- ./build/load_test --clients 25 --requests-per-client 100 --workload prime --prime-value 10000019
```

### 3. Multiple Runs

**Why**: Single runs can be affected by system noise

**How**:
```bash
# Run 3 times and average
for i in 1 2 3; do
  echo "Run $i"
  perf stat -p <server-pid> sleep 10
  sleep 2
done
```

### 4. Minimize Background Processes

**Why**: Other processes affect profiling results

**How**:
```bash
# Check running processes
ps aux | grep -v grep | grep -v threadforge

# Kill unnecessary processes
sudo killall firefox chromium
```

### 5. Use Consistent Environment

**Why**: Different environments produce different results

**How**:
- Use same hardware
- Use same OS/compiler
- Use Docker for reproducibility
- Document environment details

## Comparing Profiling Results

### Before and After Optimization

```bash
# Profile before optimization
echo "Before optimization:"
perf stat -p <server-pid> sleep 10

# Make optimization
# ...

# Profile after optimization
echo "After optimization:"
perf stat -p <server-pid> sleep 10

# Compare results
```

### Different Workloads

```bash
# Profile PRIME workload
echo "PRIME workload:"
perf stat -p <server-pid> -- ./build/load_test --workload prime --prime-value 10000019

# Profile SORT workload
echo "SORT workload:"
perf stat -p <server-pid> -- ./build/load_test --workload sort --sort-count 100000

# Compare results
```

## References

- [perf Documentation](https://perf.wiki.kernel.org/)
- [Valgrind Documentation](https://valgrind.org/)
- [GDB Documentation](https://sourceware.org/gdb/documentation/)
- [Flame Graphs](http://www.brendangregg.com/flamegraphs.html)
- [Linux Performance](http://www.brendangregg.com/linuxperf.html)
- [Error Handling Guide](error-handling.md)
- [Load Testing Guide](load-testing.md)
- [Troubleshooting Guide](troubleshooting.md)
