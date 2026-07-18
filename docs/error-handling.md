# Error Handling Guide

This document describes how ThreadForge handles errors at various levels and provides guidance for debugging connection issues.

## Error Response Format

All errors are returned in the following format:

```
ERROR <job_id> <message>
```

Where:
- `job_id`: The server-assigned job ID for the request
- `message`: A human-readable error message

## Protocol-Level Errors

These errors occur when the client sends malformed or invalid requests.

### Invalid Command

**Error Message**: `ERROR <job_id> Unknown command: <command>`

**Cause**: The command is not one of: PRIME, SORT, MATMUL, HASH

**Example**:
```
Client sends: INVALID 123
Server responds: ERROR 1 Unknown command: INVALID
```

**Resolution**: Use one of the supported commands: PRIME, SORT, MATMUL, or HASH

### Oversized Request

**Error Message**: `ERROR <job_id> Request exceeds maximum size (4096 bytes)`

**Cause**: The request line exceeds 4096 bytes

**Example**:
```
Client sends: HASH <4097 bytes of text>
Server responds: ERROR 1 Request exceeds maximum size (4096 bytes)
```

**Resolution**: Reduce the request size to under 4096 bytes

### Invalid Argument Format

**Error Message**: `ERROR <job_id> Invalid argument: <details>`

**Cause**: The argument cannot be parsed as the expected type

**Examples**:
```
Client sends: PRIME abc
Server responds: ERROR 1 Invalid argument: not a valid uint64

Client sends: SORT -100
Server responds: ERROR 1 Invalid argument: count must be positive

Client sends: MATMUL 1000
Server responds: ERROR 1 Invalid argument: size must be between 1 and 512
```

**Resolution**: Ensure arguments match the expected format and range

### Out-of-Range Arguments

**Error Message**: `ERROR <job_id> Argument out of range: <details>`

**Cause**: The argument value is outside the valid range

**Valid Ranges**:
- `PRIME <value>`: value must be a positive uint64
- `SORT <count>`: count must be between 1 and 2,000,000
- `MATMUL <size>`: size must be between 1 and 512
- `HASH <text>`: text must be non-empty

**Examples**:
```
Client sends: SORT 0
Server responds: ERROR 1 Argument out of range: count must be at least 1

Client sends: SORT 3000000
Server responds: ERROR 1 Argument out of range: count must not exceed 2000000

Client sends: MATMUL 0
Server responds: ERROR 1 Argument out of range: size must be at least 1

Client sends: MATMUL 1024
Server responds: ERROR 1 Argument out of range: size must not exceed 512

Client sends: HASH
Server responds: ERROR 1 Argument out of range: text must not be empty
```

**Resolution**: Adjust the argument to be within the valid range

## Connection-Level Errors

These errors occur at the TCP connection level.

### Connection Reset by Peer

**Symptom**: Connection closes unexpectedly without receiving a response

**Possible Causes**:
- Client disconnected abruptly
- Network connectivity issue
- Server crashed or restarted
- Firewall or proxy terminated the connection

**Resolution**:
1. Check network connectivity
2. Verify the server is running: `ps aux | grep threadforge_server`
3. Check firewall rules
4. Retry the connection

### Broken Pipe

**Symptom**: Client receives "Broken pipe" error when writing to socket

**Possible Causes**:
- Server closed the connection
- Network connectivity lost
- Server shutdown in progress

**Resolution**:
1. Verify the server is still running
2. Check network connectivity
3. Reconnect and retry

### Connection Timeout

**Symptom**: Client hangs waiting for a response

**Possible Causes**:
- Server is overloaded (queue is full)
- Server is processing a long-running job
- Network latency is high
- Server is unresponsive

**Resolution**:
1. Check server load: `pidstat -p <server-pid> 1`
2. Check queue depth in server logs
3. Increase client timeout if appropriate
4. Reduce client concurrency
5. Restart the server if unresponsive

### Connection Refused

**Symptom**: Client receives "Connection refused" error

**Possible Causes**:
- Server is not running
- Server is listening on a different port
- Firewall is blocking the connection
- Server crashed

**Resolution**:
1. Verify the server is running: `ps aux | grep threadforge_server`
2. Verify the correct port: `netstat -tlnp | grep threadforge_server`
3. Check firewall rules: `sudo ufw status`
4. Start the server if not running: `./build/threadforge_server 9000 4 1024`

## Job Execution Errors

These errors occur during job processing.

### Queue Overflow

**Error Message**: `ERROR <job_id> Queue is full, try again later`

**Cause**: The bounded queue has reached its maximum capacity

**Symptom**: Client is blocked trying to send a request

**Resolution**:
1. Reduce the number of concurrent clients
2. Reduce the request rate
3. Increase the queue capacity when starting the server: `./build/threadforge_server 9000 4 2048`
4. Increase the number of worker threads: `./build/threadforge_server 9000 8 1024`

### Resource Exhaustion

**Error Message**: `ERROR <job_id> Resource limit exceeded`

**Cause**: A job exceeded system resource limits

**Possible Causes**:
- SORT with very large count (2,000,000) on low-memory system
- MATMUL with large size (512x512) on low-memory system
- Many concurrent jobs exhausting memory

**Resolution**:
1. Reduce the job size/count
2. Reduce concurrent clients
3. Increase system memory
4. Increase worker thread count to distribute load

## Server-Level Errors

These errors occur at the server level.

### Server Shutdown

**Symptom**: All connections close, new connections are refused

**Cause**: Server received SIGTERM or SIGINT (Ctrl+C)

**Behavior**:
1. Server stops accepting new connections
2. Existing connections are closed gracefully
3. In-flight jobs are allowed to complete
4. Server prints final statistics and exits

**Resolution**:
1. Restart the server: `./build/threadforge_server 9000 4 1024`
2. Reconnect clients

### Server Crash

**Symptom**: Server exits unexpectedly, connections are dropped

**Possible Causes**:
- Segmentation fault (memory corruption)
- Unhandled exception
- Out of memory
- System signal

**Resolution**:
1. Check system logs: `dmesg | tail -20`
2. Run with ASAN to detect memory errors: `./build-asan/threadforge_server 9000 4 1024`
3. Run with TSAN to detect race conditions: `./build-tsan/threadforge_server 9000 4 1024`
4. Report the issue with reproduction steps

## Debugging Connection Issues

### Verify Server is Running

```bash
# Check if process is running
ps aux | grep threadforge_server

# Check if port is listening
netstat -tlnp | grep 9000

# Or using ss (newer systems)
ss -tlnp | grep 9000
```

### Test Basic Connectivity

```bash
# Test with nc (netcat)
nc -zv 127.0.0.1 9000

# Test with telnet
telnet 127.0.0.1 9000

# Send a simple command
printf 'HASH hello\n' | nc 127.0.0.1 9000
```

### Monitor Server Activity

```bash
# Monitor CPU and I/O
pidstat -p <server-pid> 1

# Monitor system calls
strace -p <server-pid>

# Monitor network connections
netstat -an | grep 9000

# Monitor with perf
perf stat -p <server-pid>
```

### Capture Network Traffic

```bash
# Capture traffic on port 9000
sudo tcpdump -i lo -n port 9000

# Capture with more detail
sudo tcpdump -i lo -n -A port 9000
```

### Enable Debug Logging

ThreadForge does not have built-in debug logging, but you can:

1. **Compile with debug symbols**:
   ```bash
   cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug
   cmake --build build-debug -j
   ```

2. **Run under debugger**:
   ```bash
   gdb ./build-debug/threadforge_server
   (gdb) run 9000 4 1024
   ```

3. **Run with sanitizers**:
   ```bash
   cmake -S . -B build-asan -DENABLE_ASAN=ON
   cmake --build build-asan -j
   ./build-asan/threadforge_server 9000 4 1024
   ```

## Common Error Scenarios

### Scenario 1: "Connection refused" on startup

**Problem**: Client cannot connect to server

**Diagnosis**:
```bash
# Check if server is running
ps aux | grep threadforge_server

# Check if port is in use
netstat -tlnp | grep 9000
```

**Solution**:
1. Start the server: `./build/threadforge_server 9000 4 1024`
2. Wait a moment for it to start
3. Retry the client connection

### Scenario 2: "Queue is full" errors

**Problem**: Clients receive queue overflow errors

**Diagnosis**:
```bash
# Monitor server load
pidstat -p <server-pid> 1

# Check number of connections
netstat -an | grep 9000 | wc -l
```

**Solution**:
1. Reduce client concurrency
2. Increase queue capacity: `./build/threadforge_server 9000 4 2048`
3. Increase worker threads: `./build/threadforge_server 9000 8 1024`

### Scenario 3: Slow responses

**Problem**: Responses are taking a long time

**Diagnosis**:
```bash
# Measure latency
./build/load_test --host 127.0.0.1 --port 9000 --clients 10 \
  --requests-per-client 100 --workload prime --prime-value 10000019

# Monitor CPU usage
pidstat -p <server-pid> 1
```

**Solution**:
1. Increase worker threads: `./build/threadforge_server 9000 8 1024`
2. Reduce job complexity (smaller SORT count, smaller MATMUL size)
3. Reduce client concurrency
4. Check system resources (CPU, memory)

### Scenario 4: Connection drops

**Problem**: Connections are being closed unexpectedly

**Diagnosis**:
```bash
# Check system logs
dmesg | tail -20

# Check for server crashes
ps aux | grep threadforge_server

# Monitor network
sudo tcpdump -i lo -n port 9000
```

**Solution**:
1. Check if server is still running
2. Look for segmentation faults in dmesg
3. Run with ASAN to detect memory errors
4. Reduce load on the server

## Error Recovery Strategies

### Client-Side Retry Logic

```cpp
int max_retries = 3;
int retry_delay_ms = 100;

for (int attempt = 0; attempt < max_retries; ++attempt) {
    try {
        // Send request
        send_request(socket, request);
        
        // Receive response
        std::string response = receive_response(socket);
        
        if (response.find("ERROR") == 0) {
            // Handle error
            if (response.find("Queue is full") != std::string::npos) {
                // Retry with backoff
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(retry_delay_ms * (attempt + 1)));
                continue;
            }
        }
        
        // Success
        return response;
    } catch (const std::exception& e) {
        if (attempt < max_retries - 1) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(retry_delay_ms * (attempt + 1)));
            continue;
        }
        throw;
    }
}
```

### Server-Side Graceful Degradation

When the queue is full:
1. Server rejects new requests with "Queue is full" error
2. Clients can retry with exponential backoff
3. As workers complete jobs, queue space becomes available
4. Clients can successfully submit new requests

## References

- [ThreadForge Protocol](../README.md#protocol-and-limits)
- [Shutdown Behavior](../README.md#shutdown)
- [Troubleshooting Guide](troubleshooting.md)
- [Platform Support](platform-support.md)
