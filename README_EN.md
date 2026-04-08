# TGLog SDK - High-Performance C Logging Client

![C99](https://img.shields.io/badge/C-C99-blue.svg)
![Performance](https://img.shields.io/badge/Performance-1M_logs/sec-green.svg)
![Protocol](https://img.shields.io/badge/Protocol-TCP/UDP-orange.svg)
![License](https://img.shields.io/badge/License-MIT-blue.svg)

TGLog SDK is a high-performance C logging client library designed for efficient log transmission. It provides both multi-threaded and single-threaded versions, supporting transmission of 100,000 to 1,000,000 log records per second. The SDK follows the C99 standard, built on libevent and ConcurrencyKit, providing lock-free, high-concurrency log transmission capabilities, with Python language bindings.

> Note: For Java support, please use the pure Java version [SDK](https://git.woa.com/tglog/v3/sdk-java)

## 📝 Known Limitations
- Single log messages cannot contain `\n` characters except at the end. If log field values need to include `\n` or `|`, these 2 characters must be escaped
- UDP mode: single batch cannot exceed 65507 bytes
- Due to historical reasons, the protocol is request-only with no response. Send success only indicates data was written to network buffer successfully, not guaranteed server receipt. Has send failure retry mechanism but no application-layer acknowledgment and retransmission
- Single-threaded version may occasionally (about 5/100,000,000 probability) exceed 1ms per call for large logs due to inherent socket system call overhead, depending on socket buffer and system scheduling. Testing shows old TLog API has same issue for both TCP/UDP
- Single-threaded version: after all connections abnormally disconnect (extremely low probability) and recover, short-term increased I/O due to backlog processing may cause single calls to exceed 1ms. Can test by stopping server mid-run using `test-tapp` program


## ✨ Core Features

### 🚀 Dual-Version Architecture
- **Multi-threaded version (tglog)**: 1 million logs/sec, suitable for high-throughput servers
- **Single-threaded version (tglog-st)**: 100-200k logs/sec, suitable for game servers and single-threaded applications
- **API Compatible**: Both versions maintain the same API interface

### 🌐 Network Protocol Support
- **TCP Protocol**: Reliable transmission, batch max 512KB
- **UDP Protocol**: High-performance transmission, batch max 65507 bytes
- **Load Balancing**: Connection management for CLB scenarios
- **Auto-Reconnect**: Exponential backoff reconnection strategy

### 📊 Monitoring & Debugging
- **Real-time Metrics**: Rich performance statistics
- **Pluggable Logging**: Support for custom debug log functions
- **JSON Output**: Easy integration with monitoring systems
- **Multi-dimensional Statistics**: Global and Worker-level metrics

### 🔌 Multi-Language Support
- **C/C++**: Native API
- **Python**: Three implementations: ctypes, C extension, and Cython

### ⚙️ Flexible Configuration
- **Tunable Parameters**: Batch size, timeout, retry count, etc.
- **Connection Management**: Semi-persistent connections with lifecycle configuration
- **DNS Resolution**: Periodic updates supporting domain changes
- **Memory Optimization**: Pre-allocated buffers with jemalloc support

## 📋 System Requirements

### Dependencies
- **libevent**: High-performance event library
- **ConcurrencyKit (ck)**: Lock-free data structure library
- **jemalloc** (recommended): High-performance memory allocator
- **pthread**: POSIX thread support

> **Important Note**: TGLog library itself does not link jemalloc; it should be linked by the final executable or language binding. This avoids version conflicts and allows users to freely choose memory allocators.

### Build Environment
- **C Compiler**: C99 standard support (GCC 4.9+, Clang 3.9+, MSVC 2019+)
- **Operating System**: Linux, Windows
- **Architecture**: x86_64

> Note: Windows platform uses CMake + MSVC for building. It does not depend on ConcurrencyKit or jemalloc. See [Windows Build](#method-3-windows-with-cmake--msvc) section below.

### Language Binding Requirements
- **Python**: Python 3.6 or higher

## 🚀 Quick Start

### Build and Installation

The project supports two build methods: **Makefile** and **CMake**.

#### Method 1: Using Makefile (Recommended)

```bash
# Clone repository (recommended: auto-initialize submodules)
git clone --recurse-submodules https://git.woa.com/tglog/v3/sdk-c.git
cd sdk-c

# Or, if already cloned without submodule initialization
git clone https://git.woa.com/tglog/v3/sdk-c.git
cd sdk-c
git submodule update --init --recursive

# If downloaded as ZIP archive (not git clone)
# Can directly make, will auto-download dependencies
# Or manually download: ./download-deps.sh

# View available build targets
make help

# Build all components (dependencies + TGLog SDK + test programs + language bindings)
make

# Or build step by step
make deps          # Build dependencies only (jemalloc, libevent, ck)
make tglog         # Build multi-threaded version only
make tglog-st      # Build single-threaded version only
make test          # Build test programs
make bindings      # Build Python bindings
make python        # Build Python bindings

# Check dependency status
make check-deps

# Install to dist directory
make install
```

#### Method 2: Using CMake

```bash
# Create build directory
mkdir build
cd build

# Configure project (Debug version)
cmake ..

# Or configure as Release version
cmake -DRELEASE=1 ..

# Build all components at once (recommended, consistent with Makefile)
make -j4

# This automatically builds:
# - Dependencies (jemalloc, libevent, ConcurrencyKit)
# - Core libraries (tglog and tglog-st)
# - Test programs (multi-threaded and single-threaded versions)
# - TAPP integration examples
# - Language bindings (Python)

# Or build step by step
make deps -j4                          # Build dependencies only
make tglog tglog-st -j4               # Build core libraries only
make test_programs test_st_programs -j4  # Build test programs only
make bindings -j4                      # Build language bindings only

# View all available targets
make help_info

# Install to dist directory
make install
```

**CMake Notes**:
- Running `make` automatically builds **all components**, including dependencies, core libraries, test programs, and language bindings
- Recommended to use separate build directory (out-of-source build)
- CMake generates `compile_commands.json` for IDE integration (CLion, VSCode, etc.)
- Build behavior fully consistent with Makefile

#### Method 3: Windows with CMake + MSVC

Windows platform uses CMake as the build entry point, supporting Visual Studio 2019 and above.

**Prerequisites**:
- Visual Studio 2019+ or Visual Studio Build Tools (with "Desktop development with C++" workload)
- CMake 3.10+ (included with Visual Studio or installed separately)
- Git (for initializing submodules)

```bash
# Clone repository and initialize submodules
git clone --recurse-submodules https://git.woa.com/tglog/v3/sdk-c.git
cd sdk-c

# Generate Visual Studio project with CMake
mkdir out
cd out
cmake ..

# Command-line build (Debug)
cmake --build . --config Debug

# Command-line build (Release)
cmake --build . --config Release

# Install to dist directory
cmake --install . --config Release
```

**Using Visual Studio IDE**:
```bash
# After generating VS solution, open it directly
cmake ..
start sdk_c.sln
```

Or use Visual Studio's "Open Folder" feature to open the project root directory; VS will automatically detect `CMakeLists.txt`.

**Windows Build Notes**:
- libevent is built via `add_subdirectory` on Windows, no manual installation required
- Does not depend on ConcurrencyKit or jemalloc; uses the system default memory allocator
- Build artifacts include both multi-threaded (tglog) and single-threaded (tglog-st) static and dynamic libraries
- Multi-threaded version uses Windows native thread API instead of pthread on Windows
- Requires linking `ws2_32` (Windows Sockets) system library

### Basic C Usage

#### Multi-threaded Version (tglog)

```c
#include "tglog/tglog.h"

// Process-level configuration, important: only call once per process
tglog_use_thread();

// Initialize configuration
tglog_conf *conf = tglog_conf_create(
    TGLOG_NETWORK_TCP,      // Protocol type
    "your.server.com",      // Server address
    8080,                   // Server port
    NULL,                   // Use default log filter
    NULL,                   // Use default log output
    NULL                    // User data
);

// Create client
tglog_client *client = tglog_client_create(conf);
tglog_conf_destroy(conf);  // Client copies config, conf can be freed

// Send log callback function
void log_callback(const char *log_payload, int err_code,
                  const char *err_msg, void *user_data) {
    if (err_code == TGLOG_ERR_OK) {
        printf("Send success: %s", log_payload);
    } else {
        printf("Send failed: %s (error code: %d)\n", err_msg, err_code);
    }
}

// Send log (must end with \n)
const char *log = "userlogin|userid=12345|username=alice|timestamp=1640995200\n";
tglog_client_send(client, log, log_callback, NULL, TGLOG_MSG_F_COPY);

// Cleanup
tglog_client_destroy(client);
```

#### Single-threaded Version (tglog-st)

```c
#include "tglog-st/tglog.h"

// Initialize configuration
tglog_conf *conf = tglog_conf_create(
    TGLOG_NETWORK_TCP,
    "your.server.com",
    8080,
    NULL, NULL, NULL
);

// Create client
tglog_client *client = tglog_client_create(conf);
tglog_conf_destroy(conf);

// Game main loop
while (game_running) {
    // Process game logic
    process_game_logic();

    // Send log
    const char *log = "battle|winner=1|loser=2|score=100\n";
    tglog_client_send(client, log, NULL, NULL, TGLOG_MSG_F_COPY);

    // Must periodically call update to process network I/O
    tglog_client_update(client);
}

// Cleanup
tglog_client_destroy(client);
```

#### Compilation and Linking

When using TGLog library, link the following libraries:

```bash
# Multi-threaded version
gcc your_program.c -o your_program \
    -I/path/to/sdk-c/dist/include \
    -L/path/to/sdk-c/dist/lib \
    -L/path/to/sdk-c/thirdparty/jemalloc/lib \
    -ltglog -levent -levent_pthreads -lck -ljemalloc -lpthread -lm

# Single-threaded version
gcc your_program.c -o your_program \
    -I/path/to/sdk-c/dist/include \
    -L/path/to/sdk-c/dist/lib \
    -L/path/to/sdk-c/thirdparty/jemalloc/lib \
    -ltglog-st -levent -lck -ljemalloc -lpthread -lm
```

> **Note**: jemalloc must be linked by the final executable, not by TGLog library. This avoids version conflicts.

**Windows (MSVC) Linking**:

```bash
# Multi-threaded version
cl your_program.c /I path\to\sdk-c\dist\include ^
    /link path\to\sdk-c\dist\lib\tglog.lib ^
    path\to\sdk-c\thirdparty\event\lib\event_core.lib ^
    path\to\sdk-c\thirdparty\event\lib\event_pthreads.lib ^
    ws2_32.lib

# Single-threaded version
cl your_program.c /I path\to\sdk-c\dist\include ^
    /link path\to\sdk-c\dist\lib\tglog-st.lib ^
    path\to\sdk-c\thirdparty\event\lib\event_core.lib ^
    ws2_32.lib
```

> **Note**: On Windows, jemalloc and pthread are not needed; the system default memory allocator and thread library are used.

### Python Usage Example

```python
# Example 1: ctypes version (pure Python, no compilation needed, suitable for rapid development)
from tglog_client import TglogClientPy

client = TglogClientPy(protocol="tcp", host="dev.tglog.com", port=20001, workers=8)

def callback(payload, err_code, err_msg):
    if err_code == 0:
        print(f"Success: {payload}")
    else:
        print(f"Error: {err_msg}")

client.send("mylog|field1|field2\n", callback=callback)
client.destroy()

# Example 2: C Extension version (high performance, requires compilation)
import tglog_ext

client = tglog_ext.TglogClient("tcp", "dev.tglog.com", 20001)
client.send("mylog|field1|field2\n", use_callback=True)

# Example 3: Cython version (highest performance, recommended for production)
from tglog_cython import TglogClient

client = TglogClient(protocol="tcp", host="dev.tglog.com", port=20001, workers=4)

def callback(payload, err_code, err_msg):
    if err_code == 0:
        print(f"Success: {payload}")

# High-performance send (~1,283,890 msg/s)
for i in range(10000):
    client.send(f"mylog|data|{i}\n", callback)

# Get statistics
metrics = client.get_metrics()
print(f"Statistics: {metrics}")

client.destroy()
```

## 📖 Detailed Configuration

### tglog_conf Configuration Options

> **Version Differences**:
> - Multi-threaded version (tglog) has `worker_num` config, single-threaded version (tglog-st) does not
> - Single-threaded version (tglog-st) has `max_update_us` and `max_messages_per_update` config, multi-threaded version (tglog) does not
> - Memory pool default values differ: multi-threaded version is 10x single-threaded version
> - Multi-threaded version callback functions (`can_log`, `write_log`) must be thread-safe

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| **Basic Config** | | | |
| `network` | enum | TCP | Network protocol (TCP/UDP) |
| `host` | string | - | Server address (IP or domain) |
| `port` | int | - | Server port |
| **Connection Config** | | | |
| `update_interval` | int | 60s | DNS update interval |
| `conn_timeout` | int | 3000ms | Connection timeout (TCP only) |
| `max_conn_lifetime` | int | 300s | Connection lifetime |
| **Buffer Config** | | | |
| `write_high_watermark` | int | 32MB | Write buffer high watermark |
| `socket_send_buffer_size` | int | 64MB | Socket send buffer size |
| `socket_recv_buffer_size` | int | 16KB | Socket receive buffer size |
| **Batching Config** | | | |
| `batching_max_publish_delay` | int | 10ms | Batch max delay |
| `batching_max_messages` | int | 50 | Batch max messages |
| `batching_max_size` | int | 32KB | Batch max size |
| **Queue and Timeout Config** | | | |
| `max_pending_messages` | int | 409600 (multi-threaded)<br>1000000 (single-threaded) | Max pending messages per worker |
| `send_timeout` | int | 30000ms | Send timeout |
| `max_retries` | int | 3 | Max retry count |
| `stop_timeout` | int | 3000ms | Stop timeout |
| **Thread Config** | | | |
| `worker_num` | int | 8 | Worker thread count (multi-threaded only) |
| **Single-threaded Specific Config** | | | |
| `max_update_us` | int | 200us | Max time per update call (single-threaded only) |
| `max_messages_per_update` | int | 1000 | Max messages per update (single-threaded only) |
| **Memory Pool Config** | | | |
| `message_pool_cap` | size_t | 1000000 (multi-threaded)<br>100000 (single-threaded) | Message pool capacity |
| `payload_pool_256b_cap` | size_t | 20000 (multi-threaded)<br>2000 (single-threaded) | 256-byte payload pool capacity |
| `payload_pool_512b_cap` | size_t | 20000 (multi-threaded)<br>2000 (single-threaded) | 512-byte payload pool capacity |
| `payload_pool_1kb_cap` | size_t | 20000 (multi-threaded)<br>2000 (single-threaded) | 1KB payload pool capacity |
| `payload_pool_2kb_cap` | size_t | 10000 (multi-threaded)<br>1000 (single-threaded) | 2KB payload pool capacity |
| `payload_pool_4kb_cap` | size_t | 10000 (multi-threaded)<br>1000 (single-threaded) | 4KB payload pool capacity |
| `payload_pool_8kb_cap` | size_t | 10000 (multi-threaded)<br>1000 (single-threaded) | 8KB payload pool capacity |
| `payload_pool_16kb_cap` | size_t | 5000 (multi-threaded)<br>500 (single-threaded) | 16KB payload pool capacity |
| `payload_pool_32kb_cap` | size_t | 5000 (multi-threaded)<br>500 (single-threaded) | 32KB payload pool capacity |
| `payload_pool_48kb_cap` | size_t | 1000 (multi-threaded)<br>100 (single-threaded) | 48KB payload pool capacity |
| `payload_pool_64kb_cap` | size_t | 1000 (multi-threaded)<br>100 (single-threaded) | 64KB payload pool capacity |
| **Debug Log Config** | | | |
| `can_log` | func | tglog_can_log_info | Log filter function |
| `write_log` | func | tglog_std_write_log | Log write function |
| `user_data` | void* | NULL | User data |

### Advanced Configuration Example

```c
tglog_conf *conf = tglog_conf_create(TGLOG_NETWORK_TCP, "logs.example.com", 8080, NULL, NULL, NULL);

// Performance optimization config
conf->worker_num = 16;                      // Increase worker threads (multi-threaded)
conf->batching_max_messages = 200;          // Increase batch size
conf->batching_max_size = 80 * 1024;        // Increase batch bytes
conf->batching_max_publish_delay = 5;       // Reduce delay

// Network optimization config
conf->socket_send_buffer_size = 32 * 1024 * 1024;  // 32MB send buffer
conf->write_high_watermark = 64 * 1024 * 1024;     // 64MB write buffer high watermark
conf->max_conn_lifetime = 600;              // 10-minute connection lifetime

// Reliability config
conf->max_retries = 5;                      // Increase retry count
conf->send_timeout = 60000;                 // 60-second send timeout
```

## 📊 Performance Monitoring

### Get Statistics

```c
// Print performance metrics (JSON format)
char buffer[8192];
int len = tglog_client_print_metrics(client, buffer, sizeof(buffer), true);
if (len > 0) {
    printf("Performance metrics:\n%s\n", buffer);
}

// Print memory pool statistics
len = tglog_client_print_mem_pool_stats(client, buffer, sizeof(buffer), true);
if (len > 0) {
    printf("Memory pool statistics:\n%s\n", buffer);
}
```

### Metrics Description

#### Counter Type (Cumulative Count)

**Message Statistics**
- `request_msg_count`: Total messages requested by application
- `enqueued_msg_count`: Successfully enqueued messages
- `dequeued_msg_count`: Successfully dequeued messages
- `succeeded_msg_count`: Successfully sent messages
- `failed_msg_count`: Failed messages (including timeout and dropped)
- `timeout_msg_count`: Timed out messages
- `retry_msg_count`: Retry count (each retry counted)
- `dropped_msg_count`: Dropped messages (requested but not enqueued)

**Error Statistics**
- `eagain_err_count`: EAGAIN error count

**Connection Statistics**
- `opened_conn_count`: Opened connection count
- `closed_conn_count`: Closed connection count
- `error_conn_count`: Error connection count

**Endpoint Statistics**
- `add_endpoint_count`: Endpoint add count
- `remove_endpoint_count`: Endpoint remove count

#### Gauge Type (Instantaneous State)
- `endpoint_count`: Current endpoint count
- `msg_queue_size`: Current message queue size
- `msg_queue_capacity`: Message queue capacity

#### Single-threaded Version Specific Metrics (Update Performance Monitoring)

**Update Call Statistics**
- `total_update_count`: Total `tglog_client_update()` call count
- `timeout_update_count`: Timed out update call count
- `idle_update_count`: Idle update call count (no messages dispatched)

**Update Performance Metrics**
- `max_update_time_us`: Max time per update call (microseconds)
- `limit_update_time_us`: Configured update call time limit (microseconds)
- `max_event_dispatch_time_us`: Max event dispatch time per update (microseconds)
- `max_msg_dispatch_time_us`: Max message dispatch time per update (microseconds)
- `max_msg_dispatch_count`: Max messages dispatched per update

> **Note**: Single-threaded version requires application to actively call `tglog_client_update()` to drive event loop. These metrics monitor update call performance.

## 🏗️ Architecture Design

### Multi-threaded Version Architecture

```
Application
     │
     ▼
┌─────────────────┐
│   tglog_client   │ ◄─── Main entry, API interface
└─────────────────┘
     │
     ├── tglog_discoverer ◄─── DNS resolution, endpoint discovery
     │
     ├── tglog_worker[] ◄─── Multiple worker threads
     │    │
     │    ├── ck_ring_t ◄─── Lock-free message queue
     │    ├── tglog_conn_list ◄─── Connection pool
     │    └── event_base ◄─── Event loop
     │
     ├── tglog_metrics ◄─── Performance statistics
     │
     └── tglog_debug_logger ◄─── Debug logging
```

### Single-threaded Version Architecture

```
Application (Game Server)
     │
     ▼ (calls tglog_client_update)
     │
┌─────────────────┐
│   tglog_client   │ ◄─── Main entry, API interface
└─────────────────┘
     │
     ├── event_base ◄─── Internal event loop (self-contained)
     │
     ├── tglog_discoverer ◄─── DNS resolution, endpoint discovery
     │
     ├── tglog_worker ◄─── Single worker thread (integrated in update)
     │    │
     │    ├── batch_buffer ◄─── Batch buffer
     │    └── tglog_conn_list ◄─── Connection pool
     │
     ├── tglog_metrics ◄─── Performance statistics
     │
     └── tglog_debug_logger ◄─── Debug logging
```

## 📄 TGLog Protocol Format

### Standard Format
```
tglogname|field1=value1|field2=value2|...|fieldN=valueN\n
```

### Examples
```c
// User login log
"userlogin|userid=12345|username=alice|timestamp=1640995200|result=success\n"

// Order processing log
"order|orderid=ORDER123|userid=12345|amount=99.99|status=completed\n"

// Error log
"error|module=payment|errorcode=500|message=database timeout|timestamp=1640995200\n"
```

### Format Requirements
- Must end with newline `\n`
- Use `|` as field separator
- First field is log name
- Field values cannot contain `|` and `\n` characters

## 🔧 Build Options

### Makefile Targets

```bash
# Main build targets
make all           # Build all components
make deps          # Build dependencies only
make tglog         # Build multi-threaded version only
make tglog-st      # Build single-threaded version only
make test          # Build test programs
make test-tapp     # Build TAPP integration example
make bindings      # Build Python bindings
make python        # Build Python bindings

# Clean targets
make clean         # Clean build artifacts
make clean-deps    # Clean dependencies
make clean-bindings # Clean language bindings

# Other targets
make check-deps    # Check dependency status
make install       # Install to dist directory
make uninstall     # Remove dist directory
make help          # Show help information
```

### Build Options

```bash
# Enable optimization build
make RELEASE=1

# Debug build
make

# Show verbose build information
make V=1
```

## 🌍 Language Bindings

### Python Binding

TGLog provides three Python binding implementations:

1. **Cython extension** ⭐⭐⭐⭐⭐: Highest performance, **exceeds C baseline by 5%**, recommended for production
2. **C extension** ⭐⭐⭐⭐: High performance, reaches 78% of C baseline, suitable for production
3. **ctypes binding** ❌: Pure Python implementation, no compilation needed, only 3% performance, not recommended for production

**Performance Comparison** (sending 1,000,000 logs):

| Implementation | Send Time | Throughput | Relative Performance | Rating |
|----------------|-----------|------------|---------------------|--------|
| **Python Cython** 🏆 | 776.7ms | **1,283,890 msg/s** | **105%** | ⭐⭐⭐⭐⭐ |
| **C Baseline** | 813.6ms | 1,225,684 msg/s | 100% | ⭐⭐⭐⭐⭐ |
| **Python C Extension** | 1,049.7ms | 950,718 msg/s | 78% | ⭐⭐⭐⭐ |
| **Python ctypes** | 25,887.2ms | 38,615 msg/s | 3% | ❌ |

**Documentation**: See [bindings/python/README.md](bindings/python/README.md)

## 🎮 Integration Examples

### TAPP Framework Integration

TGLog SDK provides integration example with Tencent TAPP framework, demonstrating single-threaded version usage in game servers.

**Example Location**: `test-tapp/`

**Key Features**:
- Integrated into TAPP tick callback (called every 5ms)
- Uses TAPP logging system for debug output
- Supports dynamic configuration via config file
- Real-time performance metrics monitoring

**Run Example**:
```bash
cd test-tapp
./start.sh    # Start TAPP application
./stop.sh     # Stop TAPP application
```

## 🔍 Troubleshooting

### Common Issues

#### Build Issues
```bash
# Clean and rebuild
make clean && make

# Check dependency library status
make check-deps

# View detailed build information
make V=1

# Complete rebuild (including dependencies)
make clean-deps && make
```

#### Runtime Issues
- **Connection failure**: Check server address and port configuration
- **Poor performance**: Adjust worker thread count and batch parameters
- **High memory usage**: Check queue capacity and connection count configuration
- **Log loss**: Check error callbacks, increase retry count

#### Debugging Tips
```c
// Enable verbose debug logging (show all level logs)
tglog_conf_set_debug_logger(conf, tglog_can_log_debug, tglog_std_write_log, NULL);
```

### Performance Tuning

#### High Throughput Scenario
```c
conf->worker_num = CPU_cores * 2;
conf->batching_max_messages = 200;
conf->batching_max_size = 100 * 1024;
conf->batching_max_publish_delay = 20;
```

#### Low Latency Scenario
```c
conf->worker_num = CPU_cores;
conf->batching_max_messages = 10;
conf->batching_max_publish_delay = 5;
conf->max_pending_messages = 1024;
```

## 🆚 Version Comparison

| Feature | Multi-threaded (tglog) | Single-threaded (tglog-st) |
|---------|------------------------|---------------------------|
| Thread Model | Multiple worker threads, auto-processing | Single thread, requires update call |
| Performance Target | 1 million logs/sec | 100-200k logs/sec |
| CPU Overhead | Medium (multi-thread overhead) | Low (no thread switching) |
| Latency | Low (background thread processing) | Depends on update frequency |
| Memory Usage | Medium (multi-thread) | Low (single-thread) |
| Integration Complexity | Create and use | Requires main loop integration |
| Use Case | High throughput, multi-threaded apps | Game servers, single-threaded apps |
| Thread Safety | Fully thread-safe | Not needed (single-thread) |
| Platform Support | Linux, **Windows** | Linux, **Windows** |

## 📚 API Reference

### Core Functions

#### tglog_client_create
```c
tglog_client *tglog_client_create(const tglog_conf *conf);
```
Create TGLog client instance.

#### tglog_client_send
```c
int tglog_client_send(tglog_client *client, const char *log_payload,
                      tglog_send_callback callback, void *user_data, char msg_flags);
```
Send log message with async callback notification.

**msg_flags parameter description**:
- `TGLOG_MSG_F_NONE`: Zero-copy mode, client neither copies nor frees message
- `TGLOG_MSG_F_FREE`: Client auto-frees message after send (must be malloc allocated)
- `TGLOG_MSG_F_COPY`: Client auto-copies message (recommended)

#### tglog_client_update (single-threaded only)
```c
int tglog_client_update(tglog_client *client);
```
Update client state, process network I/O, batch sending, DNS updates, etc. **Must be called periodically in main loop**.

#### tglog_client_print_metrics
```c
int tglog_client_print_metrics(tglog_client *client, char *buffer, size_t size, bool json);
```
Print client performance statistics to buffer.

#### tglog_client_destroy
```c
void tglog_client_destroy(tglog_client *client);
```
Destroy client instance, release all resources.

### Configuration Functions

#### tglog_conf_create
```c
tglog_conf *tglog_conf_create(tglog_network network, const char *host,
                              int port, can_log_func can_log,
                              write_log_func write_log, void *user_data);
```
Create configuration instance.

#### tglog_conf_set_default
```c
void tglog_conf_set_default(tglog_conf *conf);
```
Set default configuration values.

## 📂 Project Structure

```
sdk-c/
├── tglog/                  # Multi-threaded version source code
│   ├── client.c/h         # Client main logic
│   ├── worker.c/h         # Worker threads
│   ├── conn.c/h           # Connection management
│   ├── discoverer.c/h     # DNS resolution
│   ├── request.c/h        # Request processing
│   ├── endpoint.c/h       # Endpoint management
│   ├── metrics.c/h        # Statistics
│   ├── README.md          # Multi-threaded version docs
│   └── DESIGN.md          # Design docs
│
├── tglog-st/              # Single-threaded version source code
│   ├── client.c/h         # Client main logic
│   ├── worker.c/h         # Worker logic
│   ├── conn.c/h           # Connection management
│   ├── discoverer.c/h     # DNS resolution
│   ├── request.c/h        # Request processing
│   ├── endpoint.c/h       # Endpoint management
│   ├── metrics.c/h        # Statistics
│   ├── README.md          # Single-threaded version docs
│   └── DESIGN.md          # Design docs
│
├── bindings/              # Language bindings
│   └── python/            # Python bindings
│       ├── tglog_client.py      # ctypes binding
│       ├── tglog_ext.c          # C extension
│       ├── tglog_cython.pyx     # Cython extension
│       └── README.md
│
├── test/                  # Multi-threaded version test programs
├── test-st/               # Single-threaded version test programs
├── test-tapp/             # TAPP framework integration example
│   ├── tglog_tapp.c
│   ├── start.sh
│   └── stop.sh
│
├── event/                 # libevent source code
├── ck/                    # ConcurrencyKit source code
├── jemalloc/              # jemalloc source code
│
├── lib/                   # Compiled library files
│   ├── libtglog.a         # Multi-threaded static library
│   ├── libtglog.so        # Multi-threaded dynamic library
│   ├── libtglog-st.a      # Single-threaded static library
│   └── libtglog-st.so     # Single-threaded dynamic library
│
├── Makefile               # Main Makefile
└── README.md              # This document
```

## 🤝 Contributing

### Development Environment
1. Install C99-compatible compiler
2. Install dependencies (libevent, ck)
3. Clone repository and build

### Code Standards
- Follow C99 standard
- Functions and variables prefixed with `tglog_`
- Focus on memory safety and error handling
- Add unit tests

### Submit Code
1. Fork project
2. Create feature branch
3. Submit code with tests
4. Create Pull Request

## 📄 License

This project is open-sourced under the MIT License.

## 🆘 Support

- **Documentation**:
  - Multi-threaded version: [tglog/README.md](tglog/README.md) and [tglog/DESIGN.md](tglog/DESIGN.md)
  - Single-threaded version: [tglog-st/README.md](tglog-st/README.md) and [tglog-st/DESIGN.md](tglog-st/DESIGN.md)
  - Python binding: [bindings/python/README.md](bindings/python/README.md)
  - CMake build guide: [CMAKE_BUILD_GUIDE.md](CMAKE_BUILD_GUIDE.md)
  - Git Submodules guide: [GIT_SUBMODULES_FAQ.md](GIT_SUBMODULES_FAQ.md)
- **Examples**: Refer to example code in `test/`, `test-st/`, and `test-tapp/` directories
- **Issue Reporting**: Submit issues via GitHub Issues
- **Technical Discussion**: Welcome to participate in code review and technical discussion

## 🎯 Performance Benchmarks

### C Language Performance (sending 1,000,000 logs)

| Version | Send Time | Throughput | Latency | Use Case |
|---------|-----------|------------|---------|----------|
| Multi-threaded (8 workers) | ~813ms | ~1,225,684 msg/s | <1ms | High-throughput servers |
| Single-threaded | ~685ms | ~1,459,854 msg/s | <1ms | Game servers, single-threaded apps |

### Language Binding Performance (sending 1,000,000 logs)

| Language/Implementation | Send Time | Throughput | Relative Performance | Rating |
|------------------------|-----------|------------|---------------------|--------|
| **Python Cython** 🏆 | 776.7ms | **1,283,890 msg/s** | **105%** | ⭐⭐⭐⭐⭐ |
| **C Baseline** | 813.6ms | 1,225,684 msg/s | 100% | ⭐⭐⭐⭐⭐ |
| **Python C Extension** | 1,049.7ms | 950,718 msg/s | 78% | ⭐⭐⭐⭐ |
| **Python ctypes** | 25,887ms | 38,615 msg/s | 3% | ❌ |

**Key Findings**:
- Python Cython has best performance, even exceeding C baseline by 5%
- Python C Extension reaches 78% of C baseline, suitable for production
- Python ctypes has poor performance, not recommended for production

---

⭐ If this project helps you, please give us a Star!
