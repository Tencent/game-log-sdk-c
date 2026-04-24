# TGLog SDK-ST - 高性能C语言日志客户端（单线程版本）

![C99](https://img.shields.io/badge/C-C99-blue.svg)
![Performance](https://img.shields.io/badge/Performance-100K_logs/sec-green.svg)
![Protocol](https://img.shields.io/badge/Protocol-TCP/UDP-orange.svg)
![Threading](https://img.shields.io/badge/Threading-Single--Threaded-yellow.svg)

TGLog SDK-ST 是专为单线程环境（如游戏服务器）设计的高性能日志客户端库。它采用纯单线程架构，通过事件驱动模型实现高效的日志传输，目标性能为每秒10-20万条约300字节的日志记录。

## ✨ 核心特性

### 🎮 单线程友好
- **零后台线程**: 所有处理在主线程完成，无线程同步开销
- **事件驱动**: 基于libevent的非阻塞I/O模型
- **增量处理**: 每次update处理有限工作量，不阻塞主循环
- **游戏服务器优化**: 专为帧驱动的游戏服务器设计

### 🚀 高性能设计
- **无锁架构**: 单线程无需锁和原子操作
- **内存池**: 预分配和对象复用减少malloc/free开销
- **批量传输**: 智能批量聚合，减少网络开销
- **零拷贝支持**: 支持多种内存管理策略

### 🌐 网络协议支持
- **TCP协议**: 可靠传输，批次最大512KB
- **UDP协议**: 高性能传输，批次最大65507字节
- **负载均衡**: 支持CLB场景下的连接管理
- **自动重连**: 指数退避重连策略

### 📊 监控与调试
- **实时指标**: 丰富的性能统计指标
- **update性能监控**: 跟踪update调用的性能表现
- **可插拔日志**: 支持自定义调试日志函数
- **JSON输出**: 便于监控系统集成

### ⚙️ 灵活配置
- **参数可调**: 批次大小、超时时间、重传次数等
- **update控制**: 可配置单次update的最大处理时间和消息数
- **连接管理**: 半长连接，支持生命周期配置
- **DNS解析**: 定时更新，支持域名变更

## 📋 系统要求

### 依赖库
- **libevent**: 高性能事件库
- **ConcurrencyKit (ck)**: 用于部分数据结构
- **pthread**: POSIX线程支持（仅用于libevent内部）

### 编译环境
- **C编译器**: 支持C99标准 (GCC 4.9+, Clang 3.9+, MSVC 2019+)
- **操作系统**: Linux, Windows
- **架构**: x86_64

## 🚀 快速开始

### 编译安装

```bash
# 进入项目目录
cd sdk-c

# 查看可用的编译目标
make help

# 编译单线程版本
make tglog-st

# 编译测试程序
make test-st
```

### 基本使用

```c
#include "tglog-st/tglog.h"

// 初始化配置
tglog_conf *conf = tglog_conf_create(
    TGLOG_NETWORK_TCP,      // 协议类型
    "your.server.com",      // 服务器地址
    8080,                   // 服务器端口
    NULL,                   // 使用默认日志过滤
    NULL,                   // 使用默认日志输出
    NULL                    // 用户数据
);

// 创建客户端
tglog_client *client = tglog_client_create(conf);
// client会复制配置，conf可以释放了
tglog_conf_destroy(conf);

// 发送日志回调函数
void log_callback(const char *log_payload, int err_code,
                  const char *err_msg, void *user_data) {
    if (err_code == TGLOG_ERR_OK) {
        printf("发送成功: %s", log_payload);
    } else {
        printf("发送失败: %s (错误码: %d)\n", err_msg, err_code);
    }
}

// 游戏主循环
while (game_running) {
    // 处理游戏逻辑
    process_game_logic();

    // 发送日志 (必须以\n结尾)
    const char *log = "userlogin|userid=12345|username=alice|timestamp=1640995200\n";
    int ret = tglog_client_send(client, log, log_callback, NULL, TGLOG_MSG_F_COPY);

    // 更新日志客户端（必须定期调用）
    tglog_client_update(client);

    // 其他游戏逻辑...
}

// 获取性能指标
char buffer[8192];
int len = tglog_client_print_metrics(client, buffer, sizeof(buffer), true);
if (len > 0) {
    printf("性能指标:\n%s\n", buffer);
}

// 清理资源
tglog_client_destroy(client);
```

> [!IMPORTANT]
>
> 重要：
>
> - **必须定期调用 `tglog_client_update()`**：这是单线程版本的核心，用于处理网络I/O、批次发送、DNS更新等
> - `tglog_client_create(conf)` 会复制传入的conf，调用结束后，conf就可以释放了
> - 回调函数会在 `tglog_client_update()` 中被调用，无需考虑线程安全
> - `tglog_client` 创建后可以长期持有，在进程结束的时候再调用 `tglog_client_destroy()` 释放它

## 📖 详细配置

### tglog_conf 配置项

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `network` | enum | UDP | 网络协议 (TCP/UDP) |
| `host` | string | - | 服务器地址 (IP或域名) |
| `port` | int | - | 服务器端口 |
| `update_interval` | int | 60s | DNS更新间隔 |
| `conn_timeout` | int | 3000ms | 连接超时时间 (仅TCP) |
| `write_high_watermark` | int | 32MB | 写缓冲区高水位，超过此值停止写入 |
| `socket_send_buffer_size` | int | 16MB | socket发送缓冲区大小 |
| `socket_recv_buffer_size` | int | 1MB | socket接收缓冲区大小 |
| `max_conn_lifetime` | int | 300s | 连接生命周期 |
| `batching_max_publish_delay` | int | 10ms | 批次最大延迟 |
| `batching_max_messages` | int | 100 | 批次最大消息数 |
| `batching_max_size` | int | 32KB | 批次最大大小 |
| `max_pending_messages` | int | 1000000 | 队列最大容量 |
| `send_timeout` | int | 30000ms | 消息在客户端的最长停留时间 |
| `max_retries` | int | 3 | 最大重传次数 |
| `stop_timeout` | int | 3000ms | 停止超时时间 |
| `max_update_us` | int | 300us | 单次update最大处理时间（微秒） |
| `max_messages_per_update` | int | 1000 | 单次update最大处理消息数 |
| `can_log` | function | tglog_can_log_debug | 日志过滤函数 |
| `write_log` | function | tglog_std_write_log | 日志输出函数 |

### 高级配置示例

```c
tglog_conf *conf = tglog_conf_create(TGLOG_NETWORK_TCP, "logs.example.com", 8080, NULL, NULL, NULL);

// 性能优化配置
conf->batching_max_messages = 200;          // 增大批次大小
conf->batching_max_size = 80 * 1024;        // 增大批次字节数
conf->batching_max_publish_delay = 5;       // 减少延迟

// update控制配置
conf->max_update_us = 500;                  // 单次update最多500微秒
conf->max_messages_per_update = 2000;       // 单次update最多处理2000条消息

// 网络优化配置
conf->socket_send_buffer_size = 32 * 1024 * 1024;  // 32MB发送缓冲区
conf->write_high_watermark = 64 * 1024 * 1024;     // 64MB写缓冲区高水位
conf->max_conn_lifetime = 600;              // 10分钟连接生命周期
conf->conn_timeout = 5000;                  // 5秒连接超时

// 可靠性配置
conf->max_retries = 5;                      // 增加重传次数
conf->send_timeout = 60000;                 // 60秒发送超时

// 内存池配置
conf->message_pool_cap = 200000;            // 增大消息池容量
conf->payload_pool_1kb_cap = 5000;          // 增大1KB负载池容量
```

## 📊 性能监控

### 获取统计指标

```c
// 打印性能指标 (JSON格式)
char buffer[8192];
int len = tglog_client_print_metrics(client, buffer, sizeof(buffer), true);
if (len > 0) {
    printf("性能指标:\n%s\n", buffer);
}

// 打印性能指标 (文本格式)
len = tglog_client_print_metrics(client, buffer, sizeof(buffer), false);
if (len > 0) {
    printf("性能指标:\n%s\n", buffer);
}

// 打印内存池统计
len = tglog_client_print_mem_pool_stats(client, buffer, sizeof(buffer), true);
if (len > 0) {
    printf("内存池统计:\n%s\n", buffer);
}
```

### 统计指标说明

#### Counter类型 (累计计数)
- `request_msg_count`: 应用请求发送的消息数
- `enqueued_msg_count`: 成功入队的消息数
- `dequeued_msg_count`: 成功出队的消息数
- `succeeded_msg_count`: 发送成功的消息数
- `failed_msg_count`: 发送失败的消息数 (包含超时和丢弃)
- `timeout_msg_count`: 超时的消息数
- `retry_msg_count`: 重传次数
- `dropped_msg_count`: 丢弃的消息数 (请求但未入队)
- `eagain_err_count`: EAGAIN错误计数
- `opened_conn_count`: 打开的连接数
- `closed_conn_count`: 关闭的连接数
- `error_conn_count`: 错误的连接数
- `add_endpoint_count`: 增加的端点数
- `remove_endpoint_count`: 移除的端点数

#### Gauge类型 (瞬时状态)
- `endpoint_count`: 当前端点数量
- `msg_queue_size`: 当前队列大小
- `msg_queue_capacity`: 队列容量

#### Update性能指标（单线程特有）
- `total_update_count`: 总update调用次数
- `timeout_update_count`: 超时的update调用次数
- `idle_update_count`: 空闲的update调用次数（无消息处理）
- `max_update_time_us`: 单次update最大耗时（微秒）
- `limit_update_time_us`: 配置的update时间限制（微秒）
- `max_event_dispatch_time_us`: 单次update中事件分发最大耗时
- `max_msg_dispatch_time_us`: 单次update中消息分发最大耗时
- `max_msg_dispatch_count`: 单次update中最大消息处理数

## 🏗️ 架构设计

### 整体架构图

```
Application (Game Server)
     │
     ▼ (调用 tglog_client_update)
     │
┌─────────────────┐
│   tglog_client   │ ◄─── 主入口，API接口
└─────────────────┘
     │
     ├── event_base ◄─── 内部事件循环（自包含）
     │
     ├── tglog_discoverer ◄─── DNS解析，端点发现
     │
     ├── tglog_worker ◄─── 单工作线程（集成在update中）
     │    │
     │    ├── batch_buffer ◄─── 批次缓冲区
     │    ├── tglog_conn_list ◄─── 连接池
     │    └── 消息处理逻辑
     │
     ├── tglog_metrics ◄─── 性能统计
     │
     └── tglog_debug_logger ◄─── 调试日志
```

### 数据流程

```
发送请求 → 消息入队 → 批量处理 → 网络发送 → 回调通知
   │           │           │           │           │
   │           └─直接入批次─┘   └─update驱动─┘   └─同步回调─┘
   │
   └── 统计更新 (普通变量)
```

## 🎮 集成方式

### 推荐集成方式（游戏服务器）

```c
// 全局日志客户端
tglog_client *g_log_client = NULL;

// 游戏事件处理函数示例
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

// 初始化
void game_init() {
    tglog_conf *conf = tglog_conf_create(TGLOG_NETWORK_TCP, "server.com", 8080, NULL, NULL, NULL);
    g_log_client = tglog_client_create(conf);
    tglog_conf_destroy(conf);
}

// 游戏主循环
void game_loop() {
    while (game_running) {
        // 游戏逻辑处理，内部会触发各种事件
        process_game_logic();    // 会调用on_player_login, on_battle_result等
        handle_network_messages();
        update_game_state();

        // 更新日志客户端（批次发送、重传等）
        tglog_client_update(g_log_client);
    }
}

// 清理
void game_cleanup() {
    tglog_client_destroy(g_log_client);
    g_log_client = NULL;
}
```

### 高帧率游戏场景

```c
// 对于帧率很高的游戏，可以降低update频率
static int update_counter = 0;
while (game_running) {
    game_update();

    // 每10帧调用一次update
    if (++update_counter % 10 == 0) {
        tglog_client_update(client);
    }
}
```

### 低延迟场景

```c
// 配置更短的批次超时和更频繁的update
conf->batching_max_publish_delay = 5;  // 5ms批次超时
conf->max_update_us = 500;             // 每次update最多500微秒

while (game_running) {
    game_update();

    // 每帧都调用update保证低延迟
    tglog_client_update(client);
}
```

## 🔧 编译选项

### Makefile目标

```bash
make tglog-st      # 编译单线程版本库
make test-st       # 编译单线程版本测试程序
make clean-st      # 清理单线程版本编译产物
```

### 编译优化

```bash
# 开启优化编译
CFLAGS="-O3 -DNDEBUG" make tglog-st

# 调试版本
CFLAGS="-g -O0 -DDEBUG" make tglog-st
```

> **注意**: TGLog-ST库本身不链接jemalloc。如果你的应用程序需要使用jemalloc，应该在编译最终可执行程序时链接，例如：
> ```bash
> gcc your_app.c -o your_app -ltglog-st -levent -ljemalloc -lpthread -latomic -lm
> ```

## 🔍 故障排除

### 常见问题

#### 性能问题
- **update耗时过长**: 检查 `max_update_time_us` 指标，调整 `conf->max_update_us` 和 `conf->max_messages_per_update`
- **日志发送慢**: 增加update调用频率，或调整批次参数
- **内存使用高**: 检查队列容量和内存池配置

#### 运行时问题
- **连接失败**: 检查服务器地址和端口配置
- **日志丢失**: 检查错误回调，增加重传次数
- **update调用超时**: 减少 `max_messages_per_update` 或增加 `max_update_us`

#### 调试技巧
```c
// 启用详细调试日志 (显示所有级别日志)
tglog_conf_set_debug_logger(conf, tglog_can_log_debug, tglog_std_write_log, NULL);
```

### 性能调优

#### 高吞吐量场景
```c
conf->batching_max_messages = 200;
conf->batching_max_size = 100 * 1024;
conf->batching_max_publish_delay = 20;
conf->max_messages_per_update = 2000;
```

#### 低延迟场景
```c
conf->batching_max_messages = 10;
conf->batching_max_publish_delay = 5;
conf->max_pending_messages = 10000;
conf->max_update_us = 200;
```

## 📄 TGLog协议格式

### 标准格式
```
tglogname|field1=value1|field2=value2|...|fieldN=valueN\n
```

### 示例
```c
// 用户登录日志
"userlogin|userid=12345|username=alice|timestamp=1640995200|result=success\n"

// 订单处理日志
"order|orderid=ORDER123|userid=12345|amount=99.99|status=completed\n"

// 错误日志
"error|module=payment|errorcode=500|message=database timeout|timestamp=1640995200\n"
```

### 格式要求
- 必须以换行符 `\n` 结尾
- 使用 `|` 作为字段分隔符
- 第一个字段为日志名称
- 字段值中不能包含 `|` 和 `\n` 字符

## 📚 API参考

### 核心函数

#### tglog_client_create
```c
tglog_client *tglog_client_create(const tglog_conf *conf);
```
创建TGLog客户端实例。

#### tglog_client_send
```c
int tglog_client_send(tglog_client *client, const char *log_payload,
                      tglog_send_callback callback, void *user_data, char msg_flags);
```
发送日志消息，支持异步回调通知。

**msg_flags 参数说明**：
- `TGLOG_MSG_F_NONE`: 零拷贝模式，客户端不复制不释放消息
- `TGLOG_MSG_F_FREE`: 客户端在发送后自动释放消息（需用malloc分配）
- `TGLOG_MSG_F_COPY`: 客户端自动拷贝消息（推荐使用）

#### tglog_client_update
```c
int tglog_client_update(tglog_client *client);
```
更新客户端状态，处理网络I/O、批次发送、DNS更新等。**必须在主循环中定期调用**。

返回值：处理的消息数量，负数表示错误。

#### tglog_client_print_metrics
```c
int tglog_client_print_metrics(tglog_client *client, char *buffer, size_t size, bool json);
```
打印客户端性能统计指标到缓冲区。

#### tglog_client_print_mem_pool_stats
```c
int tglog_client_print_mem_pool_stats(tglog_client *client, char *buffer, size_t size, bool json);
```
打印内存池统计信息到缓冲区。

#### tglog_client_destroy
```c
void tglog_client_destroy(tglog_client *client);
```
销毁客户端实例，释放所有资源。

### 配置函数

#### tglog_conf_create
```c
tglog_conf *tglog_conf_create(tglog_network network, const char *host,
                              int port, can_log_func can_log,
                              write_log_func write_log, void *user_data);
```
创建配置实例。

#### tglog_conf_set_default
```c
void tglog_conf_set_default(tglog_conf *conf);
```
设置默认配置值。

## 🆚 与多线程版本的对比

| 特性 | 单线程版本 (tglog-st) | 多线程版本 (tglog) |
|------|---------------------|-------------------|
| 线程模型 | 单线程，需调用update | 多Worker线程，自动处理 |
| 性能目标 | 10-20万条/秒 | 100万条/秒 |
| CPU开销 | 低（无线程切换） | 中（多线程开销） |
| 延迟 | 取决于update频率 | 低（后台线程处理） |
| 内存占用 | 低（单线程） | 中（多线程） |
| 集成复杂度 | 需要主循环集成 | 创建即用 |
| 适用场景 | 游戏服务器、单线程应用 | 高吞吐量、多线程应用 |
| 线程安全 | 不需要（单线程） | 完全线程安全 |

## 🤝 贡献指南

### 开发环境
1. 安装C99兼容编译器
2. 安装依赖库 (libevent, ck)
3. 克隆代码库并编译

### 代码规范
- 遵循C99标准
- 函数和变量以 `tglog_` 前缀命名
- 注重内存安全和错误处理
- 添加单元测试

### 提交代码
1. Fork项目
2. 创建功能分支
3. 提交代码并添加测试
4. 创建Pull Request

## 📄 许可证

本项目采用 [MIT许可证](LICENSE) 开源。

## 🆘 支持

- **文档**: 查看 [DESIGN.md](DESIGN.md) 了解详细设计
- **示例**: 参考 `test/` 目录下的示例代码
- **问题反馈**: 通过GitHub Issues提交问题
- **技术讨论**: 欢迎参与代码Review和技术讨论

---

⭐ 如果这个项目对您有帮助，请给我们一个Star！
