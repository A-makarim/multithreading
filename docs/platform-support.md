# Platform Support Matrix

This document describes the platforms, compilers, and configurations that ThreadForge supports or has been tested on.

## Tested Configurations

### Primary Test Environment

- **OS**: Ubuntu 24.04 LTS
- **Environment**: WSL2 on Windows
- **Compiler**: GCC 13.3.0
- **Build Type**: Release (-O2)
- **Hardware**: Intel Core i5-13500HX (10 cores / 20 logical CPUs)
- **Status**: ✅ Fully tested and verified

## Platform Support

### Linux

#### Distributions

| Distribution | Version | GCC | Clang | Status | Notes |
|---|---|---|---|---|---|
| Ubuntu | 22.04 LTS | 11+ | 14+ | ✅ Supported | Tested |
| Ubuntu | 24.04 LTS | 13+ | 16+ | ✅ Supported | Primary test platform |
| Debian | 12 (Bookworm) | 12+ | 15+ | ✅ Supported | Compatible |
| Fedora | 39+ | 13+ | 16+ | ✅ Supported | Compatible |
| RHEL/CentOS | 9+ | 11+ | 14+ | ✅ Supported | Compatible |
| Alpine | 3.18+ | 12+ | 15+ | ✅ Supported | Lightweight option |
| Arch Linux | Latest | 13+ | 16+ | ✅ Supported | Rolling release |

#### Compiler Support

| Compiler | Version | Status | Notes |
|---|---|---|---|
| GCC | 11.x | ✅ Supported | Minimum version |
| GCC | 12.x | ✅ Supported | Recommended |
| GCC | 13.x | ✅ Supported | Primary test compiler |
| GCC | 14.x | ✅ Supported | Latest |
| Clang | 14.x | ✅ Supported | Minimum version |
| Clang | 15.x | ✅ Supported | Recommended |
| Clang | 16.x | ✅ Supported | Latest |
| Clang | 17.x | ✅ Supported | Latest |

### WSL2 (Windows Subsystem for Linux)

| Aspect | Status | Notes |
|---|---|---|
| Build | ✅ Supported | Fully functional |
| Tests | ✅ Supported | All tests pass |
| ASAN/UBSAN | ✅ Supported | Fully functional |
| TSAN | ⚠️ Limited | Fails with "unexpected memory mapping" error |
| Benchmarks | ✅ Supported | Primary benchmark environment |

**Note**: TSAN on WSL2 has known limitations. For meaningful thread sanitizer checks, use a native Linux host or CI runner.

### macOS

| Aspect | Status | Notes |
|---|---|---|
| Build | ⚠️ Untested | Likely compatible with Clang |
| Tests | ⚠️ Untested | Expected to work |
| Sanitizers | ⚠️ Untested | Likely supported |
| Benchmarks | ⚠️ Untested | May have different performance characteristics |

**Note**: macOS support is not actively tested. Community contributions welcome.

### Windows (Native)

| Aspect | Status | Notes |
|---|---|---|
| Build | ❌ Not Supported | Requires POSIX APIs (sockets, pthreads) |
| Tests | ❌ Not Supported | POSIX-dependent |
| Sanitizers | ❌ Not Supported | Not applicable |

**Note**: ThreadForge uses POSIX APIs and is not compatible with native Windows. Use WSL2 instead.

### Docker

| Aspect | Status | Notes |
|---|---|---|
| Build | ✅ Supported | Works in Docker containers |
| Tests | ✅ Supported | All tests pass |
| Sanitizers | ✅ Supported | ASAN/UBSAN and TSAN work |
| Benchmarks | ✅ Supported | Suitable for reproducible benchmarks |

**Recommended Base Images**:
- `ubuntu:24.04` - Full-featured, primary test environment
- `ubuntu:22.04` - LTS, stable
- `alpine:3.18` - Lightweight option

## Build Requirements

### Minimum Requirements

- **CMake**: 3.15 or later
- **C++ Standard**: C++20
- **Compiler**: GCC 11+ or Clang 14+
- **Build Tools**: make or ninja

### Dependencies

#### Ubuntu/Debian

```bash
sudo apt-get install -y cmake build-essential
```

#### Fedora/RHEL

```bash
sudo dnf install -y cmake gcc-c++ make
```

#### Alpine

```bash
apk add --no-cache cmake g++ make
```

#### macOS (Homebrew)

```bash
brew install cmake
```

## Sanitizer Support

### ASAN/UBSAN (Address Sanitizer + Undefined Behavior Sanitizer)

| Platform | Status | Notes |
|---|---|---|
| Linux (native) | ✅ Supported | Fully functional |
| WSL2 | ✅ Supported | Fully functional |
| Docker | ✅ Supported | Fully functional |
| macOS | ⚠️ Untested | Likely supported |

**Build Command**:
```bash
cmake -S . -B build-asan -DENABLE_ASAN=ON
cmake --build build-asan -j
ctest --test-dir build-asan --output-on-failure
```

### TSAN (Thread Sanitizer)

| Platform | Status | Notes |
|---|---|---|
| Linux (native) | ✅ Supported | Fully functional |
| WSL2 | ❌ Not Supported | Memory mapping error |
| Docker | ✅ Supported | Fully functional |
| macOS | ⚠️ Untested | Likely supported |

**Build Command**:
```bash
cmake -S . -B build-tsan -DENABLE_TSAN=ON
cmake --build build-tsan -j
ctest --test-dir build-tsan --output-on-failure
```

**Note**: ASAN and TSAN cannot be enabled together.

## Build Types

| Build Type | Optimization | Debug Info | Use Case |
|---|---|---|---|
| Debug | -O0 | Yes | Development, debugging |
| Release | -O2 | No | Production, benchmarks |
| RelWithDebInfo | -O2 | Yes | Profiling, debugging optimized code |

**Build Command**:
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

## Performance Characteristics

### Measured Performance (Primary Environment)

- **OS**: Ubuntu 24.04 WSL2
- **Compiler**: GCC 13.3.0 -O2
- **Hardware**: Intel Core i5-13500HX

| Workers | Throughput (100 clients) | Mean Latency | p99 Latency |
|---|---|---|---|
| 1 | 2,403 req/s | 40.81 ms | 46.01 ms |
| 2 | 4,505 req/s | 21.62 ms | 35.15 ms |
| 4 | 8,491 req/s | 11.36 ms | 16.70 ms |
| 8 | 13,659 req/s | 6.88 ms | 9.98 ms |

**Note**: Performance varies significantly by hardware. These are single-run measurements on a specific machine.

## Known Limitations

### Platform-Specific

1. **WSL2 TSAN**: Thread Sanitizer fails with "unexpected memory mapping" error. Use native Linux for TSAN testing.
2. **macOS**: Not actively tested. May have compatibility issues.
3. **Windows**: Not supported. Use WSL2 instead.

### General

1. **One reader thread per client**: Does not scale like `epoll`. Suitable for moderate client counts.
2. **Text-only framing**: Newline-based protocol. Binary data requires length-prefixed framing.
3. **Single-threaded accept loop**: May become a bottleneck with very high connection rates.

## Continuous Integration

The project uses GitHub Actions for automated testing:

- **Compilers**: GCC and Clang
- **Build Types**: Debug and Release
- **Sanitizers**: ASAN/UBSAN and TSAN
- **Platforms**: Ubuntu latest

See `.github/workflows/ci.yml` for details.

## Support Policy

### Supported

- Latest stable versions of GCC and Clang
- Current and previous LTS versions of Ubuntu
- Recent versions of other major Linux distributions

### Best Effort

- Older compiler versions (GCC 11, Clang 14)
- Older Linux distributions
- macOS (untested but likely compatible)

### Not Supported

- Windows (native)
- Very old compilers (GCC < 11, Clang < 14)
- Exotic platforms

## Contributing

If you test ThreadForge on a platform not listed here, please open an issue or pull request to update this matrix. Community contributions are welcome!

## References

- [CMake Documentation](https://cmake.org/documentation/)
- [GCC Documentation](https://gcc.gnu.org/)
- [Clang Documentation](https://clang.llvm.org/)
- [AddressSanitizer](https://github.com/google/sanitizers/wiki/AddressSanitizer)
- [ThreadSanitizer](https://github.com/google/sanitizers/wiki/ThreadSanitizerCppManual)
