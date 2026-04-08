# TGLog SDK - 高性能C语言日志客户端

![C99](https://img.shields.io/badge/C-C99-blue.svg)
![Performance](https://img.shields.io/badge/Performance-1M_logs/sec-green.svg)
![Protocol](https://img.shields.io/badge/Protocol-TCP/UDP-orange.svg)

TGLog SDK 是一个专为高性能日志传输设计的C语言客户端库，目标性能为每秒传输100万条约300字节的日志记录。该SDK遵循C99标准，基于libevent和ConcurrencyKit构建，提供无锁、高并发的日志传输能力。

## ✨ 核心特性

### 🚀 高性能设计
- **无锁队列**: 基于ConcurrencyKit实现的无锁环形队列
- **多线程架构**: 多Worker线程并行处理
- **批量传输**: 智能批量聚合，减少网络开销
- **事件驱动**: 基于libevent的高效事件循环

### 🌐 网络协议支持
- **TCP协议**: 可靠传输，批次最大512KB
- **UDP协议**: 高性能传输，批次最大65507字节
- **负载均衡**: 支持CLB场景下的连接管理
- **自动重连**: 指数退避重连策略

### 📊 监控与调试
- **实时指标**: 丰富的性能统计指标
- **可插拔日志**: 支持自定义调试日志函数
- **JSON输出**: 便于监控系统集成
- **多维度统计**: 全局和Worker粒度的指标

### ⚙️ 灵活配置
- **参数可调**: 批次大小、超时时间、重传次数等
- **连接管理**: 半长连接，支持生命周期配置
- **DNS解析**: 定时更新，支持域名变更
- **内存优化**: 预分配缓冲区，支持jemalloc

## 📋 系统要求

### 依赖库
- **libevent**: 高性能事件库
- **ConcurrencyKit (ck)**: 无锁数据结构库
- **pthread**: POSIX线程支持

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

# 编译所有组件 (依赖库 + TGLog SDK + 测试程序)
make

# 或者分步编译
make deps          # 只编译依赖库
make tglog         # 只编译TGLog库
make test          # 只编译测试程序

# 检查依赖状态
make check-deps
```

### 基本使用

```c
#include "tglog/tglog.h"

// 进程级配置，重要：一个进程只需要调用一次
tglog_use_thread();

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

// 发送日志 (必须以\n结尾)
const char *log = "userlogin|userid=12345|username=alice|timestamp=1640995200\n";
int ret = tglog_client_send(client, log, log_callback, NULL, TGLOG_MSG_F_COPY);

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
> - `tglog_use_thread()`是进程级配置，一个进程只需要调用一次；
> - `tglog_client_create(conf)`会复制传入的conf，调用结束后，conf就可以释放了；
> - `tglog_client`是多线程的，传给它的调试日志回调函数，发送日志回调函数要求是线程安全的；
> - `tglog_client`创建后可以长期持有，在进程结束的时候再调用`tglog_client_destroy()`释放它。

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
| `worker_num` | int | 8 | Worker线程数量 |
| `batching_max_publish_delay` | int | 10ms | 批次最大延迟 |
| `batching_max_messages` | int | 100 | 批次最大消息数 |
| `batching_max_size` | int | 32KB | 批次最大大小 |
| `max_pending_messages` | int | 409600 | 每个Worker的队列最大容量 |
| `send_timeout` | int | 30000ms | 消息在客户端的最长停留时间 |
| `max_retries` | int | 3 | 最大重传次数 |
| `stop_timeout` | int | 3000ms | 停止超时时间 |
| `can_log` | function | tglog_can_log_debug | 日志过滤函数 |
| `write_log` | function | tglog_std_write_log | 日志输出函数 |

### 高级配置示例

```c
tglog_conf *conf = tglog_conf_create(TGLOG_NETWORK_TCP, "logs.example.com", 8080, NULL, NULL, NULL);

// 性能优化配置
conf->worker_num = 16;                      // 增加Worker线程
conf->batching_max_messages = 200;          // 增大批次大小
conf->batching_max_size = 80 * 1024;        // 增大批次字节数
conf->batching_max_publish_delay = 5;       // 减少延迟

// 网络优化配置
conf->socket_send_buffer_size = 32 * 1024 * 1024;  // 32MB发送缓冲区
conf->write_high_watermark = 64 * 1024 * 1024;     // 64MB写缓冲区高水位
conf->max_conn_lifetime = 600;              // 10分钟连接生命周期
conf->conn_timeout = 5000;                  // 5秒连接超时

// 可靠性配置
conf->max_retries = 5;                      // 增加重传次数
conf->send_timeout = 60000;                 // 60秒发送超时

// 内存池配置
conf->message_pool_cap = 2000000;           // 增大消息池容量
conf->payload_pool_1kb_cap = 50000;         // 增大1KB负载池容量
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

## 🏗️ 架构设计

### 整体架构图

```
Application
     │
     ▼
┌─────────────────┐
│   tglog_client   │ ◄─── 主入口，API接口
└─────────────────┘
     │
     ├── tglog_discoverer ◄─── DNS解析，端点发现
     │
     ├── tglog_worker[] ◄─── 多工作线程
     │    │
     │    ├── ck_ring_t ◄─── 无锁消息队列
     │    ├── tglog_conn_list ◄─── 连接池
     │    └── event_base ◄─── 事件循环
     │
     ├── tglog_metrics ◄─── 性能统计
     │
     └── tglog_debug_logger ◄─── 调试日志
```

### 数据流程

```
发送请求 → 选择Worker → 消息入队 → 批量处理 → 网络发送 → 回调通知
   │           │           │           │           │           │
   │           └─轮询策略─┘   └─无锁队列─┘   └─事件驱动─┘   └─异步回调─┘
   │
   └── 统计更新 (原子操作)
```

## 🔧 编译选项

### Makefile目标

```bash
make all        # 编译所有组件 (依赖库 + TGLog + 测试)
make deps       # 只编译依赖库 (jemalloc, libevent, ck)
make tglog      # 只编译TGLog库
make test       # 编译测试程序
make examples   # test的别名
make clean      # 清理编译产物
make clean-deps # 清理依赖库
make check-deps # 检查依赖状态
make help       # 显示帮助信息
```

### 编译优化

```bash
# 开启优化编译
CFLAGS="-O3 -DNDEBUG" make

# 调试版本
CFLAGS="-g -O0 -DDEBUG" make
```

> **注意**: TGLog库本身不链接jemalloc。如果你的应用程序需要使用jemalloc，应该在编译最终可执行程序时链接，例如：
> ```bash
> gcc your_app.c -o your_app -ltglog -levent -levent_pthreads -lck -ljemalloc -lpthread -lm
> ```

## 🔍 故障排除

### 常见问题

#### 编译问题
```bash
# 清理重新编译
make clean && make

# 检查依赖库状态
make check-deps

# 查看详细编译信息
make V=1

# 完全重新构建 (包括依赖库)
make clean-deps && make
```

#### 运行时问题
- **连接失败**: 检查服务器地址和端口配置
- **性能不佳**: 调整Worker线程数和批次参数
- **内存使用高**: 检查队列容量和连接数配置
- **日志丢失**: 检查错误回调，增加重传次数

#### 调试技巧
```c
// 启用详细调试日志 (显示所有级别日志)
tglog_conf_set_debug_logger(conf, tglog_can_log_debug, tglog_std_write_log);
```

### 性能调优

#### 高吞吐量场景
```c
conf->worker_num = CPU核心数 * 2;
conf->batching_max_messages = 200;
conf->batching_max_size = 100 * 1024;
conf->batching_max_publish_delay = 20;
```

#### 低延迟场景
```c
conf->worker_num = CPU核心数;
conf->batching_max_messages = 10;
conf->batching_max_publish_delay = 5;
conf->max_pending_messages = 1024;
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

## 🤝 贡献指南

### 开发环境
1. 安装C99兼容编译器
2. 安装依赖库 (libevent, ck)
3. 克隆代码库并编译

### 代码规范
- 遵循C99标准 (项目设置)
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