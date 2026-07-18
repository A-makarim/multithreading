# Troubleshooting Guide and FAQ

This document provides solutions to common issues and frequently asked questions about ThreadForge.

## Build Issues

### CMake not found

**Error**: `cmake: command not found`

**Solution**:

```bash
# Ubuntu/Debian
sudo apt-get install -y cmake

# Fedora/RHEL
sudo dnf install -y cmake

# Alpine
apk add --no-cache cmake

# macOS (Homebrew)
brew install cmake
```

Verify installation:
```bash
cmake --version
```

### Compiler not found

**Error**: `g++: command not found` or `clang: command not found`

**Solution**:

```bash
# Ubuntu/Debian (GCC)
sudo apt-get install -y build-essential

# Ubuntu/Debian (Clang)
sudo apt-get install -y clang

# Fedora/RHEL (GCC)
sudo dnf install -y gcc-c++ make

# Fedora/RHEL (Clang)
sudo dnf install -y clang

# Alpine
apk add --no-cache g++

# macOS
xcode-select --install
```

Verify installation:
```bash
g++ --version
# or
clang++ --version
```

### C++20 not supported

**Error**: `error: '-std=c++20' is not supported by this compiler`

**Cause**: Your compiler is too old

**Solution**: Upgrade your compiler

```bash
# Ubuntu/Debian - upgrade to newer version
sudo apt-get install -y g++-12  # or g++-13, g++-14

# Then use it explicitly
cmake -S . -B build -DCMAKE_CXX_COMPILER=g++-12
```

**Minimum Versions**:
- GCC: 11.x or later
- Clang: 14.x or later

### Build fails with linking errors

**Error**: `undefined reference to ...`

**Cause**: Missing dependencies or incomplete build

**Solution**:

```bash
# Clean and rebuild
rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### Out of memory during build

**Error**: `fatal error: Killed signal terminated program cc1plus`

**Cause**: Insufficient memory for parallel build

**Solution**:

```bash
# Build with fewer parallel jobs
cmake --build build -j 1

# Or limit to 2 jobs
cmake --build build -j 2
```

## Runtime Issues

### Server won't start

**Error**: Server process exits immediately

**Diagnosis**:

```bash
# Check if process is running
ps aux | grep threadforge_server

# Try running with output
./build/threadforge_server 9000 4 1024
```

**Common Causes and Solutions**:

1. **Port already in use**
   ```bash
   # Check what's using the port
   netstat -tlnp | grep 9000
   
   # Use a different port
   ./build/threadforge_server 9001 4 1024
   
   # Or kill the process using the port
   sudo kill -9 <PID>
   ```

2. **Permission denied**
   ```bash
   # Check file permissions
   ls -la ./build/threadforge_server
   
   # Make executable
   chmod +x ./build/threadforge_server
   ```

3. **Segmentation fault**
   ```bash
   # Run with ASAN to detect memory errors
   cmake -S . -B build-asan -DENABLE_ASAN=ON
   cmake --build build-asan -j
   ./build-asan/threadforge_server 9000 4 1024
   ```

### Port already in use

**Error**: `Address already in use`

**Solution**:

```bash
# Find what's using the port
sudo netstat -tlnp | grep 9000
# or
sudo ss -tlnp | grep 9000

# Kill the process
sudo kill -9 <PID>

# Or use a different port
./build/threadforge_server 9001 4 1024

# Wait for TIME_WAIT to expire (usually 60 seconds)
sleep 60
./build/threadforge_server 9000 4 1024
```

### Permission denied

**Error**: `Permission denied` when running server

**Solution**:

```bash
# Make the binary executable
chmod +x ./build/threadforge_server

# Or rebuild
cmake --build build -j
```

### Segmentation fault

**Error**: `Segmentation fault (core dumped)`

**Solution**:

1. **Run with ASAN to detect memory errors**:
   ```bash
   cmake -S . -B build-asan -DENABLE_ASAN=ON
   cmake --build build-asan -j
   ./build-asan/threadforge_server 9000 4 1024
   ```

2. **Run under debugger**:
   ```bash
   gdb ./build/threadforge_server
   (gdb) run 9000 4 1024
   (gdb) bt  # Print backtrace
   ```

3. **Check system logs**:
   ```bash
   dmesg | tail -20
   ```

## Connection Issues

### Cannot connect to server

**Error**: `Connection refused`

**Solution**:

```bash
# Verify server is running
ps aux | grep threadforge_server

# Check if port is listening
netstat -tlnp | grep 9000

# Start the server if not running
./build/threadforge_server 9000 4 1024

# Test connection
nc -zv 127.0.0.1 9000
```

### Connection timeout

**Error**: Client hangs waiting for response

**Solution**:

```bash
# Check server is responsive
printf 'HASH test\n' | nc 127.0.0.1 9000

# Monitor server load
pidstat -p <server-pid> 1

# Check queue depth
netstat -an | grep 9000 | wc -l

# Reduce client concurrency or increase workers
./build/threadforge_server 9000 8 1024
```

### Connection refused

**Error**: `Connection refused`

**Solution**: See "Cannot connect to server" above

### Broken pipe

**Error**: `Broken pipe` when writing to socket

**Solution**:

```bash
# Verify server is still running
ps aux | grep threadforge_server

# Check network connectivity
ping 127.0.0.1

# Restart the server
./build/threadforge_server 9000 4 1024

# Retry the client
```

## Performance Issues

### Slow responses

**Symptom**: Responses take a long time

**Diagnosis**:

```bash
# Measure latency
./build/load_test --host 127.0.0.1 --port 9000 --clients 10 \
  --requests-per-client 100 --workload prime --prime-value 10000019

# Monitor CPU usage
pidstat -p <server-pid> 1

# Check system resources
free -h
top
```

**Solutions**:

1. **Increase worker threads**:
   ```bash
   ./build/threadforge_server 9000 8 1024  # 8 workers instead of 4
   ```

2. **Reduce job complexity**:
   - Use smaller PRIME values
   - Use smaller SORT counts
   - Use smaller MATMUL sizes

3. **Reduce client concurrency**:
   ```bash
   ./build/load_test --clients 5 ...  # Reduce from 25 to 5
   ```

4. **Increase queue capacity**:
   ```bash
   ./build/threadforge_server 9000 4 2048  # 2048 instead of 1024
   ```

### High latency

**Symptom**: p99 latency is very high

**Solution**: See "Slow responses" above

### Low throughput

**Symptom**: Requests per second is lower than expected

**Solution**: See "Slow responses" above

### High CPU usage

**Symptom**: Server is using 100% CPU

**Diagnosis**:

```bash
# Check CPU usage
top -p <server-pid>

# Check context switches
pidstat -p <server-pid> 1

# Profile with perf
perf stat -p <server-pid>
```

**Solutions**:

1. **Reduce job complexity** (see above)
2. **Increase worker threads** (see above)
3. **Check for busy-waiting** (run with TSAN)

## Sanitizer Issues

### ASAN errors

**Error**: `ERROR: AddressSanitizer: ...`

**Solution**:

1. **Read the error message carefully** - it usually indicates the exact problem
2. **Run with debug symbols**:
   ```bash
   cmake -S . -B build-asan -DENABLE_ASAN=ON -DCMAKE_BUILD_TYPE=Debug
   cmake --build build-asan -j
   ./build-asan/threadforge_server 9000 4 1024
   ```

3. **Report the issue** with the full error output

### TSAN errors

**Error**: `WARNING: ThreadSanitizer: ...`

**Solution**:

1. **Read the error message** - it indicates the race condition
2. **Run with debug symbols**:
   ```bash
   cmake -S . -B build-tsan -DENABLE_TSAN=ON -DCMAKE_BUILD_TYPE=Debug
   cmake --build build-tsan -j
   ./build-tsan/threadforge_server 9000 4 1024
   ```

3. **Note**: TSAN doesn't work on WSL2. Use native Linux or Docker.

### Memory leaks

**Error**: `ERROR: LeakSanitizer: detected memory leaks`

**Solution**:

1. **Run with ASAN**:
   ```bash
   cmake -S . -B build-asan -DENABLE_ASAN=ON
   cmake --build build-asan -j
   ./build-asan/threadforge_server 9000 4 1024
   ```

2. **Suppress known leaks** (if any are acceptable)
3. **Report the issue** with the leak report

### Race conditions

**Error**: `WARNING: ThreadSanitizer: data race`

**Solution**:

1. **Run with TSAN on native Linux** (not WSL2):
   ```bash
   cmake -S . -B build-tsan -DENABLE_TSAN=ON
   cmake --build build-tsan -j
   ./build-tsan/threadforge_server 9000 4 1024
   ```

2. **Analyze the race condition** from the error output
3. **Report the issue** with reproduction steps

## WSL2-Specific Issues

### TSAN not working

**Error**: `ThreadSanitizer: unexpected memory mapping`

**Cause**: WSL2 doesn't support TSAN

**Solution**:

1. **Use native Linux** for TSAN testing
2. **Use Docker** with Linux container
3. **Use GitHub Actions** CI/CD
4. **Skip TSAN** on WSL2 and use ASAN instead

### Performance differences

**Symptom**: Performance is different from native Linux

**Cause**: WSL2 has different performance characteristics

**Solution**:

1. **Benchmark on native Linux** for accurate results
2. **Use Docker** for consistent environment
3. **Document WSL2-specific results** separately

### Network issues

**Symptom**: Network connectivity problems

**Solution**:

1. **Use localhost (127.0.0.1)** instead of hostname
2. **Check WSL2 network settings**
3. **Restart WSL2**: `wsl --shutdown`
4. **Use Docker** for consistent networking

## Frequently Asked Questions (FAQ)

### Q: What is ThreadForge?

**A**: ThreadForge is an educational C++20/Linux multithreaded TCP job server. It demonstrates production-style design considerations like explicit socket ownership, bounded back-pressure, and deterministic shutdown.

### Q: Is ThreadForge production-ready?

**A**: No. ThreadForge is designed for educational purposes. It is not recommended for production use.

### Q: What platforms does ThreadForge support?

**A**: ThreadForge supports Linux (native), WSL2, and Docker. See [platform-support.md](platform-support.md) for details.

### Q: Can I use ThreadForge on macOS?

**A**: ThreadForge has not been tested on macOS, but it may work. It uses POSIX APIs that are available on macOS.

### Q: Can I use ThreadForge on Windows?

**A**: ThreadForge requires POSIX APIs and is not compatible with native Windows. Use WSL2 instead.

### Q: How do I increase performance?

**A**: See "Performance Issues" section above. Key strategies:
1. Increase worker threads
2. Reduce job complexity
3. Reduce client concurrency
4. Increase queue capacity

### Q: How do I debug issues?

**A**: See "Debugging Connection Issues" in [error-handling.md](error-handling.md). Key tools:
1. Run with ASAN/UBSAN for memory errors
2. Run with TSAN for race conditions
3. Use gdb for interactive debugging
4. Use perf for profiling

### Q: What are the system requirements?

**A**: See [platform-support.md](platform-support.md). Minimum:
- CMake 3.15+
- C++20 compiler (GCC 11+ or Clang 14+)
- Linux or WSL2

### Q: How do I report a bug?

**A**: Open an issue on GitHub with:
1. Description of the problem
2. Steps to reproduce
3. Expected vs actual behavior
4. System information (OS, compiler, etc.)
5. Error messages or logs

### Q: How do I contribute?

**A**: Fork the repository, create a branch, make changes, and submit a pull request. See the README for more details.

### Q: What is the license?

**A**: Check the LICENSE file in the repository.

### Q: How do I run tests?

**A**: 
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

### Q: How do I run benchmarks?

**A**:
```bash
./build/load_test --host 127.0.0.1 --port 9000 --clients 25 \
  --requests-per-client 100 --workload prime --prime-value 32416190071
```

### Q: How do I profile the server?

**A**: See [profiling.md](profiling.md) for detailed profiling guide.

### Q: What are the protocol limits?

**A**: See [README.md](../README.md#protocol-and-limits) for protocol details:
- PRIME: positive uint64
- SORT: 1 to 2,000,000
- MATMUL: 1 to 512
- HASH: non-empty text
- Request size: max 4096 bytes

### Q: How does shutdown work?

**A**: See [README.md](../README.md#shutdown) for shutdown details. Press Ctrl+C to gracefully shutdown.

## Getting Help

If you can't find the answer here:

1. **Check the documentation**:
   - [README.md](../README.md)
   - [error-handling.md](error-handling.md)
   - [platform-support.md](platform-support.md)
   - [profiling.md](profiling.md)

2. **Search existing issues** on GitHub

3. **Open a new issue** with detailed information

4. **Check the code** - ThreadForge is designed to be readable

## References

- [ThreadForge README](../README.md)
- [Error Handling Guide](error-handling.md)
- [Platform Support Matrix](platform-support.md)
- [Profiling Guide](profiling.md)
