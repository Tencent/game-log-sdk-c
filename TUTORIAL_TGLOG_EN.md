# TGLog Multi-threaded Version Tutorial

> Note: This tutorial is AI-generated for reference only. Please refer to actual code. Example code uses dynamic linking during compilation; static linking is recommended for actual projects.

## Table of Contents
1. [Introduction](#introduction)
2. [What is TGLog](#what-is-tglog)
3. [Environment Preparation](#environment-preparation)
4. [Build and Installation](#build-and-installation)
5. [Quick Start](#quick-start)
6. [Detailed Configuration](#detailed-configuration)
7. [API Reference](#api-reference)
8. [Complete Example](#complete-example)
9. [Performance Optimization](#performance-optimization)
10. [Common Questions](#common-questions)
11. [Error Handling](#error-handling)

---

## Introduction

Welcome to TGLog multi-threaded version! This is a high-performance log reporting C SDK designed for game servers. This tutorial will guide you step-by-step on how to use this library, even if you're new to C programming.

### Applicable Scenarios
- High-throughput log reporting (up to 1 million logs/sec)
- Multi-threaded game servers
- Latency-insensitive scenarios
- Asynchronous log processing needs

---

## What is TGLog

TGLog is an SDK for reporting game server runtime logs. It helps you:
- Send user behavior data from games to log servers
- Support both TCP and UDP protocols
- Automatic batch sending for improved efficiency
- Support disconnection and reconnection
- Provide send success/failure callback notifications

### Multi-threaded Version Features
- **High Performance**: Uses multiple background worker threads for log sending, throughput up to 1 million logs/sec
- **Asynchronous Processing**: Returns immediately after calling send API, doesn't block game main logic
- **Auto-Management**: Background threads automatically handle connection, reconnection, batch sending, etc.
- **Thread-Safe**: Can call API from multiple threads simultaneously

---

## Environment Preparation

### System Requirements
- Linux operating system (recommended CentOS 7+ or Ubuntu 18.04+)
- GCC 4.8 or higher
- CMake 3.10 or higher (optional)

### Dependencies
TGLog depends on the following third-party libraries:
- **libevent**: Event-driven library
- **libevent_pthreads**: libevent multi-thread support
- **concurrencykit (libck)**: Lock-free data structure library
- **jemalloc**: High-performance memory allocator

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

### 2. Build TGLog Library
```bash
# Build using Makefile
make tglog

# Or use CMake
mkdir build && cd build
cmake ..
make tglog
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
    └── libtglog.a    # Static library
```

---

## Quick Start

Here's the simplest example showing how to send a log:

### Step 1: Create Source File `hello_tglog.c`

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "tglog/tglog.h"

// Callback function: called when log send succeeds or fails
void send_callback(const char *log_payload, int err_code, const char *err_msg, void *user_data) {
    if (err_code == TGLOG_ERR_OK) {
        printf("Log sent successfully!\n");
    } else {
        printf("Log send failed, error code: %d, error message: %s\n", err_code, err_msg);
    }
}

int main() {
    // Step 1: Initialize multi-thread support (call once per process)
    tglog_use_thread();

    // Step 2: Create configuration object
    // Parameters: protocol type, server address, port, debug log callback (can be NULL), debug log callback (can be NULL), user data (can be NULL)
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

    // Step 3: Create client
    tglog_client *client = tglog_client_create(conf);
    if (client == NULL) {
        fprintf(stderr, "Failed to create client!\n");
        tglog_conf_destroy(conf);
        return 1;
    }

    // Step 4: Prepare log data
    // TGLog log format: logname|field1|field2|...\n
    const char *log_data = "userlogin|12345|alice|10\n";

    // Step 5: Send log
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

    // Step 6: Wait for log to be sent
    // Since it's async send, need to wait for background thread to complete
    printf("Waiting for log to be sent...\n");
    sleep(2);

    // Step 7: Cleanup resources
    tglog_client_destroy(client);
    tglog_conf_destroy(conf);

    printf("Program ended\n");
    return 0;
}
```

### Step 2: Compile Program

```bash
gcc hello_tglog.c -o hello_tglog \
    -I/path/to/sdk-c/dist/include \
    -L/path/to/sdk-c/dist/lib \
    -ltglog -levent -levent_pthreads -ljemalloc -lpthread -latomic -lm
```

**Compilation Parameter Description:**
- `-I`: Specify header file directory
- `-L`: Specify library file directory
- `-ltglog`: Link tglog library
- `-levent`: Link libevent library
- `-levent_pthreads`: Link libevent multi-thread support
- `-ljemalloc`: Link jemalloc library
- `-lpthread`: Link pthread thread library
- `-latomic`: Link atomic operations library (128-bit CAS support)
- `-lm`: Link math library

### Step 3: Run Program

```bash
# Ensure library file path is in LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/path/to/sdk-c/dist/lib:$LD_LIBRARY_PATH

# Run program
./hello_tglog
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

TGLog uses `tglog_conf` structure to store all configuration parameters.

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

#### 1. Worker Thread Count

```c
conf->worker_num = 8;  // Default: 8
```

- **Description**: Number of background worker threads, each thread independently processes log sending
- **Recommendation**: Adjust based on CPU core count, typically set to 4-16
- **Impact**: More threads can increase throughput but also increase CPU and memory overhead

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
// Maximum pending messages per worker thread
conf->max_pending_messages = 409600;  // Default: 409600
```

- **Description**: If pending messages exceed this limit, new send requests will fail
- **Recommendation**: Adjust based on memory size and log volume
- **Note**: Actual value will be rounded up to nearest power of 2

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

TGLog uses memory pools to reduce memory allocation overhead. You can configure capacity of different size memory pools:

```c
// Message object pool capacity
conf->message_pool_cap = 1000000;  // Default: 1000000

// Different size payload memory pool capacities
conf->payload_pool_256b_cap = 20000;   // 256-byte pool, default: 20000
conf->payload_pool_512b_cap = 20000;   // 512-byte pool, default: 20000
conf->payload_pool_1kb_cap = 20000;    // 1KB pool, default: 20000
conf->payload_pool_2kb_cap = 10000;    // 2KB pool, default: 10000
conf->payload_pool_4kb_cap = 10000;    // 4KB pool, default: 10000
conf->payload_pool_8kb_cap = 10000;    // 8KB pool, default: 10000
conf->payload_pool_16kb_cap = 5000;    // 16KB pool, default: 5000
conf->payload_pool_32kb_cap = 5000;    // 32KB pool, default: 5000
conf->payload_pool_48kb_cap = 1000;    // 48KB pool, default: 1000
conf->payload_pool_64kb_cap = 1000;    // 64KB pool, default: 1000
```

### Debug Log Configuration

If you want to view SDK internal operations, can configure debug logging:

```c
#include <stdio.h>
#include <stdarg.h>
#include "tglog/tglog.h"

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
    printf("[TGLOG-%s] %s:%d %s() - ", level_str[level], file, line, func);

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    printf("\n");
}

int main() {
    tglog_use_thread();

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

**Note**: Debug log callback functions must be thread-safe as they will be called in different worker threads.

---

## API Reference

### Initialization Functions

#### tglog_use_thread()

```c
void tglog_use_thread(void);
```

**Description**: Initialize multi-thread support, must be called before creating client, and only once per process.

**Note**: This function internally calls `evthread_use_pthreads()`. If your program has already called this function, no need to call `tglog_use_thread()`.

**Example**:
```c
int main() {
    tglog_use_thread();  // Call at beginning of main function
    // ... create client
}
```

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

**Note**: Callback functions must be thread-safe.

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

**Note**: After creating client, background threads automatically start and connect to server.

#### tglog_client_destroy()

```c
void tglog_client_destroy(tglog_client *client);
```

**Description**: Destroy client object, stop background threads, close connections, free resources.

**Note**:
- This function waits for all pending logs to be sent (waits at most `stop_timeout` milliseconds)
- After destruction, callbacks for all incomplete send requests will be called with error code `TGLOG_ERR_PRODUCER_CLOSED`

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

**Note**:
- Callback function executes in background worker thread, must be thread-safe
- Don't perform time-consuming operations in callback, will affect performance
- Can access `user_data` in callback, but ensure data is still valid when callback executes

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

**Return Value**: Queue capacity (equals `worker_num * max_pending_messages`).

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

Here's a more complete example showing typical usage in actual game servers. Due to length, please refer to the Chinese version for the full example code, which demonstrates:

- Multi-threaded worker simulation
- Callback handling
- Debug logging
- Statistics monitoring
- Proper resource cleanup

---

## Performance Optimization

### 1. Adjust Worker Thread Count

```c
conf->worker_num = 8;  // Adjust based on CPU core count
```

- **Recommendation**: Set to 0.5 to 1 times CPU core count
- **Impact**: More threads can increase concurrent processing capability but also increase overhead

### 2. Optimize Batch Sending Parameters

```c
// High throughput scenario
conf->batching_max_messages = 100;           // Increase batch size
conf->batching_max_publish_delay = 20;       // Increase delay tolerance

// Low latency scenario
conf->batching_max_messages = 10;            // Reduce batch size
conf->batching_max_publish_delay = 1;        // Reduce delay
```

### 3. Use Zero-Copy Mode

If you can guarantee log data lifecycle, using `TGLOG_MSG_F_NONE` can avoid memory copying:

```c
// Use static buffer or global buffer
static char log_buffers[1000][256];
static atomic_int buffer_index = 0;

void send_log_zero_copy(tglog_client *client, int user_id) {
    int idx = atomic_fetch_add(&buffer_index, 1) % 1000;
    snprintf(log_buffers[idx], 256, "userlogin|%ld|%d\n", time(NULL), user_id);

    // Zero-copy send
    tglog_client_send(client, log_buffers[idx], NULL, NULL, TGLOG_MSG_F_NONE);
}
```

**Note**: Must ensure buffer is not modified before send completes.

### 4. Adjust Memory Pool Size

Based on your log size distribution, adjust memory pool configuration:

```c
// If most logs are around 512 bytes
conf->payload_pool_512b_cap = 40000;  // Increase 512-byte pool capacity
```

### 5. Monitor Queue Size

Periodically check queue size to avoid backlog:

```c
size_t queue_size = tglog_client_get_msg_queue_size(client);
size_t queue_capacity = tglog_client_get_msg_queue_capacity(client);

if (queue_size > queue_capacity * 0.8) {
    // Queue nearly full, consider reducing log send frequency or increasing queue capacity
    printf("Warning: Queue usage %.1f%%\n",
           (double)queue_size / queue_capacity * 100);
}
```

---

## Common Questions

### Q1: How to ensure all logs are sent when program exits?

**A**: Before calling `tglog_client_destroy()`, wait for queue to empty:

```c
// Wait for queue to empty
while (tglog_client_get_msg_queue_size(client) > 0) {
    usleep(10000);  // Wait 10ms
}

// Or set longer stop timeout
conf->stop_timeout = 10000;  // Wait 10 seconds

tglog_client_destroy(client);
```

### Q2: How to determine if log send succeeded?

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
```

### Q3: In which thread does callback function execute?

**A**: Callback function executes in background worker thread, not in the thread that called `tglog_client_send()`. Therefore:
- Callback function must be thread-safe
- Don't access non-thread-safe data structures in callback
- Don't perform time-consuming operations in callback

### Q4: Can I call tglog_client_send() again in callback?

**A**: Yes, but not recommended. If you must do so, be careful to avoid stack overflow from recursive calls.

### Q5: How to handle server disconnection?

**A**: SDK will automatically reconnect, you don't need to do anything. If reconnection fails, send request will return error, callback will receive error code.

### Q6: Should I choose UDP or TCP?

**A**:
- **TCP**: Reliable, ordered, recommended for important logs
- **UDP**: High performance but may lose packets, suitable for scenarios with low reliability requirements

### Q7: Why does tglog_client_send() return 0 but callback receives error?

**A**: `tglog_client_send()` returning 0 only means log successfully added to send queue, doesn't mean send succeeded. Actual send result needs to be obtained through callback function.

### Q8: How to reduce memory usage?

**A**:
- Reduce `worker_num`
- Reduce `max_pending_messages`
- Reduce memory pool capacity configuration

### Q9: What are the log format requirements?

**A**:
- Must end with `\n`
- Recommended format: `logname|v1|v2|v3|...\n`
- Field values should not contain `\n`

### Q10: How to debug SDK internal issues?

**A**: Configure debug logging:

```c
bool can_log(tglog_level level, void *user_data) {
    return level >= TGLOG_LEVEL_DEBUG;  // Output all levels
}

void write_log(void *user_data, tglog_level level, const char *file,
               const int line, const char *func, const char *format, ...) {
    printf("[TGLOG] %s:%d - ", file, line);
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

tglog_conf *conf = tglog_conf_create(
    TGLOG_NETWORK_TCP, "127.0.0.1", 9999,
    can_log, write_log, NULL
);
```

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
            // Send success, no handling needed
            break;

        case TGLOG_ERR_SEND_TIMEOUT:
            fprintf(stderr, "Send timeout: %s\n", err_msg);
            // Consider retry or log to local file
            break;

        case TGLOG_ERR_SEND_QUEUE_IS_FULL:
            fprintf(stderr, "Send queue full: %s\n", err_msg);
            // Consider reducing log send frequency or increasing queue capacity
            break;

        case TGLOG_ERR_NO_AVAILABLE_CONN:
            fprintf(stderr, "No available connection: %s\n", err_msg);
            // SDK will auto-reconnect, can temporarily cache logs
            break;

        case TGLOG_ERR_PRODUCER_CLOSED:
            fprintf(stderr, "Client closed: %s\n", err_msg);
            // Program exiting, ignore this error
            break;

        case TGLOG_ERR_LOG_TOO_LONG:
            fprintf(stderr, "Log too long: %s\n", err_msg);
            // Need to shorten log content
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

Congratulations on completing the TGLog multi-threaded version tutorial! Now you should be able to:

1. ✓ Understand TGLog basic concepts and use cases
2. ✓ Correctly install dependencies and build SDK
3. ✓ Create configuration and client
4. ✓ Send logs and handle callbacks
5. ✓ Adjust configuration parameters to optimize performance
6. ✓ Monitor runtime status and statistics
7. ✓ Handle errors and exceptions

### Next Steps

- Read `tglog/DESIGN.md` to understand internal implementation principles
- Check `test/tglog_test.c` to learn more advanced usage
- Adjust configuration parameters based on actual business needs
- Test thoroughly in test environment before deploying to production

### Get Help

- View comments and documentation in source code
- Refer to example code in `test` directory
- Use debug logging feature to troubleshoot issues

Happy coding!
