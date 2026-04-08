# TGLog SDK - 高性能C语言日志客户端

![C99](https://img.shields.io/badge/C-C99-blue.svg)
![Performance](https://img.shields.io/badge/Performance-1M_logs/sec-green.svg)
![Protocol](https://img.shields.io/badge/Protocol-TCP/UDP-orange.svg)
![License](https://img.shields.io/badge/License-MIT-blue.svg)

TGLog SDK 是一个专为高性能日志传输设计的C语言客户端库，提供多线程和单线程两个版本，支持每秒传输10万到100万条日志记录。该SDK遵循C99标准，基于libevent和ConcurrencyKit构建，提供无锁、高并发的日志传输能力，并提供Python语言绑定。

> 说明：如需使用Java，请使用纯Java版本的[SDK](https://git.woa.com/tglog/v3/sdk-java)
## 📝 已知限制
- 单条日志消息除了结尾不能包含 `\n` 字符，如果日志字段值内部要包含`\n`或`|`，需要把这2个字符转义
- UDP 模式下单个批次不能超过 65507 字节
- 因为历史原因，协议是只有请求没有响应的，发送成功仅表示数据写入网络缓冲区成功，不表示服务器一定收到，有发送失败重试机制，但没有应用层确认和重传机制
- 单线程版本大日志时单次调用有可能（5/100,000,000左右的概率）超过1ms，这是socket相关系统调用固有开销，取决于socket缓冲区和系统调度，实测老的TLog API TCP/UDP都有同样的问题
- 单线程版本在所有连接异常断开（可能性极低）又恢复后，因为要发送堆积的数据，短时间内IO变多，单次调用有可能超过1ms，可通过`test-tapp`程序中途停止服务器测试


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
- **C编译器**: 支持C99标准 (GCC 4.9+, Clang 3.9+, MSVC 2019+)
- **操作系统**: Linux, Windows
- **架构**: x86_64

> 说明：Windows 平台使用 CMake + MSVC 构建，不依赖 ConcurrencyKit 和 jemalloc。详见下方 [Windows 构建](#方式三windows-使用-cmake--msvc) 章节。

### 语言绑定要求
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

# 如果下载的是ZIP压缩包（不是git clone）
# 可以直接make，会自动下载依赖
# 或者手动下载：./download-deps.sh

# 查看可用的编译目标
make help

# 编译所有组件 (依赖库 + TGLog SDK + 测试程序 + 语言绑定)
make

# 或者分步编译
make deps          # 只编译依赖库 (jemalloc, libevent, ck)
make tglog         # 只编译多线程版本
make tglog-st      # 只编译单线程版本
make test          # 编译测试程序
make bindings      # 编译Python绑定
make python        # 编译Python绑定

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
# - 语言绑定（Python）

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

#### 方式三：Windows 使用 CMake + MSVC

Windows 平台使用 CMake 作为构建入口，支持 Visual Studio 2019 及以上版本。

**前置条件**：
- Visual Studio 2019+ 或 Visual Studio Build Tools（需包含"使用 C++ 的桌面开发"工作负载）
- CMake 3.10+（Visual Studio 自带或单独安装）
- Git（用于初始化子模块）

```bash
# 克隆代码库并初始化子模块
git clone --recurse-submodules https://git.woa.com/tglog/v3/sdk-c.git
cd sdk-c

# 使用 CMake 生成 Visual Studio 工程
mkdir out
cd out
cmake ..

# 命令行构建（Debug 版本）
cmake --build . --config Debug

# 命令行构建（Release 版本）
cmake --build . --config Release

# 安装到 dist 目录
cmake --install . --config Release
```

**使用 Visual Studio IDE**：
```bash
# 生成 VS 解决方案后，直接用 VS 打开
cmake ..
start sdk_c.sln
```

或直接用 Visual Studio 的"打开文件夹"功能打开项目根目录，VS 会自动识别 `CMakeLists.txt`。

**Windows 构建说明**：
- Windows 下通过 `add_subdirectory` 方式构建 libevent，无需手动安装
- 不依赖 ConcurrencyKit 和 jemalloc，使用系统默认内存分配器
- 构建产物包括多线程版本 (tglog) 和单线程版本 (tglog-st) 的静态库和动态库
- 多线程版本在 Windows 下使用 Windows 原生线程 API 替代 pthread
- 需要链接 `ws2_32`（Windows Sockets）系统库

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

**Windows (MSVC) 链接**：

```bash
# 多线程版本
cl your_program.c /I path\to\sdk-c\dist\include ^
    /link path\to\sdk-c\dist\lib\tglog.lib ^
    path\to\sdk-c\thirdparty\event\lib\event_core.lib ^
    path\to\sdk-c\thirdparty\event\lib\event_pthreads.lib ^
    ws2_32.lib

# 单线程版本
cl your_program.c /I path\to\sdk-c\dist\include ^
    /link path\to\sdk-c\dist\lib\tglog-st.lib ^
    path\to\sdk-c\thirdparty\event\lib\event_core.lib ^
    ws2_32.lib
```

> **注意**: Windows 下不需要链接 jemalloc 和 pthread，使用系统默认内存分配器和线程库。

### Python使用示例

```python
# 示例 1: ctypes 版本（纯 Python，无需编译，适合快速开发）
from tglog_client import TglogClientPy

client = TglogClientPy(protocol="tcp", host="dev.tglog.com", port=20001, workers=8)

def callback(payload, err_code, err_msg):
    if err_code == 0:
        print(f"成功: {payload}")
    else:
        print(f"错误: {err_msg}")

client.send("mylog|field1|field2\n", callback=callback)
client.destroy()

# 示例 2: C Extension 版本（高性能，需要编译）
import tglog_ext

client = tglog_ext.TglogClient("tcp", "dev.tglog.com", 20001)
client.send("mylog|field1|field2\n", use_callback=True)

# 示例 3: Cython 版本（最高性能，推荐生产环境）
from tglog_cython import TglogClient

client = TglogClient(protocol="tcp", host="dev.tglog.com", port=20001, workers=4)

def callback(payload, err_code, err_msg):
    if err_code == 0:
        print(f"成功: {payload}")

# 高性能发送（可达 ~1,283,890 msg/s）
for i in range(10000):
    client.send(f"mylog|data|{i}\n", callback)

# 获取统计信息
metrics = client.get_metrics()
print(f"统计信息: {metrics}")

client.destroy()
```

## 📖 详细配置

### tglog_conf 配置项

> **版本差异说明**:
> - 多线程版本（tglog）有 `worker_num` 配置，单线程版本（tglog-st）没有
> - 单线程版本（tglog-st）有 `max_update_us` 和 `max_messages_per_update` 配置，多线程版本（tglog）没有
> - 内存池配置的默认值不同：多线程版本是单线程版本的 10 倍
> - 多线程版本的回调函数（`can_log`、`write_log`）必须是线程安全的

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| **基础配置** | | | |
| `network` | enum | TCP | 网络协议 (TCP/UDP) |
| `host` | string | - | 服务器地址 (IP或域名) |
| `port` | int | - | 服务器端口 |
| **连接配置** | | | |
| `update_interval` | int | 60s | DNS更新间隔 |
| `conn_timeout` | int | 3000ms | 连接超时时间 (仅TCP) |
| `max_conn_lifetime` | int | 300s | 连接生命周期 |
| **缓冲区配置** | | | |
| `write_high_watermark` | int | 32MB | 写缓冲区高水位 |
| `socket_send_buffer_size` | int | 64MB | socket发送缓冲区大小 |
| `socket_recv_buffer_size` | int | 16KB | socket接收缓冲区大小 |
| **批处理配置** | | | |
| `batching_max_publish_delay` | int | 10ms | 批次最大延迟 |
| `batching_max_messages` | int | 50 | 批次最大消息数 |
| `batching_max_size` | int | 32KB | 批次最大大小 |
| **队列和超时配置** | | | |
| `max_pending_messages` | int | 409600 (多线程)<br>1000000 (单线程) | 每个worker最大待处理消息数 |
| `send_timeout` | int | 30000ms | 发送超时时间 |
| `max_retries` | int | 3 | 最大重试次数 |
| `stop_timeout` | int | 3000ms | 停止超时时间 |
| **线程配置** | | | |
| `worker_num` | int | 8 | Worker线程数量 (仅多线程版本) |
| **单线程版本特有配置** | | | |
| `max_update_us` | int | 200us | 单次update调用的最大时间 (仅单线程版本) |
| `max_messages_per_update` | int | 1000 | 每次update处理的最大消息数 (仅单线程版本) |
| **内存池配置** | | | |
| `message_pool_cap` | size_t | 1000000 (多线程)<br>100000 (单线程) | 消息池容量 |
| `payload_pool_256b_cap` | size_t | 20000 (多线程)<br>2000 (单线程) | 256字节负载池容量 |
| `payload_pool_512b_cap` | size_t | 20000 (多线程)<br>2000 (单线程) | 512字节负载池容量 |
| `payload_pool_1kb_cap` | size_t | 20000 (多线程)<br>2000 (单线程) | 1KB负载池容量 |
| `payload_pool_2kb_cap` | size_t | 10000 (多线程)<br>1000 (单线程) | 2KB负载池容量 |
| `payload_pool_4kb_cap` | size_t | 10000 (多线程)<br>1000 (单线程) | 4KB负载池容量 |
| `payload_pool_8kb_cap` | size_t | 10000 (多线程)<br>1000 (单线程) | 8KB负载池容量 |
| `payload_pool_16kb_cap` | size_t | 5000 (多线程)<br>500 (单线程) | 16KB负载池容量 |
| `payload_pool_32kb_cap` | size_t | 5000 (多线程)<br>500 (单线程) | 32KB负载池容量 |
| `payload_pool_48kb_cap` | size_t | 1000 (多线程)<br>100 (单线程) | 48KB负载池容量 |
| `payload_pool_64kb_cap` | size_t | 1000 (多线程)<br>100 (单线程) | 64KB负载池容量 |
| **调试日志配置** | | | |
| `can_log` | func | tglog_can_log_info | 日志过滤函数 |
| `write_log` | func | tglog_std_write_log | 日志写入函数 |
| `user_data` | void* | NULL | 用户数据 |

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

#### Counter 类型（累计计数）

**消息统计**
- `request_msg_count`: 应用请求发送的消息总数
- `enqueued_msg_count`: 成功入队的消息数
- `dequeued_msg_count`: 成功出队的消息数
- `succeeded_msg_count`: 成功发送的消息数
- `failed_msg_count`: 失败的消息数（包括超时和丢弃）
- `timeout_msg_count`: 超时的消息数
- `retry_msg_count`: 重试次数（每次重试都计数）
- `dropped_msg_count`: 丢弃的消息数（请求但未入队）

**错误统计**
- `eagain_err_count`: EAGAIN 错误次数

**连接统计**
- `opened_conn_count`: 打开的连接数
- `closed_conn_count`: 关闭的连接数
- `error_conn_count`: 错误的连接数

**端点统计**
- `add_endpoint_count`: 添加端点的次数
- `remove_endpoint_count`: 移除端点的次数

#### Gauge 类型（瞬时状态）
- `endpoint_count`: 当前端点数量
- `msg_queue_size`: 当前消息队列大小
- `msg_queue_capacity`: 消息队列容量

#### 单线程版本特有指标（Update 性能监控）

**Update 调用统计**
- `total_update_count`: 总的 `tglog_client_update()` 调用次数
- `timeout_update_count`: 超时的 update 调用次数
- `idle_update_count`: 空闲的 update 调用次数（没有分发任何消息）

**Update 性能指标**
- `max_update_time_us`: 单次 update 调用的最大耗时（微秒）
- `limit_update_time_us`: 配置中 update 调用的时间限制（微秒）
- `max_event_dispatch_time_us`: 单次 update 中事件分发的最大耗时（微秒）
- `max_msg_dispatch_time_us`: 单次 update 中消息分发的最大耗时（微秒）
- `max_msg_dispatch_count`: 单次 update 中分发的最大消息数

> **注意**: 单线程版本需要应用主动调用 `tglog_client_update()` 来驱动事件循环，这些指标用于监控 update 调用的性能表现。

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
make bindings      # 编译Python绑定
make python        # 编译Python绑定

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

### Python绑定

TGLog提供三种Python绑定实现：

1. **Cython扩展** ⭐⭐⭐⭐⭐: 最高性能，**超过 C 基准 5%**，推荐生产环境
2. **C扩展** ⭐⭐⭐⭐: 高性能，达到 C 基准的 78%，适合生产环境
3. **ctypes绑定** ❌: 纯Python实现，无需编译，仅 3% 性能，不推荐生产使用

**性能对比** (发送 1,000,000 条日志):

| 实现方式 | 发送时间 | 吞吐量 | 相对性能 | 推荐度 |
|---------|---------|--------|---------|--------|
| **Python Cython** 🏆 | 776.7ms | **1,283,890 msg/s** | **105%** | ⭐⭐⭐⭐⭐ |
| **C 基准** | 813.6ms | 1,225,684 msg/s | 100% | ⭐⭐⭐⭐⭐ |
| **Python C Extension** | 1,049.7ms | 950,718 msg/s | 78% | ⭐⭐⭐⭐ |
| **Python ctypes** | 25,887.2ms | 38,615 msg/s | 3% | ❌ |

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
| 平台支持 | Linux, **Windows** | Linux, **Windows** |

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
│   └── python/            # Python绑定
│       ├── tglog_client.py      # ctypes绑定
│       ├── tglog_ext.c          # C扩展
│       ├── tglog_cython.pyx     # Cython扩展
│       └── README.md
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
  - Python绑定: [bindings/python/README.md](bindings/python/README.md)
  - CMake构建指南: [CMAKE_BUILD_GUIDE.md](CMAKE_BUILD_GUIDE.md)
  - Git Submodules指南: [GIT_SUBMODULES_FAQ.md](GIT_SUBMODULES_FAQ.md)
- **示例**: 参考 `test/`、`test-st/` 和 `test-tapp/` 目录下的示例代码
- **问题反馈**: 通过GitHub Issues提交问题
- **技术讨论**: 欢迎参与代码Review和技术讨论

## 🎯 性能基准

### C语言性能 (发送 1,000,000 条日志)

| 版本 | 发送时间 | 吞吐量 | 延迟 | 适用场景 |
|------|---------|--------|------|---------|
| 多线程 (8 workers) | ~813ms | ~1,225,684 msg/s | <1ms | 高吞吐量服务器 |
| 单线程 | ~ 685ms | ~1,459,854 msg/s | <1ms | 游戏服务器、单线程应用 |

### 语言绑定性能 (发送 1,000,000 条日志)

| 语言/实现 | 发送时间 | 吞吐量 | 相对性能 | 推荐度 |
|----------|---------|--------|---------|--------|
| **Python Cython** 🏆 | 776.7ms | **1,283,890 msg/s** | **105%** | ⭐⭐⭐⭐⭐ |
| **C 基准** | 813.6ms | 1,225,684 msg/s | 100% | ⭐⭐⭐⭐⭐ |
| **Python C Extension** | 1,049.7ms | 950,718 msg/s | 78% | ⭐⭐⭐⭐ |
| **Python ctypes** | 25,887ms | 38,615 msg/s | 3% | ❌ |

**关键发现**:
- Python Cython 性能最优，甚至超过 C 基准 5%
- Python C Extension 达到 C 基准的 78%，适合生产环境
- Python ctypes 性能较差，不推荐生产使用

---

⭐ 如果这个项目对您有帮助，请给我们一个Star！
