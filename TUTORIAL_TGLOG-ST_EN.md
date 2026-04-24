# TGLog Single-threaded Version Tutorial

> Note: This tutorial is AI-generated for reference only. Please refer to actual code. Example code uses dynamic linking during compilation; static linking is recommended for actual projects.

## Table of Contents
1. [Introduction](#introduction)
2. [What is TGLog-ST](#what-is-tglog-st)
3. [Environment Preparation](#environment-preparation)
4. [Build and Installation](#build-and-installation)
5. [Quick Start](#quick-start)
6. [Detailed Configuration](#detailed-configuration)
7. [API Reference](#api-reference)
8. [Complete Example](#complete-example)
9. [Performance Optimization](#performance-optimization)
10. [Common Questions](#common-questions)
11. [Error Handling](#error-handling)
12. [Summary](#summary)

---

## Introduction

Welcome to TGLog single-threaded version! This is a log reporting C SDK optimized for game servers. This tutorial will guide you step-by-step on how to use this library, even if you're new to C programming.

### Applicable Scenarios
- Single-threaded game servers (e.g., frame-loop based game logic)
- Scenarios requiring precise execution timing control
- Latency-sensitive real-time games
- Projects not wanting to introduce multi-threading complexity

---

## What is TGLog-ST

TGLog-ST (Single-Threaded) is the single-threaded version of TGLog. It helps you:
- Send user behavior data from games to log servers
- Support both TCP and UDP protocols
- Automatic batch sending for improved efficiency
- Support disconnection and reconnection
- Provide send success/failure callback notifications

### Single-threaded Version Features
- **Complete Control**: All operations execute in your main thread, driven by `tglog_client_update()`
- **Low Latency**: No thread switching overhead, suitable for frame-rate sensitive games
- **Simple and Safe**: No need to consider thread synchronization and locking issues
- **Predictable**: Callbacks execute when you call `update()`, won't trigger at unexpected times
- **High Performance**: Despite being single-threaded, can still achieve 100-200k logs/sec throughput

---

## Environment Preparation

### System Requirements
- Linux operating system (recommended CentOS 7+ or Ubuntu 18.04+)
- GCC 4.8 or higher
- CMake 3.10 or higher (optional)

### Dependencies
TGLog-ST depends on the following third-party libraries:
- **libevent**: Event-driven library
- **concurrencykit (libck)**: Lock-free data structure library
- **jemalloc**: High-performance memory allocator

**Note**: Single-threaded version doesn't need `libevent_pthreads`, has fewer dependencies than multi-threaded version.

### Install Dependencies (CentOS)
```bash
# Install basic tools
sudo yum install -y gcc gcc-c++ make cmake git
```

### Install Dependencies (Ubuntu)
```bash
# Install basic tools
sudo apt-get update
sudo apt-get install -y build-essential cmake git
```

---

## Build and Installation

### 1. Get Source Code
```bash
# Assuming you already have SDK source directory
cd /path/to/sdk-c
```

### 2. Build TGLog-ST Library
```bash
# Build using Makefile
make tglog-st

# Or use CMake
mkdir build && cd build
cmake ..
make tglog-st
```

### 3. View Build Artifacts
After compilation, library files and headers will be installed in `dist` directory:
```
dist/
├── include/          # Header file directory
│   └── tglog/
│       ├── tglog.h   # Main header (includes all other headers)
│       ├── client.h
│       ├── conf.h
│       ├── def.h
│       ├── err.h
│       ├── metrics.h
│       └── debug_logger.h
└── lib/              # Library file directory
    └── libtglog-st.a # Static library
```

---

## Quick Start

Here's the simplest example showing how to send a log:

### Step 1: Create Source File `hello_tglog_st.c`

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "tglog-st/tglog.h"

// Callback function: called when log send succeeds or fails
void send_callback(const char *log_payload, int err_code, const char *err_msg, void *user_data) {
    if (err_code == TGLOG_ERR_OK) {
        printf("Log sent successfully!\n");
    } else {
        printf("Log send failed, error code: %d, error message: %s\n", err_code, err_msg);
    }
}

int main() {
    // Step 1: Create configuration object
    // Note: Single-threaded version doesn't need to call tglog_use_thread()
    tglog_conf *conf = tglog_conf_create(
        TGLOG_NETWORK_TCP,           // Use TCP protocol
        "127.0.0.1",                 // Log server address
        9999,                        // Log server port
        NULL,                        // Don't use debug logging
        NULL,
        NULL
    );

    if (conf == NULL) {
        fprintf(stderr, "Failed to create configuration!\n");
        return 1;
    }

    // Step 2: Create client
    tglog_client *client = tglog_client_create(conf);
    if (client == NULL) {
        fprintf(stderr, "Failed to create client!\n");
        tglog_conf_destroy(conf);
        return 1;
    }

    // Step 3: Prepare log data
    // TGLog log format: logname|field1|field2|...\n
    const char *log_data = "userlogin|12345|alice|10\n";

    // Step 4: Send log
    int ret = tglog_client_send(
        client,                      // Client object
        log_data,                    // Log content
        send_callback,               // Send result callback function
        NULL,                        // User data passed to callback
        TGLOG_MSG_F_COPY            // Flag: let SDK copy log content
    );

    if (ret != 0) {
        fprintf(stderr, "Failed to send log, return value: %d\n", ret);
    }

    // Step 5: Drive client processing (Critical! Single-threaded version specific)
    // Must periodically call update() to process network I/O and callbacks
    printf("Waiting for log to be sent...\n");
    for (int i = 0; i < 100; i++) {
        tglog_client_update(client);  // Drive client work
        usleep(10000);                // Wait 10ms
    }

    // Step 6: Cleanup resources
    tglog_client_destroy(client);
    tglog_conf_destroy(conf);

    printf("Program ended\n");
    return 0;
}
```

### Step 2: Compile Program

```bash
gcc hello_tglog_st.c -o hello_tglog_st \
    -I/path/to/sdk-c/dist/include \
    -L/path/to/sdk-c/dist/lib \
    -ltglog-st -levent -ljemalloc -lpthread -latomic -lm
```

**Compilation Parameter Description:**
- `-I`: Specify header file directory
- `-L`: Specify library file directory
- `-ltglog-st`: Link tglog-st library (note it's tglog-st, not tglog)
- `-levent`: Link libevent library
- `-ljemalloc`: Link jemalloc library
- `-lpthread`: Link pthread thread library
- `-latomic`: Link atomic operations library (128-bit CAS support)
- `-lm`: Link math library

**Note**: Single-threaded version doesn't need to link `-levent_pthreads`.

### Step 3: Run Program

```bash
# Ensure library file path is in LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/path/to/sdk-c/dist/lib:$LD_LIBRARY_PATH

# Run program
./hello_tglog_st
```

**Expected Output:**
```
Waiting for log to be sent...
Log sent successfully!
Program ended
```

---

## Detailed Configuration

### Creating Configuration Object

TGLog-ST uses `tglog_conf` structure to store all configuration parameters.

#### Create Configuration Object

```c
tglog_conf *tglog_conf_create(
    tglog_network network,      // Network protocol
    const char *host,           // Server address
    int port,                   // Server port
    can_log_func can_log,       // Debug log level judgment function (can be NULL)
    write_log_func write_log,   // Debug log output function (can be NULL)
    void *user_data            // User data passed to debug log functions (can be NULL)
);
```

**Parameter Description:**

1. **network**: Network protocol type
   - `TGLOG_NETWORK_UDP`: Use UDP protocol (high performance, but may lose packets)
   - `TGLOG_NETWORK_TCP`: Use TCP protocol (reliable, recommended)

2. **host**: Log server IP address or domain name
   - Example: `"192.168.1.100"` or `"log.example.com"`

3. **port**: Log server port number
   - Example: `9999`

4. **can_log / write_log**: Debug log callback functions (optional)
   - Used to output SDK internal debug information
   - Can pass `NULL` if debugging not needed

#### Set Default Configuration

After creating configuration object, can call `tglog_conf_set_default()` to set all parameters to default values:

```c
tglog_conf *conf = tglog_conf_create(TGLOG_NETWORK_TCP, "127.0.0.1", 9999, NULL, NULL, NULL);
tglog_conf_set_default(conf);  // Set default values
```

### Important Configuration Parameters

After creating configuration object, you can directly modify its member variables to adjust behavior:

#### 1. Update Call Configuration (Single-threaded Version Specific)

```c
// Maximum execution time per update() call (microseconds)
conf->max_update_us = 200;  // Default: 200 microseconds

// Maximum messages to process per update() call
conf->max_messages_per_update = 1000;  // Default: 1000
```

- **Description**: These two parameters control `tglog_client_update()` behavior
- **max_update_us**: Limits single update execution time to avoid blocking game main loop too long
- **max_messages_per_update**: Limits messages processed per call, used with time limit
- **Recommendations**:
  - If game frame rate is 60 FPS, each frame ~16ms, can set `max_update_us = 500` (0.5ms)
  - If log volume is large, can increase `max_messages_per_update`

#### 2. Batch Sending Configuration

```c
// Maximum number of messages in batch
conf->batching_max_messages = 50;  // Default: 50

// Maximum bytes in batch
conf->batching_max_size = 32 * 1024;  // Default: 32KB

// Maximum delay for batch sending (milliseconds)
conf->batching_max_publish_delay = 10;  // Default: 10ms
```

- **Description**: To improve efficiency, SDK merges multiple logs into one batch for sending
- **Trigger Conditions**: Batch sent when any of the following conditions met:
  - Message count reaches `batching_max_messages`
  - Total bytes reach `batching_max_size`
  - Time since last send exceeds `batching_max_publish_delay` milliseconds

#### 3. Connection Configuration

```c
// Connection timeout (milliseconds)
conf->conn_timeout = 3000;  // Default: 3000ms (3 seconds)

// Maximum connection lifetime (seconds)
conf->max_conn_lifetime = 300;  // Default: 300 seconds (5 minutes)

// Send timeout (milliseconds)
conf->send_timeout = 30000;  // Default: 30000ms (30 seconds)

// Maximum retry count
conf->max_retries = 3;  // Default: 3 times

// Stop timeout (milliseconds)
conf->stop_timeout = 3000;  // Default: 3000ms (3 seconds)
```

**Note**: When `max_conn_lifetime` is set to 0 or negative, persistent connections will be used. Only recommended when server is directly on DNS; otherwise (e.g., behind CLB) will cause traffic imbalance.

#### 4. Message Queue Configuration

```c
// Maximum pending messages
conf->max_pending_messages = 1000000;  // Default: 1000000
```

- **Description**: If pending messages exceed this limit, new send requests will fail
- **Recommendation**: Adjust based on memory size and log volume
- **Note**:
  - Actual value will be rounded up to nearest power of 2
  - Single-threaded version default (1 million) is larger than multi-threaded version (409.6k per worker)

#### 5. DNS Update Interval

```c
// DNS resolution result update interval (seconds)
conf->update_interval = 60;  // Default: 60 seconds
```

- **Description**: If server address is domain name, SDK will periodically re-resolve DNS

#### 6. Socket Buffer Configuration

```c
// Socket send buffer size (bytes)
conf->socket_send_buffer_size = 64 * 1024 * 1024;  // Default: 64MB

// Socket receive buffer size (bytes)
conf->socket_recv_buffer_size = 16 * 1024;  // Default: 16KB

// Write high watermark (bytes)
conf->write_high_watermark = 32 * 1024 * 1024;  // Default: 32MB
```

- **write_high_watermark**: When buffered data size in bufferevent exceeds this value, stop writing data to that connection

#### 7. Memory Pool Configuration

TGLog-ST uses memory pools to reduce memory allocation overhead. You can configure capacity of different size memory pools:

```c
// Message object pool capacity
conf->message_pool_cap = 100000;  // Default: 100000

// Different size payload memory pool capacities
conf->payload_pool_256b_cap = 2000;   // 256-byte pool, default: 2000
conf->payload_pool_512b_cap = 2000;   // 512-byte pool, default: 2000
conf->payload_pool_1kb_cap = 2000;    // 1KB pool, default: 2000
conf->payload_pool_2kb_cap = 1000;    // 2KB pool, default: 1000
conf->payload_pool_4kb_cap = 1000;    // 4KB pool, default: 1000
conf->payload_pool_8kb_cap = 1000;    // 8KB pool, default: 1000
conf->payload_pool_16kb_cap = 500;    // 16KB pool, default: 500
conf->payload_pool_32kb_cap = 500;    // 32KB pool, default: 500
conf->payload_pool_48kb_cap = 100;    // 48KB pool, default: 100
conf->payload_pool_64kb_cap = 100;    // 64KB pool, default: 100
```

**Note**: Single-threaded version memory pool default sizes are about 1/10 of multi-threaded version, as only one thread uses them.

### Debug Log Configuration

If you want to view SDK internal operations, can configure debug logging:

```c
#include <stdio.h>
#include <stdarg.h>
#include "tglog-st/tglog.h"

// Determine whether to output certain log level
bool can_log(tglog_level level, void *user_data) {
    // TGLOG_LEVEL_DEBUG = 0
    // TGLOG_LEVEL_INFO  = 1
    // TGLOG_LEVEL_WARN  = 2
    // TGLOG_LEVEL_ERROR = 3
    // TGLOG_LEVEL_FATAL = 4
    return level >= TGLOG_LEVEL_INFO;  // Only output INFO and above levels
}

// Output log content
void write_log(void *user_data, tglog_level level, const char *file,
               const int line, const char *func, const char *format, ...) {
    const char *level_str[] = {"DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
    printf("[TGLOG-ST-%s] %s:%d %s() - ", level_str[level], file, line, func);

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    printf("\n");
}

int main() {
    // Pass debug log callbacks when creating configuration
    tglog_conf *conf = tglog_conf_create(
        TGLOG_NETWORK_TCP,
        "127.0.0.1",
        9999,
        can_log,      // Pass level judgment function
        write_log,    // Pass log output function
        NULL
    );

    // ... subsequent code
}
```

---

## API Reference

### Configuration Management Functions

#### tglog_conf_create()

```c
tglog_conf *tglog_conf_create(
    tglog_network network,
    const char *host,
    int port,
    can_log_func can_log,
    write_log_func write_log,
    void *user_data
);
```

**Description**: Create configuration instance and set to default values.

**Return Value**: Returns configuration object pointer on success, `NULL` on failure.

#### tglog_conf_set_default()

```c
void tglog_conf_set_default(tglog_conf *conf);
```

**Description**: Set all parameters of configuration object to default values.

#### tglog_conf_set_server()

```c
bool tglog_conf_set_server(
    tglog_conf *conf,
    tglog_network network,
    const char *host,
    int port
);
```

**Description**: Set server address and protocol.

**Return Value**: Returns `true` on success, `false` on failure.

#### tglog_conf_set_debug_logger()

```c
void tglog_conf_set_debug_logger(
    tglog_conf *conf,
    can_log_func can_log,
    write_log_func write_log,
    void *user_data
);
```

**Description**: Set debug log callback functions.

#### tglog_conf_destroy()

```c
void tglog_conf_destroy(tglog_conf *conf);
```

**Description**: Destroy configuration object, free memory.

**Note**: Configuration object can be destroyed after creating client, as client saves a copy of configuration.

### Client Management Functions

#### tglog_client_create()

```c
tglog_client *tglog_client_create(const tglog_conf *conf);
```

**Description**: Create client object.

**Return Value**: Returns client pointer on success, `NULL` on failure.

**Note**:
- After creating client, need to periodically call `tglog_client_update()` to drive its work
- Single-threaded version internally creates and maintains its own event_base
- Initial connection created synchronously with timeout mechanism

#### tglog_client_destroy()

```c
void tglog_client_destroy(tglog_client *client);
```

**Description**: Destroy client object, close connections, free resources.

**Note**:
- This function tries to send all pending logs (waits at most `stop_timeout` milliseconds)
- After destruction, callbacks for all incomplete send requests will be called with error code `TGLOG_ERR_PRODUCER_CLOSED`

#### tglog_client_update() (Single-threaded Version Specific)

```c
int tglog_client_update(tglog_client *client);
```

**Description**: Drive client to process network I/O, send logs, execute callbacks, etc.

**Return Value**: Returns number of messages processed on success, negative on failure.

**Important**: This is the most critical function for single-threaded version!
- Must be called periodically in game main loop (recommended once per frame)
- All network operations and callbacks execute in this function
- Execution time limited by `max_update_us` and `max_messages_per_update`
- Function is non-blocking, returns quickly

**Example**:
```c
// Game main loop
while (game_is_running) {
    // Process game logic
    update_game_logic();

    // Drive log client
    tglog_client_update(client);

    // Render
    render_frame();

    // Control frame rate
    sleep_until_next_frame();
}
```

### Log Sending Functions

#### tglog_client_send()

```c
int tglog_client_send(
    tglog_client *client,
    const char *log_payload,
    tglog_send_callback callback,
    void *user_data,
    char msg_flag
);
```

**Description**: Send a log message.

**Parameters**:
- `client`: Client object
- `log_payload`: Log content, must end with `\n`
- `callback`: Send result callback function (can be `NULL`)
- `user_data`: User data passed to callback function (can be `NULL`)
- `msg_flag`: Message flag, controls memory management method

**Return Value**: Returns 0 on success, non-zero error code on failure.

**Log Format**:
```
<log_name>|v1|v2|v3|...|vn\n
```
For example:
```
login|2025-06-06 12:00:00|alice|password\n
```

**Message Flags (msg_flag)**:

1. **TGLOG_MSG_F_COPY** (Recommended)
   ```c
   tglog_client_send(client, log_data, callback, NULL, TGLOG_MSG_F_COPY);
   ```
   - SDK will copy log content
   - After call returns, you can immediately free or modify `log_payload`
   - Safest, simplest, recommended

2. **TGLOG_MSG_F_FREE**
   ```c
   char *log_data = malloc(100);
   sprintf(log_data, "login|2025-06-06 12:00:00|alice\n");
   tglog_client_send(client, log_data, callback, NULL, TGLOG_MSG_F_FREE);
   // Don't free(log_data), SDK will auto-free
   ```
   - `log_payload` must be memory allocated via `malloc()` or `calloc()`
   - SDK will automatically call `free()` to release after sending
   - Suitable for scenarios requiring dynamic log construction

3. **TGLOG_MSG_F_NONE**
   ```c
   static const char *log_data = "login|2025-06-06 12:00:00|alice\n";
   tglog_client_send(client, log_data, callback, NULL, TGLOG_MSG_F_NONE);
   ```
   - SDK neither copies nor frees, zero-copy
   - You must ensure `log_payload` remains valid until send completes
   - Suitable for static strings or global buffers
   - High performance but complex to use, error-prone

**Callback Function Prototype**:
```c
typedef void (*tglog_send_callback)(
    const char *log_payload,
    int err_code,
    const char *err_msg,
    void *user_data
);
```

- `log_payload`: Sent log content
- `err_code`: Error code, `TGLOG_ERR_OK` (0) indicates success
- `err_msg`: Error message string
- `user_data`: User data passed during send

**Important**:
- Callback function executes when calling `tglog_client_update()`, not when calling `send()`
- Callback executes in main thread, no need to consider thread safety
- Can safely access game data and call game functions in callback

### Monitoring Functions

#### tglog_client_get_msg_queue_size()

```c
size_t tglog_client_get_msg_queue_size(tglog_client *client);
```

**Description**: Get current number of pending messages.

**Return Value**: Pending message count.

#### tglog_client_get_msg_queue_capacity()

```c
size_t tglog_client_get_msg_queue_capacity(tglog_client *client);
```

**Description**: Get total capacity of message queue.

**Return Value**: Queue capacity (equals `max_pending_messages`).

#### tglog_client_print_metrics()

```c
int tglog_client_print_metrics(
    tglog_client *client,
    char *buffer,
    size_t size,
    bool json
);
```

**Description**: Get performance statistics.

**Parameters**:
- `client`: Client object
- `buffer`: Output buffer, at least 4096 bytes (JSON format needs 8192 bytes)
- `size`: Buffer size
- `json`: `true` for JSON format output, `false` for text format

**Return Value**: Returns bytes written on success, -1 on failure.

**Example**:
```c
char buffer[4096];
int ret = tglog_client_print_metrics(client, buffer, sizeof(buffer), false);
if (ret > 0) {
    printf("%s\n", buffer);
}
```

#### tglog_client_print_mem_pool_stats()

```c
int tglog_client_print_mem_pool_stats(
    tglog_client *client,
    char *buffer,
    size_t size,
    bool json
);
```

**Description**: Get memory pool statistics.

**Parameters**: Same as `tglog_client_print_metrics()`.

---

## Complete Example

Since the complete example for single-threaded version is very similar to multi-threaded version, here we only show key differences. For complete examples, please refer to:
- Example in source code: `test-st/tglog_test.c`
- Performance test in source code: `test-st/simple_perf_test.c`

### Key Differences

Compared to multi-threaded version, main differences in single-threaded version:

1. **No need to call `tglog_use_thread()`**
2. **Must call `tglog_client_update()` in main loop**
3. **Callbacks execute in main thread, no thread safety concerns**

### Game Main Loop Integration Example

```c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include "tglog-st/tglog.h"

// Global statistics
int g_success_count = 0;
int g_failed_count = 0;

// Send result callback
void on_send_result(const char *log_payload, int err_code, const char *err_msg, void *user_data) {
    if (err_code == TGLOG_ERR_OK) {
        g_success_count++;
    } else {
        g_failed_count++;
        fprintf(stderr, "Send failed: %s, error code: %d, error message: %s\n",
                log_payload, err_code, err_msg);
    }
}

// Get current time (microseconds)
long long get_time_us() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000LL + tv.tv_usec;
}

// Simulate game main loop
void game_main_loop(tglog_client *client, int total_frames) {
    const int target_fps = 60;
    const long long frame_time_us = 1000000 / target_fps;

    printf("\nStarting game main loop (target frame rate: %d FPS)\n", target_fps);
    printf("Frame time: %.2f ms\n\n", frame_time_us / 1000.0);

    for (int frame = 0; frame < total_frames; frame++) {
        long long frame_start = get_time_us();

        // === Game logic processing ===
        // Simulate 5 players generating logs per frame
        for (int i = 0; i < 5; i++) {
            int user_id = frame * 5 + i;
            char log_buffer[256];
            snprintf(log_buffer, sizeof(log_buffer),
                     "battle|%ld|%d|%d|win\n",
                     time(NULL), user_id, 2001 + (i % 5));
            tglog_client_send(client, log_buffer, on_send_result, NULL, TGLOG_MSG_F_COPY);
        }

        // === Drive log client (Critical!) ===
        long long update_start = get_time_us();
        int msg_count = tglog_client_update(client);
        long long update_end = get_time_us();
        long long update_time = update_end - update_start;

        // === Periodically output statistics ===
        if (frame % 60 == 0) {
            size_t queue_size = tglog_client_get_msg_queue_size(client);
            size_t queue_capacity = tglog_client_get_msg_queue_capacity(client);

            printf("[Frame %d] Queue: %zu/%zu, Success: %d, Failed: %d, update time: %lld us, processed messages: %d\n",
                   frame, queue_size, queue_capacity,
                   g_success_count, g_failed_count, update_time, msg_count);
        }

        // === Frame rate control ===
        long long frame_end = get_time_us();
        long long frame_elapsed = frame_end - frame_start;
        long long sleep_time = frame_time_us - frame_elapsed;

        if (sleep_time > 0) {
            usleep(sleep_time);
        }
    }

    printf("\nGame main loop ended\n");
}

int main(int argc, char *argv[]) {
    printf("=== TGLog Single-threaded Version Complete Example ===\n\n");

    // Create configuration
    const char *server_host = "127.0.0.1";
    int server_port = 9999;

    if (argc >= 3) {
        server_host = argv[1];
        server_port = atoi(argv[2]);
    }

    tglog_conf *conf = tglog_conf_create(TGLOG_NETWORK_TCP, server_host, server_port, NULL, NULL, NULL);
    if (conf == NULL) {
        fprintf(stderr, "Failed to create configuration!\n");
        return 1;
    }

    // Adjust configuration parameters
    conf->max_update_us = 500;
    conf->max_messages_per_update = 1000;
    conf->batching_max_messages = 100;
    conf->batching_max_publish_delay = 5;

    // Create client
    tglog_client *client = tglog_client_create(conf);
    if (client == NULL) {
        fprintf(stderr, "Failed to create client!\n");
        tglog_conf_destroy(conf);
        return 1;
    }
    tglog_conf_destroy(conf);

    // Run game main loop (simulate 10 seconds)
    game_main_loop(client, 600);

    // Wait for remaining logs to be sent
    printf("\nWaiting for remaining logs to be sent...\n");
    for (int i = 0; i < 50; i++) {
        tglog_client_update(client);
        size_t queue_size = tglog_client_get_msg_queue_size(client);
        if (queue_size == 0) {
            printf("All logs sent\n");
            break;
        }
        usleep(100000);
    }

    // Cleanup resources
    tglog_client_destroy(client);

    printf("\n=== Final Statistics ===\n");
    printf("Success: %d\n", g_success_count);
    printf("Failed: %d\n", g_failed_count);
    printf("Total: %d\n", g_success_count + g_failed_count);

    return 0;
}
```

### Compile and Run

```bash
# Compile
gcc -o game_server_st_example game_server_st_example.c \
    -I/path/to/sdk-c/dist/include \
    -L/path/to/sdk-c/dist/lib \
    -ltglog-st -levent -ljemalloc -lpthread -latomic -lm

# Run
./game_server_st_example
```

---

## Performance Optimization

### 1. Adjust Update Parameters

```c
// High throughput scenario
conf->max_update_us = 1000;              // Increase to 1ms
conf->max_messages_per_update = 2000;    // Increase to 2000

// Low latency scenario
conf->max_update_us = 200;               // Keep default 0.2ms
conf->max_messages_per_update = 500;     // Reduce to 500
```

### 2. Adjust Update Call Frequency

```c
// Strategy 1: Call once per frame (recommended)
while (running) {
    update_game();
    tglog_client_update(client);
    render();
}

// Strategy 2: Call every N frames
int frame = 0;
while (running) {
    update_game();
    if (frame % 2 == 0) {
        tglog_client_update(client);
    }
    render();
    frame++;
}

// Strategy 3: Dynamically adjust based on queue size
while (running) {
    update_game();
    size_t queue_size = tglog_client_get_msg_queue_size(client);
    if (queue_size > 100 || frame % 3 == 0) {
        tglog_client_update(client);
    }
    render();
}
```

### 3. Optimize Batch Sending Parameters

```c
// High throughput scenario
conf->batching_max_messages = 100;
conf->batching_max_publish_delay = 20;

// Low latency scenario
conf->batching_max_messages = 10;
conf->batching_max_publish_delay = 1;
```

### 4. Use Zero-Copy Mode

```c
// Use ring buffer
#define BUFFER_COUNT 1000
static char log_buffers[BUFFER_COUNT][256];
static int buffer_index = 0;

void send_log_zero_copy(tglog_client *client, int user_id) {
    int idx = buffer_index;
    buffer_index = (buffer_index + 1) % BUFFER_COUNT;

    snprintf(log_buffers[idx], 256, "userlogin|%ld|%d\n", time(NULL), user_id);
    tglog_client_send(client, log_buffers[idx], NULL, NULL, TGLOG_MSG_F_NONE);
}
```

### 5. Adjust Memory Pool Size

```c
// If most logs are around 512 bytes
conf->payload_pool_512b_cap = 4000;
```

### 6. Monitor Queue Size

```c
void check_queue_health(tglog_client *client) {
    size_t queue_size = tglog_client_get_msg_queue_size(client);
    size_t queue_capacity = tglog_client_get_msg_queue_capacity(client);
    float usage = (float)queue_size / queue_capacity;

    if (usage > 0.8) {
        printf("Warning: Queue usage %.1f%%\n", usage * 100);
    }
}
```

---

## Common Questions

### Q1: Why must I call tglog_client_update()?

**A**: Single-threaded version has no background threads. All network I/O, batch sending, callback execution happen in `update()`. If not called, logs will never be sent.

### Q2: How often should I call update()?

**A**: Recommended once per frame. If game frame rate is 60 FPS, that's once every 16ms.

### Q3: How long does update() block?

**A**: Limited by `max_update_us` parameter, default max 200 microseconds (0.2ms). Actual time depends on network conditions and pending message count.

### Q4: When does callback function execute?

**A**: Callback executes when calling `tglog_client_update()`, not when calling `send()`. This means:
- Callback execution timing is predictable
- Callback executes in main thread, no thread safety concerns
- Can safely access game data in callback

### Q5: How to ensure all logs are sent when program exits?

**A**: Before calling `tglog_client_destroy()`, continuously call `update()` until queue empties:

```c
while (tglog_client_get_msg_queue_size(client) > 0) {
    tglog_client_update(client);
    usleep(10000);
}
tglog_client_destroy(client);
```

### Q6: How to determine if log send succeeded?

**A**: Use callback function:

```c
void my_callback(const char *log_payload, int err_code, const char *err_msg, void *user_data) {
    if (err_code == TGLOG_ERR_OK) {
        printf("Send success\n");
    } else {
        printf("Send failed: %d - %s\n", err_code, err_msg);
    }
}

tglog_client_send(client, log_data, my_callback, NULL, TGLOG_MSG_F_COPY);
tglog_client_update(client);  // Trigger callback
```

### Q7: Can I call tglog_client_send() again in callback?

**A**: Yes, and completely safe! Since callback executes in main thread, no recursion or reentrancy issues.

### Q8: How to handle server disconnection?

**A**: SDK will automatically reconnect, you don't need to do anything. If reconnection fails, send request will return error, callback will receive error code.

### Q9: Should I choose UDP or TCP?

**A**:
- **TCP**: Reliable, ordered, recommended for important logs
- **UDP**: High performance but may lose packets, suitable for scenarios with low reliability requirements

### Q10: How much performance difference between single-threaded and multi-threaded versions?

**A**:
- **Single-threaded**: 100-200k logs/sec
- **Multi-threaded**: 1 million logs/sec

Single-threaded version has lower performance but sufficient for most game servers. Advantages are simplicity, controllability, low latency.

---

## Error Handling

### Common Error Codes

```c
#define TGLOG_ERR_OK                    0       // Success
#define TGLOG_ERR_SEND_TIMEOUT          10001   // Send timeout
#define TGLOG_ERR_SEND_FAILED           10002   // Send failed
#define TGLOG_ERR_PRODUCER_CLOSED       10003   // Client closed
#define TGLOG_ERR_SEND_QUEUE_IS_FULL    10004   // Send queue full
#define TGLOG_ERR_NO_AVAILABLE_CONN     10005   // No available connection
#define TGLOG_ERR_NEW_CONN_FAILED       10006   // Create connection failed
#define TGLOG_ERR_CONN_WRITE_FAILED     10007   // Connection write failed
#define TGLOG_ERR_CONN_READ_FAILED      10008   // Connection read failed
#define TGLOG_ERR_LOG_TOO_LONG          10009   // Log too long
#define TGLOG_ERR_BAD_LOG               10010   // Bad log format
#define TGLOG_ERR_BATCH_SIZE_EXCEEDED   10011   // Batch size exceeded
#define TGLOG_ERR_MEMORY_ALLOC_FAILED   10012   // Memory allocation failed
```

### Error Handling Example

```c
void on_send_result(const char *log_payload, int err_code, const char *err_msg, void *user_data) {
    switch (err_code) {
        case TGLOG_ERR_OK:
            break;

        case TGLOG_ERR_SEND_TIMEOUT:
            fprintf(stderr, "Send timeout: %s\n", err_msg);
            break;

        case TGLOG_ERR_SEND_QUEUE_IS_FULL:
            fprintf(stderr, "Send queue full: %s\n", err_msg);
            break;

        case TGLOG_ERR_NO_AVAILABLE_CONN:
            fprintf(stderr, "No available connection: %s\n", err_msg);
            break;

        case TGLOG_ERR_PRODUCER_CLOSED:
            fprintf(stderr, "Client closed: %s\n", err_msg);
            break;

        case TGLOG_ERR_LOG_TOO_LONG:
            fprintf(stderr, "Log too long: %s\n", err_msg);
            break;

        default:
            fprintf(stderr, "Unknown error: %d - %s\n", err_code, err_msg);
            break;
    }
}
```

### Get Error Message

```c
const char *tglog_get_err_msg(int code);
```

**Description**: Get error message string based on error code.

**Example**:
```c
int ret = tglog_client_send(client, log_data, NULL, NULL, TGLOG_MSG_F_COPY);
if (ret != 0) {
    const char *err_msg = tglog_get_err_msg(ret);
    fprintf(stderr, "Send failed: %s\n", err_msg);
}
```

---

## Summary

Congratulations on completing the TGLog single-threaded version tutorial! Now you should be able to:

1. ✓ Understand TGLog-ST basic concepts and use cases
2. ✓ Correctly install dependencies and build SDK
3. ✓ Create configuration and client
4. ✓ Correctly call `update()` in game main loop
5. ✓ Send logs and handle callbacks
6. ✓ Adjust configuration parameters to optimize performance
7. ✓ Monitor runtime status and statistics
8. ✓ Handle errors and exceptions

### Single-threaded vs Multi-threaded Version

| Feature | Single-threaded | Multi-threaded |
|---------|----------------|----------------|
| **Performance** | 100-200k logs/sec | 1 million logs/sec |
| **Latency** | Lower (no thread switching) | Slightly higher |
| **Control** | Fully controllable | Async execution |
| **Complexity** | Simple | More complex |
| **Callback Execution** | Main thread, predictable | Background thread, needs thread safety |
| **Initialization** | No special requirements | Needs to call `tglog_use_thread()` |
| **Drive Method** | Needs to call `update()` | Auto-runs |
| **Use Case** | Single-threaded game servers | Multi-threaded servers |

### Selection Recommendations

If your project meets the following conditions, single-threaded version is better choice:
- Game server is single-threaded architecture (e.g., frame-loop based)
- Log volume doesn't exceed 100-200k logs/sec
- Want precise control over log sending timing
- Don't want to introduce multi-threading complexity

Otherwise, consider using multi-threaded version.

### Next Steps

- Read `tglog-st/DESIGN.md` to understand internal implementation principles
- Check `test-st/tglog_test.c` to learn more advanced usage
- Adjust configuration parameters based on actual business needs
- Test thoroughly in test environment before deploying to production

### Get Help

- View comments and documentation in source code
- Refer to example code in `test-st` directory
- Use debug logging feature to troubleshoot issues

Happy coding!
