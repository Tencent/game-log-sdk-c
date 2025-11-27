# TGLog SDK - 高性能C语言日志客户端

![C99](https://img.shields.io/badge/C-C99-blue.svg)
![Performance](https://img.shields.io/badge/Performance-1M_logs/sec-green.svg)
![Protocol](https://img.shields.io/badge/Protocol-TCP/UDP-orange.svg)
![License](https://img.shields.io/badge/License-MIT-blue.svg)

TGLog SDK 是一个专为高性能日志传输设计的C语言客户端库，提供多线程和单线程两个版本，支持每秒传输10万到100万条日志记录。该SDK遵循C99标准，基于libevent和ConcurrencyKit构建，提供无锁、高并发的日志传输能力，并提供Java和Python语言绑定。

## ✨ 核心特性

### 🚀 双版本架构
- **多线程版本 (tglog)**: 100万条/秒，适用于高吞吐量服务器
- **单线程版本 (tglog-st)**: 10-20万条/秒，适用于游戏服务器和单线程应用
- **API兼容**: 两个版本保持相同的API接口

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

### 🔌 多语言支持
- **C/C++**: 原生API
- **Java**: JNI绑定，支持DirectByteBuffer零拷贝
- **Python**: 提供ctypes、C扩展和Cython三种实现

### ⚙️ 灵活配置
- **参数可调**: 批次大小、超时时间、重传次数等
- **连接管理**: 半长连接，支持生命周期配置
- **DNS解析**: 定时更新，支持域名变更
- **内存优化**: 预分配缓冲区，支持jemalloc

## 📋 系统要求

### 依赖库
- **libevent**: 高性能事件库
- **ConcurrencyKit (ck)**: 无锁数据结构库
- **jemalloc** (推荐): 高性能内存分配器
- **pthread**: POSIX线程支持

> **重要说明**: TGLog库本身不链接jemalloc，而是由最终的可执行程序或语言绑定链接。这样可以避免版本冲突，让用户自由选择内存分配器。

### 编译环境
- **C编译器**: 支持C99标准 (GCC 4.9+, Clang 3.9+)
- **操作系统**: Linux, macOS, FreeBSD
- **架构**: x86_64, ARM64

> 说明：因ConcurrencyKit不支持在VisualStudio上编译，所以不支持Window平台。

### 语言绑定要求
- **Java**: JDK 8或更高版本
- **Python**: Python 3.6或更高版本

## 🚀 快速开始

### 编译安装

项目支持两种构建方式：**Makefile** 和 **CMake**。

#### 方式一：使用 Makefile（推荐）

```bash
# 克隆代码库（推荐方式：自动初始化submodule）
git clone --recurse-submodules https://git.woa.com/tglog/v3/sdk-c.git
cd sdk-c

# 或者，如果已经克隆但没有初始化submodule
git clone https://git.woa.com/tglog/v3/sdk-c.git
cd sdk-c
git submodule update --init --recursive

# 查看可用的编译目标
make help

# 编译所有组件 (依赖库 + TGLog SDK + 测试程序 + 语言绑定)
make

# 或者分步编译
make deps          # 只编译依赖库 (jemalloc, libevent, ck)
make tglog         # 只编译多线程版本
make tglog-st      # 只编译单线程版本
make test          # 编译测试程序
make bindings      # 编译所有语言绑定
make java          # 只编译Java绑定
make python        # 只编译Python绑定

# 检查依赖状态
make check-deps

# 安装到dist目录
make install
```

#### 方式二：使用 CMake

```bash
# 创建构建目录
mkdir build
cd build

# 配置项目（Debug版本）
cmake ..

# 或者配置为Release版本
cmake -DRELEASE=1 ..

# 一次性构建所有组件（推荐，与 Makefile 一致）
make -j4

# 这会自动构建：
# - 依赖库（jemalloc、libevent、ConcurrencyKit）
# - 核心库（tglog 和 tglog-st）
# - 测试程序（多线程和单线程版本）
# - TAPP 集成示例
# - 语言绑定（Java 和 Python）

# 或者分步构建
make deps -j4                          # 只构建依赖库
make tglog tglog-st -j4               # 只构建核心库
make test_programs test_st_programs -j4  # 只构建测试程序
make bindings -j4                      # 只构建语言绑定

# 查看所有可用目标
make help_info

# 安装到dist目录
make install
```

**CMake 注意事项**：
- 执行 `make` 会自动构建**所有组件**，包括依赖库、核心库、测试程序和语言绑定
- 推荐使用独立的构建目录（out-of-source build）
- CMake 会生成 `compile_commands.json`，便于 IDE 集成（CLion、VSCode等）
- 构建行为与 Makefile 完全一致

### C语言基本使用

#### 多线程版本 (tglog)

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
tglog_conf_destroy(conf);  // client会复制配置，conf可以释放

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
tglog_client_send(client, log, log_callback, NULL, TGLOG_MSG_F_COPY);

// 清理资源
tglog_client_destroy(client);
```

#### 单线程版本 (tglog-st)

```c
#include "tglog-st/tglog.h"

// 初始化配置
tglog_conf *conf = tglog_conf_create(
    TGLOG_NETWORK_TCP,
    "your.server.com",
    8080,
    NULL, NULL, NULL
);

// 创建客户端
tglog_client *client = tglog_client_create(conf);
tglog_conf_destroy(conf);

// 游戏主循环
while (game_running) {
    // 处理游戏逻辑
    process_game_logic();

    // 发送日志
    const char *log = "battle|winner=1|loser=2|score=100\n";
    tglog_client_send(client, log, NULL, NULL, TGLOG_MSG_F_COPY);

    // 必须定期调用update处理网络I/O
    tglog_client_update(client);
}

// 清理资源
tglog_client_destroy(client);
```

#### 编译和链接

使用TGLog库时，需要链接以下库：

```bash
# 多线程版本
gcc your_program.c -o your_program \
    -I/path/to/sdk-c/dist/include \
    -L/path/to/sdk-c/dist/lib \
    -L/path/to/sdk-c/thirdparty/jemalloc/lib \
    -ltglog -levent -levent_pthreads -lck -ljemalloc -lpthread -lm

# 单线程版本
gcc your_program.c -o your_program \
    -I/path/to/sdk-c/dist/include \
    -L/path/to/sdk-c/dist/lib \
    -L/path/to/sdk-c/thirdparty/jemalloc/lib \
    -ltglog-st -levent -lck -ljemalloc -lpthread -lm
```

> **注意**: jemalloc必须由最终的可执行程序链接，而不是由TGLog库链接。这样可以避免版本冲突。

### Java使用示例

```java
import com.tencent.tglog.*;

// 创建配置
TglogConfig config = TglogConfig.createTcp("127.0.0.1", 20001)
    .setWorkerNum(8)
    .setMaxConnLifetime(300);

// 创建客户端
try (TglogClient client = new TglogClient(config)) {
    // 发送日志
    client.sendAsync("mylog|field1|field2|field3\n",
        new TglogCallback() {
            public void onSuccess(String payload) {
                System.out.println("发送成功");
            }
            public void onError(String payload, int code, String msg) {
                System.err.println("发送失败: " + msg);
            }
        }).get();

    // 获取统计信息
    TglogStats stats = client.getStats();
    System.out.println("成功率: " + stats.getSuccessRate());
}
```

### Python使用示例

```python
from tglog_client import TglogClient, TglogConfig, TGLOG_NETWORK_TCP

# 创建配置
config = TglogConfig(
    network=TGLOG_NETWORK_TCP,
    host="127.0.0.1",
    port=20001
)

# 创建客户端
client = TglogClient(config)

# 发送日志
def callback(payload, err_code, err_msg):
    if err_code == 0:
        print(f"成功: {payload}")
    else:
        print(f"错误: {err_msg}")

client.send("mylog|field1|field2\n", callback)

# 获取统计信息
metrics = client.get_metrics()
print(f"统计信息: {metrics}")

# 清理
client.destroy()
```

## 📖 详细配置

### tglog_conf 配置项

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `network` | enum | UDP | 网络协议 (TCP/UDP) |
| `host` | string | - | 服务器地址 (IP或域名) |
| `port` | int | - | 服务器端口 |
| `worker_num` | int | 8 | Worker线程数量 (仅多线程版本) |
| `update_interval` | int | 60s | DNS更新间隔 |
| `conn_timeout` | int | 3000ms | 连接超时时间 (仅TCP) |
| `write_high_watermark` | int | 32MB | 写缓冲区高水位 |
| `socket_send_buffer_size` | int | 16MB | socket发送缓冲区大小 |
| `max_conn_lifetime` | int | 300s | 连接生命周期 |
| `batching_max_publish_delay` | int | 10ms | 批次最大延迟 |
| `batching_max_messages` | int | 100 | 批次最大消息数 |
| `batching_max_size` | int | 32KB | 批次最大大小 |
| `max_pending_messages` | int | 409600/1000000 | 队列最大容量 |
| `send_timeout` | int | 30000ms | 消息最长停留时间 |
| `max_retries` | int | 3 | 最大重传次数 |
| `max_update_us` | int | 300us | 单次update最大时间 (仅单线程版本) |

### 高级配置示例

```c
tglog_conf *conf = tglog_conf_create(TGLOG_NETWORK_TCP, "logs.example.com", 8080, NULL, NULL, NULL);

// 性能优化配置
conf->worker_num = 16;                      // 增加Worker线程 (多线程版本)
conf->batching_max_messages = 200;          // 增大批次大小
conf->batching_max_size = 80 * 1024;        // 增大批次字节数
conf->batching_max_publish_delay = 5;       // 减少延迟

// 网络优化配置
conf->socket_send_buffer_size = 32 * 1024 * 1024;  // 32MB发送缓冲区
conf->write_high_watermark = 64 * 1024 * 1024;     // 64MB写缓冲区高水位
conf->max_conn_lifetime = 600;              // 10分钟连接生命周期

// 可靠性配置
conf->max_retries = 5;                      // 增加重传次数
conf->send_timeout = 60000;                 // 60秒发送超时
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
- `succeeded_msg_count`: 发送成功的消息数
- `failed_msg_count`: 发送失败的消息数
- `timeout_msg_count`: 超时的消息数
- `retry_msg_count`: 重传次数
- `dropped_msg_count`: 丢弃的消息数

#### Gauge类型 (瞬时状态)
- `endpoint_count`: 当前端点数量
- `msg_queue_size`: 当前队列大小
- `msg_queue_capacity`: 队列容量

## 🏗️ 架构设计

### 多线程版本架构

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

### 单线程版本架构

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
     │    └── tglog_conn_list ◄─── 连接池
     │
     ├── tglog_metrics ◄─── 性能统计
     │
     └── tglog_debug_logger ◄─── 调试日志
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

## 🔧 编译选项

### Makefile目标

```bash
# 主要编译目标
make all           # 编译所有组件
make deps          # 只编译依赖库
make tglog         # 只编译多线程版本
make tglog-st      # 只编译单线程版本
make test          # 编译测试程序
make test-tapp     # 编译TAPP集成示例
make bindings      # 编译所有语言绑定
make java          # 只编译Java绑定
make python        # 只编译Python绑定

# 清理目标
make clean         # 清理编译产物
make clean-deps    # 清理依赖库
make clean-bindings # 清理语言绑定

# 其他目标
make check-deps    # 检查依赖状态
make install       # 安装到dist目录
make uninstall     # 删除dist目录
make help          # 显示帮助信息
```

### 编译选项

```bash
# 开启优化编译
make RELEASE=1

# 调试版本
make

# 显示详细编译信息
make V=1
```

## 🌍 语言绑定

### Java绑定

TGLog提供完整的Java JNI绑定，支持高性能日志发送。

**特性**:
- DirectByteBuffer零拷贝支持
- 线程安全的回调机制
- 四种性能模式（标准、性能、DirectByteBuffer无回调、DirectByteBuffer带回调）
- 完整的配置和统计API

**性能**: DirectByteBuffer模式可达到C版本56%的性能（100万条日志约1-2秒）

**文档**: 详见 [bindings/java/README.md](bindings/java/README.md)

### Python绑定

TGLog提供三种Python绑定实现：

1. **ctypes绑定**: 纯Python实现，无需编译
2. **C扩展**: 原生Python C API扩展，性能更好
3. **Cython扩展**: 高性能Cython扩展，接近原生性能

**性能对比** (100万条日志):
- C原生: 0.6秒 (基准)
- Cython: 1.1秒 (56%)
- C扩展: 1.5秒 (42%)
- ctypes: 36秒 (2.8%)

**文档**: 详见 [bindings/python/README.md](bindings/python/README.md)

## 🎮 集成示例

### TAPP框架集成

TGLog SDK提供了与腾讯TAPP框架的集成示例，展示如何在游戏服务器中使用单线程版本。

**示例位置**: `test-tapp/`

**关键特性**:
- 集成到TAPP的tick回调中（每5ms调用一次）
- 使用TAPP的日志系统输出调试信息
- 支持配置文件动态配置
- 性能指标实时监控

**运行示例**:
```bash
cd test-tapp
./start.sh    # 启动TAPP应用
./stop.sh     # 停止TAPP应用
```

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
tglog_conf_set_debug_logger(conf, tglog_can_log_debug, tglog_std_write_log, NULL);
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

## 🆚 版本对比

| 特性 | 多线程版本 (tglog) | 单线程版本 (tglog-st) |
|------|-------------------|---------------------|
| 线程模型 | 多Worker线程，自动处理 | 单线程，需调用update |
| 性能目标 | 100万条/秒 | 10-20万条/秒 |
| CPU开销 | 中（多线程开销） | 低（无线程切换） |
| 延迟 | 低（后台线程处理） | 取决于update频率 |
| 内存占用 | 中（多线程） | 低（单线程） |
| 集成复杂度 | 创建即用 | 需要主循环集成 |
| 适用场景 | 高吞吐量、多线程应用 | 游戏服务器、单线程应用 |
| 线程安全 | 完全线程安全 | 不需要（单线程） |

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

#### tglog_client_update (仅单线程版本)
```c
int tglog_client_update(tglog_client *client);
```
更新客户端状态，处理网络I/O、批次发送、DNS更新等。**必须在主循环中定期调用**。

#### tglog_client_print_metrics
```c
int tglog_client_print_metrics(tglog_client *client, char *buffer, size_t size, bool json);
```
打印客户端性能统计指标到缓冲区。

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

## 📂 项目结构

```
sdk-c/
├── tglog/                  # 多线程版本源代码
│   ├── client.c/h         # 客户端主逻辑
│   ├── worker.c/h         # Worker线程
│   ├── conn.c/h           # 连接管理
│   ├── discoverer.c/h     # DNS解析
│   ├── request.c/h        # 请求处理
│   ├── endpoint.c/h       # 端点管理
│   ├── metrics.c/h        # 统计指标
│   ├── README.md          # 多线程版本文档
│   └── DESIGN.md          # 设计文档
│
├── tglog-st/              # 单线程版本源代码
│   ├── client.c/h         # 客户端主逻辑
│   ├── worker.c/h         # 工作逻辑
│   ├── conn.c/h           # 连接管理
│   ├── discoverer.c/h     # DNS解析
│   ├── request.c/h        # 请求处理
│   ├── endpoint.c/h       # 端点管理
│   ├── metrics.c/h        # 统计指标
│   ├── README.md          # 单线程版本文档
│   └── DESIGN.md          # 设计文档
│
├── bindings/              # 语言绑定
│   ├── java/              # Java JNI绑定
│   │   ├── TglogClient.java
│   │   ├── TglogJNI.c
│   │   └── README_JAVA.md
│   └── python/            # Python绑定
│       ├── tglog_client.py      # ctypes绑定
│       ├── tglog_ext.c          # C扩展
│       ├── tglog_cython.pyx     # Cython扩展
│       └── README_PYTHON.md
│
├── test/                  # 多线程版本测试程序
├── test-st/               # 单线程版本测试程序
├── test-tapp/             # TAPP框架集成示例
│   ├── tglog_tapp.c
│   ├── start.sh
│   └── stop.sh
│
├── event/                 # libevent源代码
├── ck/                    # ConcurrencyKit源代码
├── jemalloc/              # jemalloc源代码
│
├── lib/                   # 编译生成的库文件
│   ├── libtglog.a         # 多线程静态库
│   ├── libtglog.so        # 多线程动态库
│   ├── libtglog-st.a      # 单线程静态库
│   └── libtglog-st.so     # 单线程动态库
│
├── Makefile               # 主Makefile
└── README.md              # 本文档
```

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

本项目采用 MIT许可证 开源。

## 🆘 支持

- **文档**:
  - 多线程版本: [tglog/README.md](tglog/README.md) 和 [tglog/DESIGN.md](tglog/DESIGN.md)
  - 单线程版本: [tglog-st/README.md](tglog-st/README.md) 和 [tglog-st/DESIGN.md](tglog-st/DESIGN.md)
  - Java绑定: [bindings/java/README_JAVA.md](bindings/java/README.md)
  - Python绑定: [bindings/python/README_PYTHON.md](bindings/python/README_PYTHON.md)
- **示例**: 参考 `test/`、`test-st/` 和 `test-tapp/` 目录下的示例代码
- **问题反馈**: 通过GitHub Issues提交问题
- **技术讨论**: 欢迎参与代码Review和技术讨论

## 🎯 性能基准

### C语言性能 (100万条日志)

| 版本 | 吞吐量 | 延迟 | 适用场景 |
|------|--------|------|---------|
| 多线程 (8 workers) | ~100万条/秒 | <1ms | 高吞吐量服务器 |
| 单线程 | ~10-20万条/秒 | <10ms | 游戏服务器、单线程应用 |

### 语言绑定性能 (100万条日志)

| 语言/实现 | 发送时间 | 吞吐量 | 相对性能 |
|----------|---------|--------|---------|
| C原生 | 0.6秒 | 1,618,123条/秒 | 100% |
| Java DirectByteBuffer | 1.0-2.0秒 | 500,000-1,000,000条/秒 | 50-100% |
| Python Cython | 1.1秒 | 904,888条/秒 | 56% |
| Python C扩展 | 1.5秒 | 682,810条/秒 | 42% |
| Python ctypes | 36秒 | 27,859条/秒 | 2.8% |

---

⭐ 如果这个项目对您有帮助，请给我们一个Star！
