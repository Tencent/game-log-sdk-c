# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

TGLog SDK is a high-performance C logging client library designed for efficient log transmission. It provides both multi-threaded and single-threaded versions, supporting 100,000 to 1,000,000 log records per second. The SDK follows the C99 standard and is built on libevent and ConcurrencyKit.

**Key Features:**
- Dual-version architecture: multi-threaded (tglog) for high-throughput servers, single-threaded (tglog-st) for game servers
- Network protocol support: TCP and UDP with automatic reconnection
- Lock-free design using ConcurrencyKit for multi-threaded version
- Language bindings: Python (ctypes, C extension, Cython)
- Performance: ~1.2M logs/sec (multi-threaded), ~100-200K logs/sec (single-threaded)

**Known Limitations:**
- Log messages cannot contain `\n` characters except at the end
- UDP mode limited to 65507 bytes per batch
- Protocol is request-only with no server acknowledgment

## Build System

### Primary Build Method: Makefile (Recommended)

```bash
# Build everything (dependencies + SDK + tests + bindings)
make

# Build specific components
make deps          # Build dependencies only (jemalloc, libevent, ck)
make tglog         # Build multi-threaded version
make tglog-st      # Build single-threaded version
make test          # Build test programs
make bindings      # Build Python bindings
make python        # Build Python bindings

# Build options
make RELEASE=1     # Optimized release build
make V=1           # Verbose output

# Utility commands
make check-deps    # Check dependency status
make clean         # Clean build artifacts
make clean-deps    # Clean dependencies
make install       # Install to dist/ directory
```

### Alternative: CMake

```bash
mkdir build && cd build
cmake ..                    # Debug build
cmake -DRELEASE=1 ..       # Release build
make -j4                    # Build all
make install                # Install to dist/
```

### Windows: CMake + MSVC

```bash
git clone --recurse-submodules <repo-url>
cd sdk-c
mkdir out && cd out
cmake ..
cmake --build . --config Release
cmake --install . --config Release
```

- Requires Visual Studio 2019+ (or Build Tools) with "Desktop development with C++" workload
- libevent is built via `add_subdirectory`, no manual setup needed
- Does not use ConcurrencyKit or jemalloc on Windows
- Builds both multi-threaded (tglog) and single-threaded (tglog-st) versions
- Multi-threaded version uses Windows native thread API instead of pthread
- Links `ws2_32` for Windows Sockets

### Dependency Management

The project uses git submodules for dependencies:
- **jemalloc** (5.3.0): High-performance memory allocator
- **libevent** (release-2.1.12-stable): Event notification library
- **ConcurrencyKit** (0.7.2): Lock-free data structures

Dependencies are automatically initialized via `make init-submodules` or downloaded via `./download-deps.sh` if not using git.

## Architecture

### Multi-threaded Version (tglog/)

**Core Components:**
- `client.c/h`: Main client controller, manages all components and provides API
- `worker.c/h`: Worker threads that handle log sending via event-driven architecture
- `discoverer.c/h`: DNS resolution and endpoint management (runs in separate thread)
- `conn.c/h`: Network connection management (TCP/UDP, reconnection, buffering)
- `endpoint.c/h`: Endpoint (server IP) management
- `request.c/h`: Log message request handling
- `metrics.c/h`: Performance statistics (uses atomic operations)
- `pool.c/h`: Memory pool for pre-allocated buffers
- `conf.c/h`: Configuration management
- `debug_logger.c/h`: Pluggable debug logging
- `err.c/h`: Error code definitions

**Key Design Patterns:**
- Lock-free message queue using ConcurrencyKit's `ck_ring_t` between client and workers
- Round-robin message distribution across worker threads
- Each worker maintains independent connection pool and event loop
- Atomic operations for lock-free statistics updates

### Single-threaded Version (tglog-st/)

**Architecture Differences:**
- No background threads - all processing in main thread via `tglog_client_update()`
- Event-driven using libevent's `event_base` (non-blocking I/O)
- Simplified batch buffer (single buffer vs. multi-level queues)
- DNS resolution via timer events instead of separate thread
- No atomic operations needed (single-threaded)

**Critical API Difference:**
- Multi-threaded: `tglog_client_send()` returns immediately, workers handle async; call `tglog_use_thread()` once at process startup
- Single-threaded: Must call `tglog_client_update()` periodically in main loop to process I/O

**`tglog_send()` Message Flag (memory ownership):**
- `TGLOG_MSG_F_NONE`: Caller owns payload; client does not copy or free it
- `TGLOG_MSG_F_FREE`: Client calls `free()` on payload after use
- `TGLOG_MSG_F_COPY`: Client copies the payload internally (safest, slight overhead)

**File Correspondence:**
The single-threaded version mirrors the multi-threaded structure with identical file names but simplified implementations (no threading, no atomic ops).

## Code Structure

```
sdk-c/
├── tglog/              # Multi-threaded version (~10K lines)
├── tglog-st/           # Single-threaded version (~10K lines)
├── test/               # Multi-threaded test programs
├── test-st/            # Single-threaded test programs
├── test-tapp/          # TAPP framework integration example
├── bindings/           # Language bindings
│   └── python/         # ctypes, C extension, Cython implementations
├── event/              # libevent submodule
├── ck/                 # ConcurrencyKit submodule
├── jemalloc/           # jemalloc submodule
├── thirdparty/         # Built dependencies (generated)
├── lib/                # Built libraries (generated)
└── dist/               # Installation directory (generated)
```

## Development Guidelines

### When Modifying Core Logic

1. **Maintain API compatibility** between tglog and tglog-st versions
2. **Update both versions** if changing core logic (client, conn, request, etc.)
3. **Preserve lock-free design** in multi-threaded version (use CK atomic ops)
4. **Keep single-threaded version simple** (no threads, no atomic ops)
5. **Test both versions** - they share similar logic but different execution models

### Memory Management

- TGLog libraries do NOT link jemalloc - final executables must link it
- Use memory pools (pool.c/h) for frequently allocated objects
- Pre-allocate buffers where possible to reduce malloc/free overhead
- Memory pool capacities differ: multi-threaded is 10x single-threaded defaults

### Error Handling

- Use error codes defined in `err.h`
- Callbacks provide error notification: `tglog_send_callback(payload, err_code, err_msg, user_data)`
- Debug logging is pluggable via `can_log` and `write_log` function pointers

### Configuration

Key configuration differences between versions:
- Multi-threaded: `worker_num` (default 8), uses `ck_ring` queues
- Single-threaded: `max_update_us` (default 200µs), `max_messages_per_update` (default 1000)
- Memory pool defaults: multi-threaded = 10x single-threaded

### Testing

```bash
# Multi-threaded tests
cd test
./tglog_test

# Single-threaded tests
cd test-st
./tglog_test_st
./simple_perf_test_st

# TAPP integration example
cd test-tapp
./start.sh
./stop.sh
```

### Language Bindings

**Python (bindings/python/):**
- Three implementations: ctypes (pure Python), C extension, Cython
- Cython has best performance (105% of C baseline)
- Compile: `make python` or `cd bindings && make python`

## Common Tasks

### Adding a New Configuration Option

1. Add field to `tglog_conf` struct in `tglog/conf.h` and `tglog-st/conf.h`
2. Update `tglog_conf_set_default()` in both versions
3. Update configuration documentation in README.md
4. If behavior differs between versions, document in configuration table

### Modifying Connection Logic

1. Edit `conn.c/h` in both tglog/ and tglog-st/
2. Ensure TCP/UDP protocol handling remains consistent
3. Test reconnection behavior (exponential backoff)
4. Verify connection lifecycle management (semi-persistent connections)

### Adding Metrics

1. Add metric field to `tglog_metrics` struct in `metrics.h`
2. Multi-threaded: Use atomic operations (`ck_pr_*`) for updates
3. Single-threaded: Use regular variable updates
4. Update `tglog_metrics_print()` to include new metric in JSON output
5. Update metrics documentation

### Debugging Build Issues

```bash
# Check dependency status
make check-deps

# Clean rebuild
make clean && make

# Clean rebuild including dependencies
make clean-deps && make

# Verbose build output
make V=1

# Check submodule status
git submodule status
```

## Protocol Format

TGLog uses a pipe-delimited format:
```
tglogname|field1=value1|field2=value2|...|fieldN=valueN\n
```

**Requirements:**
- Must end with `\n`
- Fields separated by `|`
- First field is log name
- Field values cannot contain `|` or `\n` (must be escaped)

**Example:**
```c
"userlogin|userid=12345|username=alice|timestamp=1640995200\n"
```

## Performance Considerations

### Multi-threaded Version
- Optimal worker count: CPU cores × 2 for high throughput, CPU cores for low latency
- Batch size: Larger batches (200 msgs, 100KB) for throughput, smaller (10 msgs, 5ms delay) for latency
- Uses lock-free queues - avoid blocking operations in callbacks

### Single-threaded Version
- Call `tglog_client_update()` frequently (every game tick, ~5ms)
- Configure `max_update_us` to limit single call duration
- Tune `max_messages_per_update` based on message rate
- Monitor update metrics: `max_update_time_us`, `timeout_update_count`

## Important Notes

- **Thread Safety**: Multi-threaded version is fully thread-safe; single-threaded version is NOT thread-safe
- **Callback Context**: Multi-threaded callbacks execute in worker threads; single-threaded callbacks execute in main thread during `update()`
- **jemalloc Linking**: Always link jemalloc in final executable, not in TGLog library
- **Submodule Versions**: Build system automatically checks out specific dependency versions
- **C99 Standard**: Code must be C99 compliant (no C11/C++ features)
