# TGLog 单线程版本使用教程

> 说明：此教程由AI生成，仅供参考，请以实际代码为准。示例代码编译时使用的是动态链接，实际项目建议静态链接。

## 目录
1. [简介](#简介)
2. [什么是 TGLog-ST](#什么是-tglog-st)
3. [环境准备](#环境准备)
4. [编译安装](#编译安装)
5. [快速入门](#快速入门)
6. [详细配置说明](#详细配置说明)
7. [API 参考](#api-参考)
8. [完整示例](#完整示例)
9. [性能优化](#性能优化)
10. [常见问题](#常见问题)
11. [错误处理](#错误处理)
12. [总结](#总结)

---

## 简介

欢迎使用 TGLog 单线程版本!这是一个专为游戏服务器优化的日志上报 C 语言 SDK。本教程将手把手教你如何使用这个库,即使你是 C 语言新手也能轻松上手。

### 适用场景
- 单线程游戏服务器(如基于帧循环的游戏逻辑)
- 需要精确控制执行时机的场景
- 对延迟敏感的实时游戏
- 不希望引入多线程复杂性的项目

---

## 什么是 TGLog-ST

TGLog-ST (Single-Threaded) 是 TGLog 的单线程版本。它可以帮助你:
- 将游戏中的用户行为数据发送到日志服务器
- 支持 TCP 和 UDP 两种协议
- 自动批量发送,提高效率
- 支持断线重连
- 提供发送成功/失败的回调通知

### 单线程版本特点
- **完全控制**: 所有操作在你的主线程中执行,通过 `tglog_client_update()` 驱动
- **低延迟**: 没有线程切换开销,适合帧率敏感的游戏
- **简单安全**: 无需考虑线程同步和锁的问题
- **可预测**: 回调在你调用 `update()` 时执行,不会在意外时刻触发
- **高性能**: 虽然是单线程,仍可达到 10-20 万条/秒的吞吐量

---

## 环境准备

### 系统要求
- Linux 操作系统(推荐 CentOS 7+ 或 Ubuntu 18.04+)
- GCC 4.8 或更高版本
- CMake 3.10 或更高版本(可选)

### 依赖库
TGLog-ST 依赖以下第三方库:
- **libevent**: 事件驱动库
- **concurrencykit (libck)**: 无锁数据结构库
- **jemalloc**: 高性能内存分配器

**注意**: 单线程版本不需要 `libevent_pthreads`,比多线程版本依赖更少。

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

### 2. 编译 TGLog-ST 库
```bash
# 使用 Makefile 编译
make tglog-st

# 或者使用 CMake
mkdir build && cd build
cmake ..
make tglog-st
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
    └── libtglog-st.a # 静态库
```

---

## 快速入门

下面是一个最简单的示例,展示如何发送一条日志:

### 步骤 1: 创建源文件 `hello_tglog_st.c`

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "tglog-st/tglog.h"

// 回调函数:当日志发送成功或失败时被调用
void send_callback(const char *log_payload, int err_code, const char *err_msg, void *user_data) {
    if (err_code == TGLOG_ERR_OK) {
        printf("日志发送成功!\n");
    } else {
        printf("日志发送失败,错误码: %d, 错误信息: %s\n", err_code, err_msg);
    }
}

int main() {
    // 第一步:创建配置对象
    // 注意:单线程版本不需要调用 tglog_use_thread()
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

    // 第二步:创建客户端
    tglog_client *client = tglog_client_create(conf);
    if (client == NULL) {
        fprintf(stderr, "创建客户端失败!\n");
        tglog_conf_destroy(conf);
        return 1;
    }

    // 第三步:准备日志数据
    // TGLog 日志格式: logname|field1|field2|...\n
    const char *log_data = "userlogin|12345|alice|10\n";

    // 第四步:发送日志
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

    // 第五步:驱动客户端处理(关键!单线程版本特有)
    // 必须定期调用 update() 来处理网络 I/O 和回调
    printf("等待日志发送...\n");
    for (int i = 0; i < 100; i++) {
        tglog_client_update(client);  // 驱动客户端工作
        usleep(10000);                // 等待 10ms
    }

    // 第六步:清理资源
    tglog_client_destroy(client);
    tglog_conf_destroy(conf);

    printf("程序结束\n");
    return 0;
}
```

### 步骤 2: 编译程序

```bash
gcc hello_tglog_st.c -o hello_tglog_st \
    -I/path/to/sdk-c/dist/include \
    -L/path/to/sdk-c/dist/lib \
    -ltglog-st -levent -ljemalloc -lpthread -latomic -lm
```

**编译参数说明:**
- `-I`: 指定头文件目录
- `-L`: 指定库文件目录
- `-ltglog-st`: 链接 tglog-st 库(注意是 tglog-st,不是 tglog)
- `-levent`: 链接 libevent 库
- `-ljemalloc`: 链接 jemalloc 库
- `-lpthread`: 链接 pthread 线程库
- `-latomic`: 链接原子操作库（128-bit CAS 支持）
- `-lm`: 链接数学库

**注意**: 单线程版本不需要链接 `-levent_pthreads`。

### 步骤 3: 运行程序

```bash
# 确保库文件路径在 LD_LIBRARY_PATH 中
export LD_LIBRARY_PATH=/path/to/sdk-c/dist/lib:$LD_LIBRARY_PATH

# 运行程序
./hello_tglog_st
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

TGLog-ST 使用 `tglog_conf` 结构体来保存所有配置参数。

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

#### 1. Update 调用配置(单线程版本特有)

```c
// 每次 update() 调用的最大执行时间(微秒)
conf->max_update_us = 200;  // 默认值: 200 微秒

// 每次 update() 调用最多处理的消息数
conf->max_messages_per_update = 1000;  // 默认值: 1000
```

- **说明**: 这两个参数控制 `tglog_client_update()` 的行为
- **max_update_us**: 限制单次 update 的执行时间,避免阻塞游戏主循环太久
- **max_messages_per_update**: 限制单次处理的消息数,配合时间限制使用
- **建议**:
  - 如果游戏帧率是 60 FPS,每帧约 16ms,可以设置 `max_update_us = 500` (0.5ms)
  - 如果日志量很大,可以增大 `max_messages_per_update`

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
// 最大待发送消息数
conf->max_pending_messages = 1000000;  // 默认值: 1000000
```

- **说明**: 如果待发送的消息超过此限制,新的发送请求会失败
- **建议**: 根据内存大小和日志量调整
- **注意**:
  - 实际值会向上取到最接近的 2 的幂次方
  - 单线程版本的默认值(100万)比多线程版本(每个 worker 40.96万)更大

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

TGLog-ST 使用内存池来减少内存分配开销。你可以配置不同大小的内存池容量:

```c
// 消息对象池容量
conf->message_pool_cap = 100000;  // 默认值: 100000

// 不同大小的 payload 内存池容量
conf->payload_pool_256b_cap = 2000;   // 256字节池,默认: 2000
conf->payload_pool_512b_cap = 2000;   // 512字节池,默认: 2000
conf->payload_pool_1kb_cap = 2000;    // 1KB池,默认: 2000
conf->payload_pool_2kb_cap = 1000;    // 2KB池,默认: 1000
conf->payload_pool_4kb_cap = 1000;    // 4KB池,默认: 1000
conf->payload_pool_8kb_cap = 1000;    // 8KB池,默认: 1000
conf->payload_pool_16kb_cap = 500;    // 16KB池,默认: 500
conf->payload_pool_32kb_cap = 500;    // 32KB池,默认: 500
conf->payload_pool_48kb_cap = 100;    // 48KB池,默认: 100
conf->payload_pool_64kb_cap = 100;    // 64KB池,默认: 100
```

**注意**: 单线程版本的内存池默认大小约为多线程版本的 1/10,因为只有一个线程使用。

### 调试日志配置

如果你想查看 SDK 内部的运行情况,可以配置调试日志:

```c
#include <stdio.h>
#include <stdarg.h>
#include "tglog-st/tglog.h"

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
    printf("[TGLOG-ST-%s] %s:%d %s() - ", level_str[level], file, line, func);

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    printf("\n");
}

int main() {
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

---

## API 参考

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

**注意**:
- 创建客户端后,需要定期调用 `tglog_client_update()` 来驱动其工作
- 单线程版本会在内部创建和维护自己的 event_base
- 初始连接会同步创建,有超时机制

#### tglog_client_destroy()

```c
void tglog_client_destroy(tglog_client *client);
```

**说明**: 销毁客户端对象,关闭连接,释放资源。

**注意**:
- 此函数会尝试发送所有待发送的日志(最多等待 `stop_timeout` 毫秒)
- 销毁后,所有未完成的发送请求的回调会被调用,错误码为 `TGLOG_ERR_PRODUCER_CLOSED`

#### tglog_client_update() (单线程版本特有)

```c
int tglog_client_update(tglog_client *client);
```

**说明**: 驱动客户端处理网络 I/O、发送日志、执行回调等操作。

**返回值**: 成功返回处理的消息数量,失败返回负数。

**重要**: 这是单线程版本最关键的函数!
- 必须在游戏主循环中定期调用(建议每帧调用一次)
- 所有的网络操作和回调都在此函数中执行
- 执行时间受 `max_update_us` 和 `max_messages_per_update` 限制
- 函数是非阻塞的,会快速返回

**示例**:
```c
// 游戏主循环
while (game_is_running) {
    // 处理游戏逻辑
    update_game_logic();

    // 驱动日志客户端
    tglog_client_update(client);

    // 渲染
    render_frame();

    // 控制帧率
    sleep_until_next_frame();
}
```

### 日志发送函数

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

**说明**: 发送一条日志。

**参数**:
- `client`: 客户端对象
- `log_payload`: 日志内容,必须以 `\n` 结尾
- `callback`: 发送结果回调函数(可为 `NULL`)
- `user_data`: 传递给回调函数的用户数据(可为 `NULL`)
- `msg_flag`: 消息标志,控制内存管理方式

**返回值**: 成功返回 0,失败返回非 0 错误码。

**日志格式**:
```
<log_name>|v1|v2|v3|...|vn\n
```
例如:
```
login|2025-06-06 12:00:00|alice|password\n
```

**消息标志 (msg_flag)**:

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

**重要**:
- 回调函数在调用 `tglog_client_update()` 时执行,不是在调用 `send()` 时
- 回调在主线程中执行,不需要考虑线程安全
- 可以在回调中安全地访问游戏数据和调用游戏函数

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

**返回值**: 队列容量(等于 `max_pending_messages`)。

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

由于单线程版本的完整示例与多线程版本非常相似,这里只展示关键差异部分。完整示例请参考:
- 源码中的示例: `test-st/tglog_test.c`
- 源码中的性能测试: `test-st/simple_perf_test.c`

### 关键差异

与多线程版本相比,单线程版本的主要差异:

1. **不需要调用 `tglog_use_thread()`**
2. **必须在主循环中调用 `tglog_client_update()`**
3. **回调在主线程执行,不需要线程安全考虑**

### 游戏主循环集成示例

```c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include "tglog-st/tglog.h"

// 全局统计
int g_success_count = 0;
int g_failed_count = 0;

// 发送结果回调
void on_send_result(const char *log_payload, int err_code, const char *err_msg, void *user_data) {
    if (err_code == TGLOG_ERR_OK) {
        g_success_count++;
    } else {
        g_failed_count++;
        fprintf(stderr, "发送失败: %s, 错误码: %d, 错误信息: %s\n",
                log_payload, err_code, err_msg);
    }
}

// 获取当前时间(微秒)
long long get_time_us() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000LL + tv.tv_usec;
}

// 模拟游戏主循环
void game_main_loop(tglog_client *client, int total_frames) {
    const int target_fps = 60;
    const long long frame_time_us = 1000000 / target_fps;

    printf("\n开始游戏主循环 (目标帧率: %d FPS)\n", target_fps);
    printf("每帧时间: %.2f ms\n\n", frame_time_us / 1000.0);

    for (int frame = 0; frame < total_frames; frame++) {
        long long frame_start = get_time_us();

        // === 游戏逻辑处理 ===
        // 模拟每帧有 5 个玩家产生日志
        for (int i = 0; i < 5; i++) {
            int user_id = frame * 5 + i;
            char log_buffer[256];
            snprintf(log_buffer, sizeof(log_buffer),
                     "battle|%ld|%d|%d|win\n",
                     time(NULL), user_id, 2001 + (i % 5));
            tglog_client_send(client, log_buffer, on_send_result, NULL, TGLOG_MSG_F_COPY);
        }

        // === 驱动日志客户端(关键!) ===
        long long update_start = get_time_us();
        int msg_count = tglog_client_update(client);
        long long update_end = get_time_us();
        long long update_time = update_end - update_start;

        // === 定期输出统计信息 ===
        if (frame % 60 == 0) {
            size_t queue_size = tglog_client_get_msg_queue_size(client);
            size_t queue_capacity = tglog_client_get_msg_queue_capacity(client);

            printf("[帧 %d] 队列: %zu/%zu, 成功: %d, 失败: %d, update耗时: %lld us, 处理消息: %d\n",
                   frame, queue_size, queue_capacity,
                   g_success_count, g_failed_count, update_time, msg_count);
        }

        // === 帧率控制 ===
        long long frame_end = get_time_us();
        long long frame_elapsed = frame_end - frame_start;
        long long sleep_time = frame_time_us - frame_elapsed;

        if (sleep_time > 0) {
            usleep(sleep_time);
        }
    }

    printf("\n游戏主循环结束\n");
}

int main(int argc, char *argv[]) {
    printf("=== TGLog 单线程版本完整示例 ===\n\n");

    // 创建配置
    const char *server_host = "127.0.0.1";
    int server_port = 9999;

    if (argc >= 3) {
        server_host = argv[1];
        server_port = atoi(argv[2]);
    }

    tglog_conf *conf = tglog_conf_create(TGLOG_NETWORK_TCP, server_host, server_port, NULL, NULL, NULL);
    if (conf == NULL) {
        fprintf(stderr, "创建配置失败!\n");
        return 1;
    }

    // 调整配置参数
    conf->max_update_us = 500;
    conf->max_messages_per_update = 1000;
    conf->batching_max_messages = 100;
    conf->batching_max_publish_delay = 5;

    // 创建客户端
    tglog_client *client = tglog_client_create(conf);
    if (client == NULL) {
        fprintf(stderr, "创建客户端失败!\n");
        tglog_conf_destroy(conf);
        return 1;
    }
    tglog_conf_destroy(conf);

    // 运行游戏主循环(模拟 10 秒)
    game_main_loop(client, 600);

    // 等待剩余日志发送完成
    printf("\n等待剩余日志发送...\n");
    for (int i = 0; i < 50; i++) {
        tglog_client_update(client);
        size_t queue_size = tglog_client_get_msg_queue_size(client);
        if (queue_size == 0) {
            printf("所有日志已发送完成\n");
            break;
        }
        usleep(100000);
    }

    // 清理资源
    tglog_client_destroy(client);

    printf("\n=== 最终统计 ===\n");
    printf("成功: %d\n", g_success_count);
    printf("失败: %d\n", g_failed_count);
    printf("总计: %d\n", g_success_count + g_failed_count);

    return 0;
}
```

### 编译运行

```bash
# 编译
gcc -o game_server_st_example game_server_st_example.c \
    -I/path/to/sdk-c/dist/include \
    -L/path/to/sdk-c/dist/lib \
    -ltglog-st -levent -ljemalloc -lpthread -latomic -lm

# 运行
./game_server_st_example
```

---

## 性能优化

### 1. 调整 Update 参数

```c
// 高吞吐量场景
conf->max_update_us = 1000;              // 增大到 1ms
conf->max_messages_per_update = 2000;    // 增大到 2000

// 低延迟场景
conf->max_update_us = 200;               // 保持默认 0.2ms
conf->max_messages_per_update = 500;     // 减小到 500
```

### 2. 调整 Update 调用频率

```c
// 策略 1: 每帧调用一次(推荐)
while (running) {
    update_game();
    tglog_client_update(client);
    render();
}

// 策略 2: 每 N 帧调用一次
int frame = 0;
while (running) {
    update_game();
    if (frame % 2 == 0) {
        tglog_client_update(client);
    }
    render();
    frame++;
}

// 策略 3: 根据队列大小动态调整
while (running) {
    update_game();
    size_t queue_size = tglog_client_get_msg_queue_size(client);
    if (queue_size > 100 || frame % 3 == 0) {
        tglog_client_update(client);
    }
    render();
}
```

### 3. 优化批量发送参数

```c
// 高吞吐量场景
conf->batching_max_messages = 100;
conf->batching_max_publish_delay = 20;

// 低延迟场景
conf->batching_max_messages = 10;
conf->batching_max_publish_delay = 1;
```

### 4. 使用零拷贝模式

```c
// 使用环形缓冲区
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

### 5. 调整内存池大小

```c
// 如果大部分日志都是 512 字节左右
conf->payload_pool_512b_cap = 4000;
```

### 6. 监控队列大小

```c
void check_queue_health(tglog_client *client) {
    size_t queue_size = tglog_client_get_msg_queue_size(client);
    size_t queue_capacity = tglog_client_get_msg_queue_capacity(client);
    float usage = (float)queue_size / queue_capacity;

    if (usage > 0.8) {
        printf("警告: 队列使用率 %.1f%%\n", usage * 100);
    }
}
```

---

## 常见问题

### Q1: 为什么必须调用 tglog_client_update()?

**A**: 单线程版本没有后台线程,所有的网络 I/O、批量发送、回调执行都在 `update()` 中完成。如果不调用,日志永远不会被发送。

### Q2: 应该多久调用一次 update()?

**A**: 建议每帧调用一次。如果游戏帧率是 60 FPS,就是每 16ms 调用一次。

### Q3: update() 会阻塞多久?

**A**: 受 `max_update_us` 参数限制,默认最多 200 微秒(0.2ms)。实际耗时取决于网络状况和待处理的消息数量。

### Q4: 回调函数在什么时候执行?

**A**: 回调在调用 `tglog_client_update()` 时执行,不是在调用 `send()` 时。这意味着:
- 回调执行时机可预测
- 回调在主线程执行,不需要考虑线程安全
- 可以在回调中安全地访问游戏数据

### Q5: 程序退出时日志没有全部发送怎么办?

**A**: 在调用 `tglog_client_destroy()` 之前,持续调用 `update()` 直到队列清空:

```c
while (tglog_client_get_msg_queue_size(client) > 0) {
    tglog_client_update(client);
    usleep(10000);
}
tglog_client_destroy(client);
```

### Q6: 如何判断日志发送成功?

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
tglog_client_update(client);  // 触发回调
```

### Q7: 可以在回调中再次调用 tglog_client_send() 吗?

**A**: 可以,而且完全安全!因为回调在主线程执行,不会有递归或重入问题。

### Q8: 如何处理服务器断线?

**A**: SDK 会自动重连,你不需要做任何处理。如果重连失败,发送请求会返回错误,回调会收到错误码。

### Q9: UDP 和 TCP 该选哪个?

**A**:
- **TCP**: 可靠,有序,推荐用于重要日志
- **UDP**: 高性能,但可能丢包,适合对可靠性要求不高的场景

### Q10: 单线程版本和多线程版本性能差多少?

**A**:
- **单线程**: 10-20 万条/秒
- **多线程**: 100 万条/秒

单线程版本性能较低,但对于大多数游戏服务器已经足够。优势是简单、可控、低延迟。

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
            break;

        case TGLOG_ERR_SEND_TIMEOUT:
            fprintf(stderr, "发送超时: %s\n", err_msg);
            break;

        case TGLOG_ERR_SEND_QUEUE_IS_FULL:
            fprintf(stderr, "发送队列已满: %s\n", err_msg);
            break;

        case TGLOG_ERR_NO_AVAILABLE_CONN:
            fprintf(stderr, "没有可用连接: %s\n", err_msg);
            break;

        case TGLOG_ERR_PRODUCER_CLOSED:
            fprintf(stderr, "客户端已关闭: %s\n", err_msg);
            break;

        case TGLOG_ERR_LOG_TOO_LONG:
            fprintf(stderr, "日志过长: %s\n", err_msg);
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

恭喜你完成了 TGLog 单线程版本的学习!现在你应该能够:

1. ✓ 理解 TGLog-ST 的基本概念和使用场景
2. ✓ 正确安装依赖和编译 SDK
3. ✓ 创建配置和客户端
4. ✓ 在游戏主循环中正确调用 `update()`
5. ✓ 发送日志并处理回调
6. ✓ 调整配置参数优化性能
7. ✓ 监控运行状态和统计信息
8. ✓ 处理错误和异常情况

### 单线程版本 vs 多线程版本

| 特性 | 单线程版本 | 多线程版本 |
|------|-----------|-----------|
| **性能** | 10-20 万条/秒 | 100 万条/秒 |
| **延迟** | 更低(无线程切换) | 稍高 |
| **控制** | 完全可控 | 异步执行 |
| **复杂度** | 简单 | 较复杂 |
| **回调执行** | 主线程,可预测 | 后台线程,需线程安全 |
| **初始化** | 无特殊要求 | 需调用 `tglog_use_thread()` |
| **驱动方式** | 需调用 `update()` | 自动运行 |
| **适用场景** | 单线程游戏服务器 | 多线程服务器 |

### 选择建议

如果你的项目满足以下条件,单线程版本是更好的选择:
- 游戏服务器是单线程架构(如基于帧循环)
- 日志量不超过 10-20 万条/秒
- 希望精确控制日志发送时机
- 不想引入多线程的复杂性

否则,考虑使用多线程版本。

### 下一步

- 阅读 `tglog-st/DESIGN.md` 了解内部实现原理
- 查看 `test-st/tglog_test.c` 学习更多高级用法
- 根据实际业务需求调整配置参数
- 在测试环境充分测试后再部署到生产环境

### 获取帮助

- 查看源码中的注释和文档
- 参考 `test-st` 目录下的示例代码
- 使用调试日志功能排查问题

祝你使用愉快!
