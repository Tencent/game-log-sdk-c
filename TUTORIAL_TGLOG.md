# TGLog 多线程版本使用教程

> 说明：此教程由AI生成，仅供参考，请以实际代码为准。示例代码编译时使用的是动态链接，实际项目建议静态链接。

## 目录
1. [简介](#简介)
2. [什么是 TGLog](#什么是-tglog)
3. [环境准备](#环境准备)
4. [编译安装](#编译安装)
5. [快速入门](#快速入门)
6. [详细配置说明](#详细配置说明)
7. [API 参考](#api-参考)
8. [完整示例](#完整示例)
9. [性能优化](#性能优化)
10. [常见问题](#常见问题)
11. [错误处理](#错误处理)

---

## 简介

欢迎使用 TGLog 多线程版本!这是一个高性能的日志上报 C 语言 SDK,专为游戏服务器设计。本教程将手把手教你如何使用这个库,即使你是 C 语言新手也能轻松上手。

### 适用场景
- 需要高吞吐量日志上报(最高 100 万条/秒)
- 多线程游戏服务器
- 对延迟不敏感的场景
- 需要异步日志处理

---

## 什么是 TGLog

TGLog 是一个用于上报游戏服务器运行日志的 SDK。它可以帮助你:
- 将游戏中的用户行为数据发送到日志服务器
- 支持 TCP 和 UDP 两种协议
- 自动批量发送,提高效率
- 支持断线重连
- 提供发送成功/失败的回调通知

### 多线程版本特点
- **高性能**: 使用多个后台工作线程处理日志发送,吞吐量可达 100 万条/秒
- **异步处理**: 调用发送 API 后立即返回,不阻塞游戏主逻辑
- **自动管理**: 后台线程自动处理连接、重连、批量发送等
- **线程安全**: 可以从多个线程同时调用 API

---

## 环境准备

### 系统要求
- Linux 操作系统(推荐 CentOS 7+ 或 Ubuntu 18.04+)
- GCC 4.8 或更高版本
- CMake 3.10 或更高版本(可选)

### 依赖库
TGLog 依赖以下第三方库:
- **libevent**: 事件驱动库
- **libevent_pthreads**: libevent 的多线程支持
- **concurrencykit (libck)**: 无锁数据结构库
- **jemalloc**: 高性能内存分配器

### 安装依赖(CentOS)
```bash
# 安装基础工具
sudo yum install -y gcc gcc-c++ make cmake git
```

### 安装依赖(Ubuntu)
```bash
# 安装基础工具
sudo apt-get update
sudo apt-get install -y build-essential cmake git
```

---

## 编译安装

### 1. 获取源码
```bash
# 假设你已经有了 SDK 源码目录
cd /path/to/sdk-c
```

### 2. 编译 TGLog 库
```bash
# 使用 Makefile 编译
make tglog

# 或者使用 CMake
mkdir build && cd build
cmake ..
make tglog
```

### 3. 查看编译产物
编译完成后,库文件和头文件会被安装到 `dist` 目录:
```
dist/
├── include/          # 头文件目录
│   └── tglog/
│       ├── tglog.h   # 主头文件(包含所有其他头文件)
│       ├── client.h
│       ├── conf.h
│       ├── def.h
│       ├── err.h
│       ├── metrics.h
│       └── debug_logger.h
└── lib/              # 库文件目录
    └── libtglog.a    # 静态库
```

---

## 快速入门

下面是一个最简单的示例,展示如何发送一条日志:

### 步骤 1: 创建源文件 `hello_tglog.c`

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "tglog/tglog.h"

// 回调函数:当日志发送成功或失败时被调用
void send_callback(const char *log_payload, int err_code, const char *err_msg, void *user_data) {
    if (err_code == TGLOG_ERR_OK) {
        printf("日志发送成功!\n");
    } else {
        printf("日志发送失败,错误码: %d, 错误信息: %s\n", err_code, err_msg);
    }
}

int main() {
    // 第一步:初始化多线程支持(每个进程只需调用一次)
    tglog_use_thread();

    // 第二步:创建配置对象
    // 参数: 协议类型, 服务器地址, 端口, 调试日志回调(可为NULL), 调试日志回调(可为NULL), 用户数据(可为NULL)
    tglog_conf *conf = tglog_conf_create(
        TGLOG_NETWORK_TCP,           // 使用 TCP 协议
        "127.0.0.1",                 // 日志服务器地址
        9999,                        // 日志服务器端口
        NULL,                        // 不使用调试日志
        NULL,
        NULL
    );

    if (conf == NULL) {
        fprintf(stderr, "创建配置失败!\n");
        return 1;
    }

    // 第三步:创建客户端
    tglog_client *client = tglog_client_create(conf);
    if (client == NULL) {
        fprintf(stderr, "创建客户端失败!\n");
        tglog_conf_destroy(conf);
        return 1;
    }

    // 第四步:准备日志数据
    // TGLog 日志格式: logname|field1|field2|...\n
    const char *log_data = "userlogin|12345|alice|10\n";

    // 第五步:发送日志
    int ret = tglog_client_send(
        client,                      // 客户端对象
        log_data,                    // 日志内容
        send_callback,               // 发送结果回调函数
        NULL,                        // 传递给回调的用户数据
        TGLOG_MSG_F_COPY            // 标志:让 SDK 复制日志内容
    );

    if (ret != 0) {
        fprintf(stderr, "发送日志失败,返回值: %d\n", ret);
    }

    // 第六步:等待日志发送完成
    // 因为是异步发送,需要等待一段时间让后台线程完成工作
    printf("等待日志发送...\n");
    sleep(2);

    // 第七步:清理资源
    tglog_client_destroy(client);
    tglog_conf_destroy(conf);

    printf("程序结束\n");
    return 0;
}
```

### 步骤 2: 编译程序

```bash
gcc hello_tglog.c -o hello_tglog \
    -I/path/to/sdk-c/dist/include \
    -L/path/to/sdk-c/dist/lib \
    -ltglog -levent -levent_pthreads -lck -ljemalloc -lpthread -lm
```

**编译参数说明:**
- `-I`: 指定头文件目录
- `-L`: 指定库文件目录
- `-ltglog`: 链接 tglog 库
- `-levent`: 链接 libevent 库
- `-levent_pthreads`: 链接 libevent 多线程支持
- `-lck`: 链接 concurrencykit 库
- `-ljemalloc`: 链接 jemalloc 库
- `-lpthread`: 链接 pthread 线程库
- `-lm`: 链接数学库

### 步骤 3: 运行程序

```bash
# 确保库文件路径在 LD_LIBRARY_PATH 中
export LD_LIBRARY_PATH=/path/to/sdk-c/dist/lib:$LD_LIBRARY_PATH

# 运行程序
./hello_tglog
```

**预期输出:**
```
等待日志发送...
日志发送成功!
程序结束
```

---

## 详细配置说明

### 配置对象的创建

TGLog 使用 `tglog_conf` 结构体来保存所有配置参数。

#### 创建配置对象

```c
tglog_conf *tglog_conf_create(
    tglog_network network,      // 网络协议
    const char *host,           // 服务器地址
    int port,                   // 服务器端口
    can_log_func can_log,       // 调试日志级别判断函数(可为NULL)
    write_log_func write_log,   // 调试日志输出函数(可为NULL)
    void *user_data            // 传递给调试日志函数的用户数据(可为NULL)
);
```

**参数说明:**

1. **network**: 网络协议类型
   - `TGLOG_NETWORK_UDP`: 使用 UDP 协议(高性能,但可能丢包)
   - `TGLOG_NETWORK_TCP`: 使用 TCP 协议(可靠,推荐)

2. **host**: 日志服务器的 IP 地址或域名
   - 示例: `"192.168.1.100"` 或 `"log.example.com"`

3. **port**: 日志服务器的端口号
   - 示例: `9999`

4. **can_log / write_log**: 调试日志回调函数(可选)
   - 用于输出 SDK 内部的调试信息
   - 如果不需要调试,可以传 `NULL`

#### 设置默认配置

创建配置对象后,可以调用 `tglog_conf_set_default()` 设置所有参数为默认值:

```c
tglog_conf *conf = tglog_conf_create(TGLOG_NETWORK_TCP, "127.0.0.1", 9999, NULL, NULL, NULL);
tglog_conf_set_default(conf);  // 设置默认值
```

### 重要配置参数

配置对象创建后,你可以直接修改其成员变量来调整行为:

#### 1. 工作线程数量

```c
conf->worker_num = 8;  // 默认值: 8
```

- **说明**: 后台工作线程的数量,每个线程独立处理日志发送
- **建议**: 根据 CPU 核心数调整,通常设置为 4-16
- **影响**: 更多线程可以提高吞吐量,但也会增加 CPU 和内存开销

#### 2. 批量发送配置

```c
// 批量发送的最大消息数
conf->batching_max_messages = 50;  // 默认值: 50

// 批量发送的最大字节数
conf->batching_max_size = 32 * 1024;  // 默认值: 32KB

// 批量发送的最大延迟(毫秒)
conf->batching_max_publish_delay = 10;  // 默认值: 10ms
```

- **说明**: 为了提高效率,SDK 会将多条日志合并成一个批次发送
- **触发条件**: 满足以下任一条件时发送批次
  - 消息数达到 `batching_max_messages`
  - 总字节数达到 `batching_max_size`
  - 距离上次发送超过 `batching_max_publish_delay` 毫秒

#### 3. 连接配置

```c
// 连接超时时间(毫秒)
conf->conn_timeout = 3000;  // 默认值: 3000ms (3秒)

// 连接最大生命周期(秒)
conf->max_conn_lifetime = 300;  // 默认值: 300秒(5分钟)

// 发送超时时间(毫秒)
conf->send_timeout = 30000;  // 默认值: 30000ms (30秒)

// 最大重试次数
conf->max_retries = 3;  // 默认值: 3次

// 停止超时时间(毫秒)
conf->stop_timeout = 3000;  // 默认值: 3000ms (3秒)
```

**注意**: `max_conn_lifetime` 设置为 0 或负数时将使用长连接。只有当服务器直接挂到 DNS 上时才建议使用长连接,否则(如挂在 CLB 上)会导致流量不均衡。

#### 4. 消息队列配置

```c
// 每个工作线程的最大待发送消息数
conf->max_pending_messages = 409600;  // 默认值: 409600
```

- **说明**: 如果待发送的消息超过此限制,新的发送请求会失败
- **建议**: 根据内存大小和日志量调整
- **注意**: 实际值会向上取到最接近的 2 的幂次方

#### 5. DNS 更新间隔

```c
// DNS 解析结果的更新间隔(秒)
conf->update_interval = 60;  // 默认值: 60秒
```

- **说明**: 如果服务器地址是域名,SDK 会定期重新解析 DNS

#### 6. Socket 缓冲区配置

```c
// Socket 发送缓冲区大小(字节)
conf->socket_send_buffer_size = 64 * 1024 * 1024;  // 默认值: 64MB

// Socket 接收缓冲区大小(字节)
conf->socket_recv_buffer_size = 16 * 1024;  // 默认值: 16KB

// 写入高水位标记(字节)
conf->write_high_watermark = 32 * 1024 * 1024;  // 默认值: 32MB
```

- **write_high_watermark**: 当 bufferevent 中缓冲的数据大小超过此值时,停止向该连接写入数据

#### 7. 内存池配置

TGLog 使用内存池来减少内存分配开销。你可以配置不同大小的内存池容量:

```c
// 消息对象池容量
conf->message_pool_cap = 1000000;  // 默认值: 1000000

// 不同大小的 payload 内存池容量
conf->payload_pool_256b_cap = 20000;   // 256字节池,默认: 20000
conf->payload_pool_512b_cap = 20000;   // 512字节池,默认: 20000
conf->payload_pool_1kb_cap = 20000;    // 1KB池,默认: 20000
conf->payload_pool_2kb_cap = 10000;    // 2KB池,默认: 10000
conf->payload_pool_4kb_cap = 10000;    // 4KB池,默认: 10000
conf->payload_pool_8kb_cap = 10000;    // 8KB池,默认: 10000
conf->payload_pool_16kb_cap = 5000;    // 16KB池,默认: 5000
conf->payload_pool_32kb_cap = 5000;    // 32KB池,默认: 5000
conf->payload_pool_48kb_cap = 1000;    // 48KB池,默认: 1000
conf->payload_pool_64kb_cap = 1000;    // 64KB池,默认: 1000
```

### 调试日志配置

如果你想查看 SDK 内部的运行情况,可以配置调试日志:

```c
#include <stdio.h>
#include <stdarg.h>
#include "tglog/tglog.h"

// 判断是否输出某个级别的日志
bool can_log(tglog_level level, void *user_data) {
    // TGLOG_LEVEL_DEBUG = 0
    // TGLOG_LEVEL_INFO  = 1
    // TGLOG_LEVEL_WARN  = 2
    // TGLOG_LEVEL_ERROR = 3
    // TGLOG_LEVEL_FATAL = 4
    return level >= TGLOG_LEVEL_INFO;  // 只输出 INFO 及以上级别
}

// 输出日志内容
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

    // 创建配置时传入调试日志回调
    tglog_conf *conf = tglog_conf_create(
        TGLOG_NETWORK_TCP,
        "127.0.0.1",
        9999,
        can_log,      // 传入级别判断函数
        write_log,    // 传入日志输出函数
        NULL
    );

    // ... 后续代码
}
```

**注意**: 调试日志回调函数必须是线程安全的,因为它们会在不同的工作线程中被调用。

---

## API 参考

### 初始化函数

#### tglog_use_thread()

```c
void tglog_use_thread(void);
```

**说明**: 初始化多线程支持,必须在创建客户端之前调用,且每个进程只需调用一次。

**注意**: 此函数内部会调用 `evthread_use_pthreads()`,如果你的程序已经调用过该函数,则不需要调用 `tglog_use_thread()`。

**示例**:
```c
int main() {
    tglog_use_thread();  // 在 main 函数开始时调用
    // ... 创建客户端
}
```

### 配置管理函数

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

**说明**: 创建配置对象并设置为默认值。

**返回值**: 成功返回配置对象指针,失败返回 `NULL`。

#### tglog_conf_set_default()

```c
void tglog_conf_set_default(tglog_conf *conf);
```

**说明**: 将配置对象的所有参数设置为默认值。

#### tglog_conf_set_server()

```c
bool tglog_conf_set_server(
    tglog_conf *conf,
    tglog_network network,
    const char *host,
    int port
);
```

**说明**: 设置服务器地址和协议。

**返回值**: 成功返回 `true`,失败返回 `false`。

#### tglog_conf_set_debug_logger()

```c
void tglog_conf_set_debug_logger(
    tglog_conf *conf,
    can_log_func can_log,
    write_log_func write_log,
    void *user_data
);
```

**说明**: 设置调试日志回调函数。

**注意**: 回调函数必须是线程安全的。

#### tglog_conf_destroy()

```c
void tglog_conf_destroy(tglog_conf *conf);
```

**说明**: 销毁配置对象,释放内存。

**注意**: 配置对象在创建客户端后就可以销毁,客户端会保存配置的副本。

### 客户端管理函数

#### tglog_client_create()

```c
tglog_client *tglog_client_create(const tglog_conf *conf);
```

**说明**: 创建客户端对象。

**返回值**: 成功返回客户端指针,失败返回 `NULL`。

**注意**: 创建客户端后,后台线程会自动启动并连接服务器。

#### tglog_client_destroy()

```c
void tglog_client_destroy(tglog_client *client);
```

**说明**: 销毁客户端对象,停止后台线程,关闭连接,释放资源。

**注意**:
- 此函数会等待所有待发送的日志发送完成(最多等待 `stop_timeout` 毫秒)
- 销毁后,所有未完成的发送请求的回调会被调用,错误码为 `TGLOG_ERR_PRODUCER_CLOSED`

### 日志发送函数

#### tglog_client_send()

```c
int tglog_client_send(
    tglog_client *client,
    const char *log_payload,
    tglog_send_callback callback,
    void *user_data,
    char msg_flags
);
```

**说明**: 发送一条日志。

**参数**:
- `client`: 客户端对象
- `log_payload`: 日志内容,必须以 `\n` 结尾
- `callback`: 发送结果回调函数(可为 `NULL`)
- `user_data`: 传递给回调函数的用户数据(可为 `NULL`)
- `msg_flags`: 消息标志,控制内存管理方式

**返回值**: 成功返回 0,失败返回非 0 错误码。

**日志格式**:
```
<log_name>|v1|v2|v3|...|vn\n
```
例如:
```
login|2025-06-06 12:00:00|alice|password\n
```

**消息标志 (msg_flags)**:

1. **TGLOG_MSG_F_COPY** (推荐)
   ```c
   tglog_client_send(client, log_data, callback, NULL, TGLOG_MSG_F_COPY);
   ```
   - SDK 会复制日志内容
   - 调用返回后,你可以立即释放或修改 `log_payload`
   - 最安全,最简单,推荐使用

2. **TGLOG_MSG_F_FREE**
   ```c
   char *log_data = malloc(100);
   sprintf(log_data, "login|2025-06-06 12:00:00|alice\n");
   tglog_client_send(client, log_data, callback, NULL, TGLOG_MSG_F_FREE);
   // 不要 free(log_data),SDK 会自动释放
   ```
   - `log_payload` 必须是通过 `malloc()` 或 `calloc()` 分配的内存
   - SDK 发送完成后会自动调用 `free()` 释放
   - 适合需要动态构造日志的场景

3. **TGLOG_MSG_F_NONE**
   ```c
   static const char *log_data = "login|2025-06-06 12:00:00|alice\n";
   tglog_client_send(client, log_data, callback, NULL, TGLOG_MSG_F_NONE);
   ```
   - SDK 不复制也不释放,零拷贝
   - 你必须保证 `log_payload` 在发送完成前一直有效
   - 适合静态字符串或全局缓冲区
   - 高性能,但使用复杂,容易出错

**回调函数原型**:
```c
typedef void (*tglog_send_callback)(
    const char *log_payload,
    int err_code,
    const char *err_msg,
    void *user_data
);
```

- `log_payload`: 发送的日志内容
- `err_code`: 错误码,`TGLOG_ERR_OK` (0) 表示成功
- `err_msg`: 错误消息字符串
- `user_data`: 发送时传入的用户数据

**注意**:
- 回调函数在后台工作线程中执行,必须是线程安全的
- 不要在回调中执行耗时操作,会影响性能
- 回调中可以访问 `user_data`,但要确保数据在回调执行时仍然有效

### 监控函数

#### tglog_client_get_msg_queue_size()

```c
size_t tglog_client_get_msg_queue_size(tglog_client *client);
```

**说明**: 获取当前待发送的消息数量。

**返回值**: 待发送消息数量。

#### tglog_client_get_msg_queue_capacity()

```c
size_t tglog_client_get_msg_queue_capacity(tglog_client *client);
```

**说明**: 获取消息队列的总容量。

**返回值**: 队列容量(等于 `worker_num * max_pending_messages`)。

#### tglog_client_print_metrics()

```c
int tglog_client_print_metrics(
    tglog_client *client,
    char *buffer,
    size_t size,
    bool json
);
```

**说明**: 获取性能统计信息。

**参数**:
- `client`: 客户端对象
- `buffer`: 输出缓冲区,至少 4096 字节(JSON 格式需要 8192 字节)
- `size`: 缓冲区大小
- `json`: `true` 输出 JSON 格式,`false` 输出文本格式

**返回值**: 成功返回写入的字节数,失败返回 -1。

**示例**:
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

**说明**: 获取内存池统计信息。

**参数**: 同 `tglog_client_print_metrics()`。

---

## 完整示例

下面是一个更完整的示例,展示了实际游戏服务器中的典型用法:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
#include "tglog/tglog.h"

// 全局统计
atomic_int g_success_count = 0;
atomic_int g_failed_count = 0;

// 发送结果回调
void on_send_result(const char *log_payload, int err_code, const char *err_msg, void *user_data) {
    if (err_code == TGLOG_ERR_OK) {
        atomic_fetch_add(&g_success_count, 1);
    } else {
        atomic_fetch_add(&g_failed_count, 1);
        fprintf(stderr, "发送失败: %s, 错误码: %d, 错误信息: %s\n",
                log_payload, err_code, err_msg);
    }
}

// 调试日志:判断是否输出某个级别
bool can_log(tglog_level level, void *user_data) {
    return level >= TGLOG_LEVEL_WARN;  // 只输出警告和错误
}

// 调试日志:输出日志内容
void write_log(void *user_data, tglog_level level, const char *file,
               const int line, const char *func, const char *format, ...) {
    const char *level_str[] = {"DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
    printf("[%s] %s:%d %s() - ", level_str[level], file, line, func);

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    printf("\n");
}

// 模拟游戏逻辑:玩家登录
void player_login(tglog_client *client, int user_id, const char *username) {
    char log_buffer[256];
    snprintf(log_buffer, sizeof(log_buffer),
             "userlogin|%ld|%d|%s\n",
             time(NULL), user_id, username);

    tglog_client_send(client, log_buffer, on_send_result, NULL, TGLOG_MSG_F_COPY);
}

// 模拟游戏逻辑:玩家购买道具
void player_buy_item(tglog_client *client, int user_id, int item_id, int price) {
    char log_buffer[256];
    snprintf(log_buffer, sizeof(log_buffer),
             "itembuy|%ld|%d|%d|%d\n",
             time(NULL), user_id, item_id, price);

    tglog_client_send(client, log_buffer, on_send_result, NULL, TGLOG_MSG_F_COPY);
}

// 模拟游戏逻辑:玩家战斗
void player_battle(tglog_client *client, int user_id, int monster_id, bool win) {
    char log_buffer[256];
    snprintf(log_buffer, sizeof(log_buffer),
             "battle|%ld|%d|%d|%s\n",
             time(NULL), user_id, monster_id, win ? "win" : "lose");

    tglog_client_send(client, log_buffer, on_send_result, NULL, TGLOG_MSG_F_COPY);
}

// 工作线程:模拟多个玩家同时游戏
void *worker_thread(void *arg) {
    tglog_client *client = (tglog_client *)arg;
    int thread_id = (int)(long)pthread_self();

    for (int i = 0; i < 1000; i++) {
        int user_id = thread_id + i;

        // 模拟玩家登录
        char username[32];
        snprintf(username, sizeof(username), "player_%d", user_id);
        player_login(client, user_id, username);

        // 模拟玩家购买道具
        player_buy_item(client, user_id, 1001, 100);

        // 模拟玩家战斗
        player_battle(client, user_id, 2001, i % 2 == 0);

        // 模拟游戏帧间隔
        usleep(1000);  // 1ms
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    printf("=== TGLog 多线程版本完整示例 ===\n\n");

    // 第一步:初始化多线程支持
    tglog_use_thread();
    printf("✓ 已初始化多线程支持\n");

    // 第二步:创建配置
    const char *server_host = "127.0.0.1";
    int server_port = 9999;

    if (argc >= 3) {
        server_host = argv[1];
        server_port = atoi(argv[2]);
    }

    printf("✓ 日志服务器: %s:%d\n", server_host, server_port);

    tglog_conf *conf = tglog_conf_create(
        TGLOG_NETWORK_TCP,
        server_host,
        server_port,
        can_log,
        write_log,
        NULL
    );

    if (conf == NULL) {
        fprintf(stderr, "✗ 创建配置失败!\n");
        return 1;
    }

    // 第三步:调整配置参数
    conf->worker_num = 4;                      // 使用 4 个工作线程
    conf->batching_max_messages = 100;         // 每批最多 100 条消息
    conf->batching_max_publish_delay = 5;      // 最多延迟 5ms
    conf->send_timeout = 10000;                // 发送超时 10 秒

    printf("✓ 配置参数:\n");
    printf("  - 工作线程数: %d\n", conf->worker_num);
    printf("  - 批量消息数: %d\n", conf->batching_max_messages);
    printf("  - 批量延迟: %d 毫秒\n", conf->batching_max_publish_delay);

    // 第四步:创建客户端
    tglog_client *client = tglog_client_create(conf);
    if (client == NULL) {
        fprintf(stderr, "✗ 创建客户端失败!\n");
        tglog_conf_destroy(conf);
        return 1;
    }
    printf("✓ 客户端创建成功\n\n");

    // 配置对象可以销毁了
    tglog_conf_destroy(conf);

    // 第五步:启动多个工作线程模拟游戏服务器
    printf("开始模拟游戏服务器运行...\n");

    int num_threads = 4;
    pthread_t threads[num_threads];

    for (int i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, worker_thread, client);
    }

    // 第六步:定期输出统计信息
    for (int i = 0; i < 10; i++) {
        sleep(1);

        size_t queue_size = tglog_client_get_msg_queue_size(client);
        size_t queue_capacity = tglog_client_get_msg_queue_capacity(client);
        int success = atomic_load(&g_success_count);
        int failed = atomic_load(&g_failed_count);

        printf("[%d秒] 队列: %zu/%zu, 成功: %d, 失败: %d\n",
               i + 1, queue_size, queue_capacity, success, failed);
    }

    // 第七步:等待所有工作线程完成
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("\n所有工作线程已完成\n");

    // 第八步:输出详细统计
    printf("\n=== 性能统计 ===\n");
    char metrics_buffer[4096];
    if (tglog_client_print_metrics(client, metrics_buffer, sizeof(metrics_buffer), false) > 0) {
        printf("%s\n", metrics_buffer);
    }

    printf("\n=== 内存池统计 ===\n");
    char mempool_buffer[4096];
    if (tglog_client_print_mem_pool_stats(client, mempool_buffer, sizeof(mempool_buffer), false) > 0) {
        printf("%s\n", mempool_buffer);
    }

    // 第九步:清理资源
    printf("\n正在关闭客户端...\n");
    tglog_client_destroy(client);

    // 最终统计
    int total_success = atomic_load(&g_success_count);
    int total_failed = atomic_load(&g_failed_count);
    printf("\n=== 最终统计 ===\n");
    printf("成功: %d\n", total_success);
    printf("失败: %d\n", total_failed);
    printf("总计: %d\n", total_success + total_failed);

    printf("\n程序结束\n");
    return 0;
}
```

### 编译运行

```bash
# 编译
gcc -o game_server_example game_server_example.c \
    -I/path/to/sdk-c/dist/include \
    -L/path/to/sdk-c/dist/lib \
    -ltglog -levent -levent_pthreads -lck -ljemalloc -lpthread -lm

# 运行(使用默认服务器地址)
./game_server_example

# 运行(指定服务器地址)
./game_server_example 192.168.1.100 9999
```

---

## 性能优化

### 1. 调整工作线程数

```c
conf->worker_num = 8;  // 根据 CPU 核心数调整
```

- **建议**: 设置为 CPU 核心数的 0.5 到 1 倍
- **影响**: 更多线程可以提高并发处理能力,但也会增加开销

### 2. 优化批量发送参数

```c
// 高吞吐量场景
conf->batching_max_messages = 100;           // 增大批量大小
conf->batching_max_publish_delay = 20;       // 增大延迟容忍度

// 低延迟场景
conf->batching_max_messages = 10;            // 减小批量大小
conf->batching_max_publish_delay = 1;        // 减小延迟
```

### 3. 使用零拷贝模式

如果你能保证日志数据的生命周期,使用 `TGLOG_MSG_F_NONE` 可以避免内存拷贝:

```c
// 使用静态缓冲区或全局缓冲区
static char log_buffers[1000][256];
static atomic_int buffer_index = 0;

void send_log_zero_copy(tglog_client *client, int user_id) {
    int idx = atomic_fetch_add(&buffer_index, 1) % 1000;
    snprintf(log_buffers[idx], 256, "userlogin|%ld|%d\n", time(NULL), user_id);

    // 零拷贝发送
    tglog_client_send(client, log_buffers[idx], NULL, NULL, TGLOG_MSG_F_NONE);
}
```

**注意**: 必须确保缓冲区在发送完成前不被修改。

### 4. 调整内存池大小

根据你的日志大小分布,调整内存池配置:

```c
// 如果大部分日志都是 512 字节左右
conf->payload_pool_512b_cap = 40000;  // 增大 512字节池的容量
```

### 5. 监控队列大小

定期检查队列大小,避免积压:

```c
size_t queue_size = tglog_client_get_msg_queue_size(client);
size_t queue_capacity = tglog_client_get_msg_queue_capacity(client);

if (queue_size > queue_capacity * 0.8) {
    // 队列快满了,考虑降低日志发送频率或增大队列容量
    printf("警告: 队列使用率 %.1f%%\n",
           (double)queue_size / queue_capacity * 100);
}
```

---

## 常见问题

### Q1: 程序退出时日志没有全部发送怎么办?

**A**: 在调用 `tglog_client_destroy()` 之前,等待队列清空:

```c
// 等待队列清空
while (tglog_client_get_msg_queue_size(client) > 0) {
    usleep(10000);  // 等待 10ms
}

// 或者设置更长的停止超时时间
conf->stop_timeout = 10000;  // 等待 10 秒

tglog_client_destroy(client);
```

### Q2: 如何判断日志发送成功?

**A**: 使用回调函数:

```c
void my_callback(const char *log_payload, int err_code, const char *err_msg, void *user_data) {
    if (err_code == TGLOG_ERR_OK) {
        printf("发送成功\n");
    } else {
        printf("发送失败: %d - %s\n", err_code, err_msg);
    }
}

tglog_client_send(client, log_data, my_callback, NULL, TGLOG_MSG_F_COPY);
```

### Q3: 回调函数在哪个线程执行?

**A**: 回调函数在后台工作线程中执行,不是在调用 `tglog_client_send()` 的线程。因此:
- 回调函数必须是线程安全的
- 不要在回调中访问非线程安全的数据结构
- 不要在回调中执行耗时操作

### Q4: 可以在回调中再次调用 tglog_client_send() 吗?

**A**: 可以,但不推荐。如果必须这样做,要小心避免递归调用导致的栈溢出。

### Q5: 如何处理服务器断线?

**A**: SDK 会自动重连,你不需要做任何处理。如果重连失败,发送请求会返回错误,回调会收到错误码。

### Q6: UDP 和 TCP 该选哪个?

**A**:
- **TCP**: 可靠,有序,推荐用于重要日志
- **UDP**: 高性能,但可能丢包,适合对可靠性要求不高的场景

### Q7: 为什么调用 tglog_client_send() 返回 0,但回调收到错误?

**A**: `tglog_client_send()` 返回 0 只表示日志成功加入发送队列,不代表发送成功。实际发送结果需要通过回调函数获取。

### Q8: 如何减少内存占用?

**A**:
- 减少 `worker_num`
- 减少 `max_pending_messages`
- 减少内存池的容量配置

### Q9: 日志格式有什么要求?

**A**:
- 必须以 `\n` 结尾
- 推荐格式: `logname|v1|v2|v3|...\n`
- 字段值中不要包含 `\n`

### Q10: 如何调试 SDK 内部问题?

**A**: 配置调试日志:

```c
bool can_log(tglog_level level, void *user_data) {
    return level >= TGLOG_LEVEL_DEBUG;  // 输出所有级别
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

## 错误处理

### 常见错误码

```c
#define TGLOG_ERR_OK                    0       // 成功
#define TGLOG_ERR_SEND_TIMEOUT          10001   // 发送超时
#define TGLOG_ERR_SEND_FAILED           10002   // 发送失败
#define TGLOG_ERR_PRODUCER_CLOSED       10003   // 客户端已关闭
#define TGLOG_ERR_SEND_QUEUE_IS_FULL    10004   // 发送队列已满
#define TGLOG_ERR_NO_AVAILABLE_CONN     10005   // 没有可用连接
#define TGLOG_ERR_NEW_CONN_FAILED       10006   // 创建连接失败
#define TGLOG_ERR_CONN_WRITE_FAILED     10007   // 连接写入失败
#define TGLOG_ERR_CONN_READ_FAILED      10008   // 连接读取失败
#define TGLOG_ERR_LOG_TOO_LONG          10009   // 日志过长
#define TGLOG_ERR_BAD_LOG               10010   // 日志格式错误
#define TGLOG_ERR_BATCH_SIZE_EXCEEDED   10011   // 批次大小超限
#define TGLOG_ERR_MEMORY_ALLOC_FAILED   10012   // 内存分配失败
```

### 错误处理示例

```c
void on_send_result(const char *log_payload, int err_code, const char *err_msg, void *user_data) {
    switch (err_code) {
        case TGLOG_ERR_OK:
            // 发送成功,无需处理
            break;

        case TGLOG_ERR_SEND_TIMEOUT:
            fprintf(stderr, "发送超时: %s\n", err_msg);
            // 可以考虑重试或记录到本地文件
            break;

        case TGLOG_ERR_SEND_QUEUE_IS_FULL:
            fprintf(stderr, "发送队列已满: %s\n", err_msg);
            // 考虑降低日志发送频率或增大队列容量
            break;

        case TGLOG_ERR_NO_AVAILABLE_CONN:
            fprintf(stderr, "没有可用连接: %s\n", err_msg);
            // SDK 会自动重连,可以暂时缓存日志
            break;

        case TGLOG_ERR_PRODUCER_CLOSED:
            fprintf(stderr, "客户端已关闭: %s\n", err_msg);
            // 程序正在退出,忽略此错误
            break;

        case TGLOG_ERR_LOG_TOO_LONG:
            fprintf(stderr, "日志过长: %s\n", err_msg);
            // 需要缩短日志内容
            break;

        default:
            fprintf(stderr, "未知错误: %d - %s\n", err_code, err_msg);
            break;
    }
}
```

### 获取错误消息

```c
const char *tglog_get_err_msg(int code);
```

**说明**: 根据错误码获取错误消息字符串。

**示例**:
```c
int ret = tglog_client_send(client, log_data, NULL, NULL, TGLOG_MSG_F_COPY);
if (ret != 0) {
    const char *err_msg = tglog_get_err_msg(ret);
    fprintf(stderr, "发送失败: %s\n", err_msg);
}
```

---

## 总结

恭喜你完成了 TGLog 多线程版本的学习!现在你应该能够:

1. ✓ 理解 TGLog 的基本概念和使用场景
2. ✓ 正确安装依赖和编译 SDK
3. ✓ 创建配置和客户端
4. ✓ 发送日志并处理回调
5. ✓ 调整配置参数优化性能
6. ✓ 监控运行状态和统计信息
7. ✓ 处理错误和异常情况

### 下一步

- 阅读 `tglog/DESIGN.md` 了解内部实现原理
- 查看 `test/tglog_test.c` 学习更多高级用法
- 根据实际业务需求调整配置参数
- 在测试环境充分测试后再部署到生产环境

### 获取帮助

- 查看源码中的注释和文档
- 参考 `test` 目录下的示例代码
- 使用调试日志功能排查问题

祝你使用愉快!
