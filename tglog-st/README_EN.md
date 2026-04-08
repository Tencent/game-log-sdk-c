# TGLog SDK-ST - High Performance C Logging Client (Single-Threaded Version)

![C99](https://img.shields.io/badge/C-C99-blue.svg)
![Performance](https://img.shields.io/badge/Performance-100K_logs/sec-green.svg)
![Protocol](https://img.shields.io/badge/Protocol-TCP/UDP-orange.svg)
![Threading](https://img.shields.io/badge/Threading-Single--Threaded-yellow.svg)

TGLog SDK-ST is a high-performance logging client library designed specifically for single-threaded environments (such as game servers). It adopts a pure single-threaded architecture and achieves efficient log transmission through an event-driven model, targeting 100K-200K log records of approximately 300 bytes per second.

## ✨ Core Features

### 🎮 Single-Thread Friendly
- **Zero Background Threads**: All processing in main thread, no thread synchronization overhead
- **Event-Driven**: Non-blocking I/O model based on libevent
- **Incremental Processing**: Each update processes limited work, doesn't block main loop
- **Game Server Optimized**: Designed for frame-driven game servers

### 🚀 High Performance Design
- **Lock-Free Architecture**: Single-threaded, no locks or atomic operations needed
- **Memory Pool**: Pre-allocation and object reuse reduce malloc/free overhead
- **Batch Transmission**: Intelligent batch aggregation reduces network overhead
- **Zero-Copy Support**: Multiple memory management strategies supported

### 🌐 Network Protocol Support
- **TCP Protocol**: Reliable transmission, max batch size 512KB
- **UDP Protocol**: High-performance transmission, max batch size 65507 bytes
- **Load Balancing**: Connection management support for CLB scenarios
- **Auto Reconnection**: Exponential backoff reconnection strategy

### 📊 Monitoring and Debugging
- **Real-time Metrics**: Rich performance statistics
- **Update Performance Monitoring**: Track performance of update calls
- **Pluggable Logging**: Support for custom debug log functions
- **JSON Output**: Easy integration with monitoring systems

### ⚙️ Flexible Configuration
- **Adjustable Parameters**: Batch size, timeout, retry count, etc.
- **Update Control**: Configurable max processing time and message count per update
- **Connection Management**: Semi-persistent connections with lifecycle configuration
- **DNS Resolution**: Periodic updates supporting domain changes

## 📋 System Requirements

### Dependencies
- **libevent**: High-performance event library
- **ConcurrencyKit (ck)**: For some data structures
- **pthread**: POSIX thread support (only used internally by libevent)

### Build Environment
- **C Compiler**: C99 standard support (GCC 4.9+, Clang 3.9+, MSVC 2019+)
- **Operating System**: Linux, Windows
- **Architecture**: x86_64

## 🚀 Quick Start

### Build and Install

```bash
# Enter project directory
cd sdk-c

# View available build targets
make help

# Build single-threaded version
make tglog-st

# Build test programs
make test-st
```

### Basic Usage

```c
#include "tglog-st/tglog.h"

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
// Client copies the configuration, conf can be freed
tglog_conf_destroy(conf);

// Send log callback function
void log_callback(const char *log_payload, int err_code,
                  const char *err_msg, void *user_data) {
    if (err_code == TGLOG_ERR_OK) {
        printf("Send success: %s", log_payload);
    } else {
        printf("Send failed: %s (error code: %d)\n", err_msg, err_code);
    }
}

// Game main loop
while (game_running) {
    // Process game logic
    process_game_logic();

    // Send log (must end with \n)
    const char *log = "userlogin|userid=12345|username=alice|timestamp=1640995200\n";
    int ret = tglog_client_send(client, log, log_callback, NULL, TGLOG_MSG_F_COPY);

    // Update log client (must be called periodically)
    tglog_client_update(client);

    // Other game logic...
}

// Get performance metrics
char buffer[8192];
int len = tglog_client_print_metrics(client, buffer, sizeof(buffer), true);
if (len > 0) {
    printf("Performance metrics:\n%s\n", buffer);
}

// Clean up resources
tglog_client_destroy(client);
```

> [!IMPORTANT]
>
> Important:
>
> - **Must periodically call `tglog_client_update()`**: This is the core of single-threaded version, used to process network I/O, batch sending, DNS updates, etc.
> - `tglog_client_create(conf)` copies the passed conf, which can be freed after the call
> - Callback functions are called in `tglog_client_update()`, no need to consider thread safety
> - `tglog_client` can be held long-term after creation, call `tglog_client_destroy()` to release it when the process ends

## 📖 Detailed Configuration

### tglog_conf Configuration Items

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `network` | enum | UDP | Network protocol (TCP/UDP) |
| `host` | string | - | Server address (IP or domain) |
| `port` | int | - | Server port |
| `update_interval` | int | 60s | DNS update interval |
| `conn_timeout` | int | 3000ms | Connection timeout (TCP only) |
| `write_high_watermark` | int | 32MB | Write buffer high watermark, stop writing when exceeded |
| `socket_send_buffer_size` | int | 16MB | Socket send buffer size |
| `socket_recv_buffer_size` | int | 1MB | Socket receive buffer size |
| `max_conn_lifetime` | int | 300s | Connection lifetime |
| `batching_max_publish_delay` | int | 10ms | Max batch delay |
| `batching_max_messages` | int | 100 | Max messages per batch |
| `batching_max_size` | int | 32KB | Max batch size |
| `max_pending_messages` | int | 1000000 | Max queue capacity |
| `send_timeout` | int | 30000ms | Max time a message can stay in client |
| `max_retries` | int | 3 | Max retry count |
| `stop_timeout` | int | 3000ms | Stop timeout |
| `max_update_us` | int | 300us | Max processing time per update (microseconds) |
| `max_messages_per_update` | int | 1000 | Max messages processed per update |
| `can_log` | function | tglog_can_log_debug | Log filter function |
| `write_log` | function | tglog_std_write_log | Log output function |

### Advanced Configuration Example

```c
tglog_conf *conf = tglog_conf_create(TGLOG_NETWORK_TCP, "logs.example.com", 8080, NULL, NULL, NULL);

// Performance optimization
conf->batching_max_messages = 200;          // Increase batch size
conf->batching_max_size = 80 * 1024;        // Increase batch bytes
conf->batching_max_publish_delay = 5;       // Reduce delay

// Update control configuration
conf->max_update_us = 500;                  // Max 500 microseconds per update
conf->max_messages_per_update = 2000;       // Max 2000 messages per update

// Network optimization
conf->socket_send_buffer_size = 32 * 1024 * 1024;  // 32MB send buffer
conf->write_high_watermark = 64 * 1024 * 1024;     // 64MB write high watermark
conf->max_conn_lifetime = 600;              // 10 minute connection lifetime
conf->conn_timeout = 5000;                  // 5 second connection timeout

// Reliability configuration
conf->max_retries = 5;                      // Increase retry count
conf->send_timeout = 60000;                 // 60 second send timeout

// Memory pool configuration
conf->message_pool_cap = 200000;            // Increase message pool capacity
conf->payload_pool_1kb_cap = 5000;          // Increase 1KB payload pool capacity
```

## 📊 Performance Monitoring

### Getting Statistics

```c
// Print performance metrics (JSON format)
char buffer[8192];
int len = tglog_client_print_metrics(client, buffer, sizeof(buffer), true);
if (len > 0) {
    printf("Performance metrics:\n%s\n", buffer);
}

// Print performance metrics (text format)
len = tglog_client_print_metrics(client, buffer, sizeof(buffer), false);
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
- `request_msg_count`: Application requested message count
- `enqueued_msg_count`: Successfully enqueued message count
- `dequeued_msg_count`: Successfully dequeued message count
- `succeeded_msg_count`: Successfully sent message count
- `failed_msg_count`: Failed message count (including timeout and dropped)
- `timeout_msg_count`: Timeout message count
- `retry_msg_count`: Retry count
- `dropped_msg_count`: Dropped message count (requested but not enqueued)
- `eagain_err_count`: EAGAIN error count
- `opened_conn_count`: Opened connection count
- `closed_conn_count`: Closed connection count
- `error_conn_count`: Error connection count
- `add_endpoint_count`: Added endpoint count
- `remove_endpoint_count`: Removed endpoint count

#### Gauge Type (Instantaneous State)
- `endpoint_count`: Current endpoint count
- `msg_queue_size`: Current queue size
- `msg_queue_capacity`: Queue capacity

#### Update Performance Metrics (Single-Threaded Specific)
- `total_update_count`: Total update call count
- `timeout_update_count`: Timeout update call count
- `idle_update_count`: Idle update call count (no messages processed)
- `max_update_time_us`: Max time a single update took (microseconds)
- `limit_update_time_us`: Configured update time limit (microseconds)
- `max_event_dispatch_time_us`: Max event dispatch time in a single update
- `max_msg_dispatch_time_us`: Max message dispatch time in a single update
- `max_msg_dispatch_count`: Max message count processed in a single update

## 🏗️ Architecture Design

### Overall Architecture

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
     │    ├── tglog_conn_list ◄─── Connection pool
     │    └── Message processing logic
     │
     ├── tglog_metrics ◄─── Performance statistics
     │
     └── tglog_debug_logger ◄─── Debug logging
```

### Data Flow

```
Send Request → Enqueue Message → Batch Processing → Network Send → Callback Notification
     │             │                  │                 │              │
     │             └─Direct to batch─┘      └─Update driven─┘    └─Sync callback─┘
     │
     └── Update Statistics (Plain variables)
```

## 🎮 Integration Methods

### Recommended Integration (Game Server)

```c
// Global log client
tglog_client *g_log_client = NULL;

// Game event handler examples
void on_player_login(int userid) {
    char log[512];
    snprintf(log, sizeof(log), "login|userid=%d|time=%ld\n", userid, time(NULL));
    tglog_client_send(g_log_client, log, NULL, NULL, TGLOG_MSG_F_COPY);
}

void on_battle_result(int winner_id, int loser_id, int score) {
    char log[512];
    snprintf(log, sizeof(log), "battle|winner=%d|loser=%d|score=%d\n",
             winner_id, loser_id, score);
    tglog_client_send(g_log_client, log, NULL, NULL, TGLOG_MSG_F_COPY);
}

// Initialization
void game_init() {
    tglog_conf *conf = tglog_conf_create(TGLOG_NETWORK_TCP, "server.com", 8080, NULL, NULL, NULL);
    g_log_client = tglog_client_create(conf);
    tglog_conf_destroy(conf);
}

// Game main loop
void game_loop() {
    while (game_running) {
        // Game logic processing, triggers various events
        process_game_logic();    // Calls on_player_login, on_battle_result, etc.
        handle_network_messages();
        update_game_state();

        // Update log client (batch sending, retries, etc.)
        tglog_client_update(g_log_client);
    }
}

// Cleanup
void game_cleanup() {
    tglog_client_destroy(g_log_client);
    g_log_client = NULL;
}
```

### High Frame Rate Game Scenario

```c
// For high frame rate games, reduce update frequency
static int update_counter = 0;
while (game_running) {
    game_update();

    // Call update every 10 frames
    if (++update_counter % 10 == 0) {
        tglog_client_update(client);
    }
}
```

### Low Latency Scenario

```c
// Configure shorter batch timeout and more frequent updates
conf->batching_max_publish_delay = 5;  // 5ms batch timeout
conf->max_update_us = 500;             // Max 500 microseconds per update

while (game_running) {
    game_update();

    // Call update every frame for low latency
    tglog_client_update(client);
}
```

## 🔧 Build Options

### Makefile Targets

```bash
make tglog-st      # Build single-threaded version library
make test-st       # Build single-threaded version test programs
make clean-st      # Clean single-threaded version build artifacts
```

### Build Optimization

```bash
# Optimized build
CFLAGS="-O3 -DNDEBUG" make tglog-st

# Debug build
CFLAGS="-g -O0 -DDEBUG" make tglog-st
```

> **Note**: The TGLog-ST library itself does not link jemalloc. If your application needs to use jemalloc, you should link it when compiling the final executable, for example:
> ```bash
> gcc your_app.c -o your_app -ltglog-st -levent -lck -ljemalloc -lpthread -lm
> ```

## 🔍 Troubleshooting

### Common Issues

#### Performance Issues
- **Update takes too long**: Check `max_update_time_us` metric, adjust `conf->max_update_us` and `conf->max_messages_per_update`
- **Slow log sending**: Increase update call frequency or adjust batch parameters
- **High memory usage**: Check queue capacity and memory pool configuration

#### Runtime Issues
- **Connection Failed**: Check server address and port configuration
- **Log Loss**: Check error callbacks, increase retry count
- **Update call timeout**: Reduce `max_messages_per_update` or increase `max_update_us`

#### Debug Tips
```c
// Enable verbose debug logging (shows all log levels)
tglog_conf_set_debug_logger(conf, tglog_can_log_debug, tglog_std_write_log, NULL);
```

### Performance Tuning

#### High Throughput Scenario
```c
conf->batching_max_messages = 200;
conf->batching_max_size = 100 * 1024;
conf->batching_max_publish_delay = 20;
conf->max_messages_per_update = 2000;
```

#### Low Latency Scenario
```c
conf->batching_max_messages = 10;
conf->batching_max_publish_delay = 5;
conf->max_pending_messages = 10000;
conf->max_update_us = 200;
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
- Field values cannot contain `|` or `\n` characters

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

**msg_flags parameter**:
- `TGLOG_MSG_F_NONE`: Zero-copy mode, client does not copy or free the message
- `TGLOG_MSG_F_FREE`: Client automatically frees the message after sending (must be allocated with malloc)
- `TGLOG_MSG_F_COPY`: Client automatically copies the message (recommended)

#### tglog_client_update
```c
int tglog_client_update(tglog_client *client);
```
Update client state, process network I/O, batch sending, DNS updates, etc. **Must be called periodically in main loop**.

Returns: Number of messages processed, negative value indicates error.

#### tglog_client_print_metrics
```c
int tglog_client_print_metrics(tglog_client *client, char *buffer, size_t size, bool json);
```
Print client performance metrics to buffer.

#### tglog_client_print_mem_pool_stats
```c
int tglog_client_print_mem_pool_stats(tglog_client *client, char *buffer, size_t size, bool json);
```
Print memory pool statistics to buffer.

#### tglog_client_destroy
```c
void tglog_client_destroy(tglog_client *client);
```
Destroy client instance and release all resources.

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

## 🆚 Comparison with Multi-Threaded Version

| Feature | Single-Threaded (tglog-st) | Multi-Threaded (tglog) |
|---------|---------------------------|------------------------|
| Threading Model | Single-threaded, requires update calls | Multiple Worker threads, automatic processing |
| Performance Target | 100K-200K logs/sec | 1M logs/sec |
| CPU Overhead | Low (no thread switching) | Medium (multi-threading overhead) |
| Latency | Depends on update frequency | Low (background thread processing) |
| Memory Usage | Low (single-threaded) | Medium (multi-threaded) |
| Integration Complexity | Requires main loop integration | Create and use |
| Use Cases | Game servers, single-threaded apps | High throughput, multi-threaded apps |
| Thread Safety | Not needed (single-threaded) | Fully thread-safe |

## 🤝 Contributing

### Development Environment
1. Install C99 compatible compiler
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

This project is open source under the [MIT License](LICENSE).

## 🆘 Support

- **Documentation**: See [DESIGN.md](DESIGN.md) for detailed design
- **Examples**: See example code in `test/` directory
- **Issues**: Submit issues via GitHub Issues
- **Discussion**: Welcome to participate in code review and technical discussions

---

⭐ If this project helps you, please give us a Star!
