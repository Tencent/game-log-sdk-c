# TGLog 单线程版本 SDK 设计文档

## 1. 设计概述

TGLog单线程版本SDK是专为单线程游戏服务器环境设计的日志客户端库。它保持了与多线程版本相同的API接口和功能特性，但采用纯单线程架构，通过事件驱动模型实现高效的日志传输。

### 1.0 与多线程版本的关系

#### 设计原则

- **完全隔离**: 单线程版本和多线程版本代码完全独立，不共享实现
- **概念一致**: 保持相同的模块划分、命名和职责
- **结构对应**: 文件名、结构名、函数名尽可能保持一致
- **逻辑相同**: 核心业务逻辑保持一致，仅执行模型不同

#### 文件对应关系

| 多线程版本              | 单线程版本                 | 说明           |
|--------------------|-----------------------|--------------|
| tglog/client.c     | tglog-st/client.c     | 客户端主逻辑       |
| tglog/conn.c       | tglog-st/conn.c       | 连接管理         |
| tglog/worker.c     | tglog-st/worker.c     | 工作逻辑（单线程化）   |
| tglog/discoverer.c | tglog-st/discoverer.c | DNS解析（定时器驱动） |
| tglog/request.c    | tglog-st/request.c    | 请求处理         |
| tglog/endpoint.c   | tglog-st/endpoint.c   | 端点管理         |
| tglog/metrics.c    | tglog-st/metrics.c    | 统计（无原子操作）    |

#### 主要差异

| 方面    | 多线程版本                   | 单线程版本            |
|-------|-------------------------|------------------|
| 线程模型  | Worker线程 + Discoverer线程 | 主线程 + event_base |
| 队列    | 无锁队列(CK)                | 简单缓冲区            |
| 同步    | 原子操作                    | 普通变量             |
| DNS解析 | 独立线程                    | 定时器事件            |
| 批次管理  | Worker线程处理              | update()中处理      |

### 1.1 设计目标

- **单线程架构**: 不创建任何后台线程，所有处理在主线程完成
- **事件驱动**: 基于libevent实现非阻塞I/O和定时器管理
- **高性能**: 支持每秒10万-20万条日志的吞吐量
- **低延迟**: 通过批量处理和智能调度减少延迟
- **兼容性**: API接口与多线程版本保持一致

### 1.2 设计原则

- **简单高效**: 避免复杂的线程同步，简化实现
- **非阻塞**: 所有I/O操作必须是非阻塞的
- **增量处理**: 每次处理有限的工作量，避免长时间阻塞
- **内存友好**: 使用预分配和对象复用减少内存分配
- **与多线程版本保持一致**: 核心逻辑与多线程版本相同，便于维护

### 1.3 简化设计的优势

相比复杂的批次管理器方案，采用单批次缓冲的简化设计有以下优势：

1. **更低的延迟**: 数据尽快发送，不在多个队列中排队
2. **更少的内存占用**: 只维护一个批次缓冲，无需多级队列
3. **更简单的错误处理**: pending时直接阻塞，逻辑清晰
4. **更好的顺序保证**: 严格FIFO，不会乱序
5. **与多线程版本一致**: 便于理解和维护，降低维护成本

## 2. 架构设计

### 2.1 整体架构

```
Application (Game Server)
     |
     v (调用tglog_client_update)
     |
tglog_client (单线程客户端)
     |
     +-- event_base (内部维护，自包含)
     |
     +-- tglog_endpoint_manager (端点管理)
     |        |
     |        +-- DNS解析
     |        +-- 端点列表维护
     |
     +-- tglog_conn_manager (连接管理)
     |        |
     |        +-- 连接池
     |        +-- 连接生命周期
     |        +-- 重连逻辑
     |
     +-- tglog_batch_buffer (批次缓冲 - 简化版)
     |        |
     |        +-- 单批次缓冲区
     |        +-- 立即发送策略
     |        +-- 部分发送处理
     |
     +-- tglog_metrics (统计指标)
     |
     +-- tglog_debug_logger (调试日志)
```

### 2.2 数据流设计

```
发送请求
    ↓
消息入队列
    ↓
批量聚合（根据配置的条件）
    ↓
选择可用连接
    ↓
非阻塞发送
    ↓
处理发送结果
    ↓
回调通知（成功/失败）
```

## 3. 核心组件设计

注：为保持与多线程版本的一致性，组件命名和职责划分完全对应，仅实现方式不同。

### 3.1 client.c/h - 客户端主控制器（对应tglog/client.c）

#### 结构定义

```c
typedef struct tglog_client {
    tglog_conf conf;                           // 配置信息
    struct event_base *base;                      // 内部event_base（自包含）
    tglog_endpoint_manager *endpoint_mgr;      // 端点管理器
    tglog_conn_manager *conn_mgr;             // 连接管理器
    tglog_batch_buffer batch_buffer;          // 批次缓冲（简化版，单批次）
    tglog_metrics *metrics;                      // 统计指标
    tglog_debug_logger *logger;                  // 调试日志
    struct event *batch_timeout_event;           // 批次超时定时器
    uint64_t last_update_time;                   // 上次更新时间
    uint32_t state;                              // 客户端状态
} tglog_client;
```

#### 主要职责

- 初始化和配置管理
- 协调各子组件工作
- 提供对外API接口
- 统计信息聚合

### 3.2 worker.c/h - 工作处理（对应tglog/worker.c）

在多线程版本中，worker是独立线程。在单线程版本中，worker的功能集成在update()调用中。

#### 设计要点

- **无独立消息队列**: 日志直接进入批次缓冲
- **背压机制**: pending数据未发送完时拒绝新日志
- **顺序保证**: 严格的FIFO顺序
- **简单高效**: 减少内存拷贝和管理开销

#### 工作原理

```
新日志 → 批次缓冲 → 网络发送
           ↑
      pending时阻塞
```

与多线程版本的主要区别：

- 多线程版本：生产者消费者模式，需要队列解耦
- 单线程版本：直接处理，不需要中间队列

### 3.3 request.c/h - 请求处理（对应tglog/request.c）

#### 设计理念

- **无批次管理器**: 与多线程版本一致，只维护单个批次缓冲
- **尽快发送**: 有数据就尝试发送，不积压
- **失败即阻塞**: 发送失败时拒绝新日志，保证顺序性

#### 批次触发条件

- **时间触发**: 超过batching_max_publish_delay
- **数量触发**: 达到batching_max_messages
- **大小触发**: 达到batching_max_size
- **立即发送**: 调用flush或destroy时

#### 结构定义

```c
// 不需要独立的批次管理器，直接集成在client中
typedef struct tglog_batch_buffer {
    // 基础缓冲区
    char *buffer;                       // 批次缓冲区
    size_t buffer_size;                 // 缓冲区总大小
    size_t current_size;                // 当前使用大小
    int message_count;                  // 当前消息数量
    
    // 用于TCP writev的结构
    struct iovec *iovecs;               // iovec数组
    int iovec_capacity;                 // iovec容量
    int iovec_count;                    // 当前iovec数量
    
    // 批次状态
    uint64_t create_time;               // 批次创建时间
    bool has_pending_data;              // 是否有未发送完的数据
    size_t send_offset;                 // 部分发送的偏移（UDP不需要）
    
    // 用于回调通知
    tglog_callback **callbacks;         // 回调数组
    void **user_data;                   // 用户数据数组
} tglog_batch_buffer;
```

### 3.4 conn.c/h - 连接管理（对应tglog/conn.c）

#### 连接池设计

- 维护多个连接（对应多个端点）
- 支持连接生命周期管理
- **初始化时同步连接，运行时异步连接**
- 智能的连接选择策略

#### 连接创建策略

- **初始化阶段（同步）**:
    - 使用阻塞式connect()
    - 设置连接超时（默认5秒）
    - 连接失败直接返回错误
    - 成功后转换为非阻塞模式

- **运行时阶段（异步）**:
    - 使用非阻塞socket
    - 通过libevent监听连接完成事件
    - 连接过程中不阻塞主线程
    - 连接失败自动触发重连

#### 事件处理设计（优化版）

使用libevent的统一事件回调机制，不需要单独的读事件：

```c
// 统一的I/O事件回调
void on_conn_io_event(evutil_socket_t fd, short events, void *arg) {
    tglog_conn *conn = (tglog_conn *)arg;
    
    // 处理各种事件
    if (events & EV_WRITE) {
        // 可写事件：发送数据或完成连接
        if (conn->state == CONN_STATE_CONNECTING) {
            handle_connect_complete(conn);
        } else {
            handle_send_ready(conn);
        }
    }
    
    if (events & EV_READ) {
        // 读事件：检测连接关闭（只请求无响应协议）
        char buf[1];
        if (recv(fd, buf, 1, MSG_PEEK) == 0) {
            handle_conn_close(conn);
        }
    }
    
    if (events & EV_CLOSED) {
        // 连接关闭事件（libevent 2.1.1+）
        handle_conn_close(conn);
    }
    
    if (events & EV_TIMEOUT) {
        // 超时事件
        handle_timeout(conn);
    }
}

// 注册事件（简化版）
void register_conn_events(tglog_conn *conn) {
    // 监听写事件和关闭事件
    short flags = EV_WRITE | EV_PERSIST;
    
    // 如果libevent版本支持，添加EV_CLOSED
    #ifdef EV_CLOSED
    flags |= EV_CLOSED;
    #else
    // 老版本需要监听读事件来检测关闭
    flags |= EV_READ;
    #endif
    
    conn->io_event = event_new(base, conn->fd, flags, 
                               on_conn_io_event, conn);
    event_add(conn->io_event, NULL);
}
```

**优势**：

- 单一事件处理函数，逻辑集中
- 自动处理连接异常和关闭
- 根据libevent版本自适应
- 减少事件对象数量

#### 结构定义

```c
typedef struct tglog_conn {
    int fd;                             // socket文件描述符
    tglog_endpoint *endpoint;           // 关联的端点
    enum {
        CONN_STATE_DISCONNECTED,        // 未连接
        CONN_STATE_CONNECTING,          // 连接中（异步）
        CONN_STATE_CONNECTED,           // 已连接
        CONN_STATE_ERROR                // 错误状态
    } state;
    struct event *io_event;             // 统一的I/O事件（处理写和异常）
    time_t connect_time;                // 连接建立时间
    time_t last_active_time;           // 最后活跃时间
    uint32_t retry_count;               // 重连次数
    uint32_t retry_interval;            // 重连间隔（指数退避）
} tglog_conn;

typedef struct tglog_conn_manager {
    tglog_conn *connections;         // 连接数组
    uint32_t conn_count;                // 连接数量
    uint32_t max_conns;                 // 最大连接数
    uint32_t current_conn_index;        // 当前连接索引（轮询）
    struct event *conn_check_event;     // 连接检查定时器
    struct event *conn_lifetime_event;  // 连接生命周期定时器
    bool is_initializing;               // 是否在初始化阶段
} tglog_conn_manager;
```

### 3.5 endpoint.c/h & discoverer.c/h - 端点管理和DNS解析（对应tglog/endpoint.c和discoverer.c）

#### DNS解析策略

- 定时DNS解析（可配置间隔）
- 异步DNS查询（使用libevent的DNS功能）
- 端点变更检测和通知
- 失败端点的恢复尝试

#### 结构定义

```c
typedef struct tglog_endpoint_manager {
    tglog_endpoint_list *endpoints;      // 当前端点列表
    char host[256];                     // 主机名
    int port;                           // 端口号
    struct event *dns_update_event;     // DNS更新定时器
    struct evdns_base *dns_base;        // DNS解析基础
    time_t last_update_time;            // 上次更新时间
    tglog_endpoint_list *unavailable;   // 不可用端点列表
} tglog_endpoint_manager;
```

## 4. 关键流程设计

### 4.1 消息发送流程（简化版）

```c
tglog_client_send()
    ↓
1. 检查是否有pending数据
   - 有 → 尝试flush
   - flush失败 → 返回QUEUE_FULL
    ↓
2. 将日志添加到批次缓冲
   - 保存回调信息
   - 更新批次大小和计数
    ↓
3. 检查是否需要立即发送
   - 达到max_messages
   - 达到max_size
   - 特殊标记（如flush）
    ↓
4. 需要发送 → 调用send_batch()
   - TCP: 使用writev批量发送
   - UDP: 单个sendto发送
    ↓
5. 处理发送结果
   - 成功: 清空批次，回调成功
   - 部分成功: 设置pending状态
   - 失败: 设置pending状态
    ↓
6. 返回结果
```

### 4.2 更新处理流程（简化版）

```c
tglog_client_update()
    ↓
1. 记录开始时间
    ↓
2. 非阻塞处理事件循环
   event_base_loop(base, EVLOOP_NONBLOCK)
    ↓
3. 处理网络I/O事件
   - 连接状态变化
   - 可写事件（继续发送pending数据）
    ↓
4. 检查批次超时
   if (now - batch.create_time > batching_max_publish_delay)
       flush_batch()
    ↓
5. 尝试发送pending数据
   if (has_pending_data && conn_is_writable)
       continue_send_pending()
    ↓
6. 处理定时器事件
   - DNS更新
   - 连接生命周期
   - 连接重连
    ↓
7. 检查处理时间限制
   - 超过max_update_time则退出
    ↓
8. 更新统计信息
```

### 4.3 连接创建流程

#### 4.3.1 初始化时同步连接

```c
tglog_client_init()
    ↓
1. 创建阻塞socket
   fd = socket(AF_INET, SOCK_STREAM, 0)
    ↓
2. 设置连接超时
   struct timeval tv = {.tv_sec = 5, .tv_usec = 0}
   setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO/SO_RCVTIMEO, &tv, sizeof(tv))
    ↓
3. 同步连接
   ret = connect(fd, addr, addrlen)
    ↓
4. 连接结果处理
   - 成功: 
     • 设置socket为非阻塞: fcntl(fd, F_SETFL, O_NONBLOCK)
     • 创建event并加入event_base
     • 标记连接状态为CONNECTED
     • 继续初始化其他组件
   - 失败:
     • 关闭socket
     • 返回错误码TGLOG_ERR_INIT_CONNECT_FAILED
     • 初始化失败，用户需要处理错误
```

#### 4.3.2 运行时异步连接

```c
tglog_conn_async_connect()
    ↓
1. 创建非阻塞socket
   fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)
    ↓
2. 发起非阻塞连接
   ret = connect(fd, addr, addrlen)
    ↓
3. 处理连接返回值
   - ret == 0: 立即成功（本地连接）
     • 标记为CONNECTED
     • 注册读写事件
   - errno == EINPROGRESS: 连接进行中
     • 标记为CONNECTING
     • 注册写事件监听连接完成
     • event_set(ev, fd, EV_WRITE, on_connect_complete, conn)
   - 其他错误: 连接失败
     • 标记为ERROR
     • 启动重连定时器
    ↓
4. 连接完成回调 on_connect_complete()
   - 检查SO_ERROR确认连接结果
   - 成功: 切换到CONNECTED状态，注册读写事件
   - 失败: 切换到ERROR状态，启动重连
```

### 4.4 简化的批次处理流程

```
日志到达
    ↓
检查是否有未发送完的数据
    ↓
有 → 尝试继续发送
    ↓
发送失败 → 返回队列满错误（阻塞新日志）
    ↓
发送成功或无pending数据
    ↓
添加到批次缓冲
    ↓
检查触发条件：
- 消息数 >= batching_max_messages
- 批次大小 >= batching_max_size
- 超时时间到达（在update中检查）
    ↓
触发 → 立即发送批次
    ↓
发送结果：
- 完全成功 → 清空批次，回调通知成功
- 部分成功 → 标记pending，已发送的回调成功
- 完全失败 → 标记pending，等待重试
```

### 4.4 连接管理流程

```
初始化时建立连接
    ↓
定期检查连接状态
    ↓
连接异常 → 标记不可用，尝试重连
    ↓
连接生命周期到期 → 关闭旧连接，建立新连接
    ↓
端点变更 → 更新连接池
```

### 4.5 销毁流程

```c
tglog_client_destroy()
    ↓
1. 设置关闭标志
    ↓
2. 自动flush所有待发送日志
   - 循环调用内部update直到队列为空
   - 等待所有批次发送完成或超时
    ↓
3. 关闭所有连接
    ↓
4. 销毁event_base
    ↓
5. 释放所有资源
    ↓
6. 释放客户端结构体
```

## 5. 性能优化设计

### 5.1 内存优化

- **预分配策略**: 消息队列、批次缓冲区预分配
- **对象复用**: 消息和批次对象复用，减少malloc/free
- **零拷贝**: 尽可能使用指针传递，避免内存拷贝
- **内存池**: 小对象使用简单的内存池管理

### 5.2 CPU优化

- **批量处理**: 减少系统调用次数
- **增量处理**: 每次处理有限工作量，避免CPU峰值
- **缓存友好**: 数据结构设计考虑CPU缓存
- **分支预测**: 热点路径优化分支预测

### 5.3 I/O优化

- **非阻塞I/O**: 所有socket操作非阻塞
- **批量发送**: 使用writev批量发送
- **缓冲区优化**: 合理设置socket缓冲区大小
- **Nagle算法**: 根据场景开启或关闭

### 5.4 TCP发送策略优化

#### 5.4.1 问题分析

多线程版本中每条日志调用一次`send()`是为了避免部分发送导致日志被拆分到不同RS。但在单线程版本中，这会带来：

- 频繁的系统调用开销
- 增加的延迟
- 降低的吞吐量

#### 5.4.2 优化方案

```c
// 使用writev批量发送，保证日志边界
typedef struct {
    struct iovec *iovecs;      // iovec数组
    int iovec_count;           // iovec数量
    int iovec_capacity;        // iovec容量
    tglog_message **messages;  // 对应的消息数组
    int message_count;         // 消息数量
} tglog_batch_send_buffer;

// 批量发送实现
int tglog_batch_send_tcp(tglog_conn *conn, tglog_batch_send_buffer *buffer) {
    // 1. 构建iovec数组
    for (int i = 0; i < buffer->message_count; i++) {
        buffer->iovecs[i].iov_base = buffer->messages[i]->data;
        buffer->iovecs[i].iov_len = buffer->messages[i]->len;
    }
    
    // 2. 一次系统调用发送所有日志
    ssize_t total_to_send = calculate_total_size(buffer);
    ssize_t sent = writev(conn->fd, buffer->iovecs, buffer->iovec_count);
    
    // 3. 处理部分发送
    if (sent < total_to_send) {
        // 计算哪些完整日志已发送
        int sent_messages = calculate_sent_messages(buffer, sent);
        // 标记已发送的消息
        mark_messages_sent(buffer, sent_messages);
        // 剩余的消息保留在缓冲区等待下次发送
        return TGLOG_PARTIAL_SEND;
    }
    
    return TGLOG_OK;
}
```

#### 5.4.3 部分发送处理策略

- **只确认完整发送的日志**: 通过iovec边界判断
- **未完整发送的日志重传**: 保留在发送缓冲区
- **避免日志交错**: writev保证原子性

#### 5.4.4 性能对比

| 发送策略       | 系统调用次数 | 延迟 | 吞吐量 | 复杂度 |
|------------|--------|----|-----|-----|
| 每条日志一次send | N      | 高  | 低   | 简单  |
| writev批量发送 | 1      | 低  | 高   | 中等  |
| 应用层合并      | 1      | 低  | 中   | 复杂  |

#### 5.4.5 实现建议

- TCP使用writev批量发送
- UDP仍然可以使用单次send（包边界天然保证）
- 设置合理的批量大小（如50-100条）
- 非阻塞模式下处理EAGAIN

### 5.5 延迟优化

- **快速路径**: 高频操作优化快速路径
- **智能调度**: 根据负载动态调整批次参数
- **优先级队列**: 重要日志优先发送

## 6. 错误处理设计

### 6.1 错误分类

- **网络错误**: 连接失败、发送失败、超时
- **资源错误**: 队列满、内存不足
- **配置错误**: 参数非法、地址无效
- **系统错误**: 系统调用失败

### 6.2 错误恢复策略

- **自动重连**: 连接断开自动重连
- **指数退避**: 失败后延迟递增重试
- **降级处理**: 资源不足时降级服务
- **错误上报**: 通过回调通知应用层

### 6.3 错误码定义

```c
// 使用与多线程版本相同的错误码
#define TGLOG_ERR_OK                    0
#define TGLOG_ERR_SEND_TIMEOUT          10001
#define TGLOG_ERR_SEND_FAILED           10002
#define TGLOG_ERR_PRODUCER_CLOSED       10003
#define TGLOG_ERR_SEND_QUEUE_IS_FULL    10004
#define TGLOG_ERR_NO_AVAILABLE_CONN     10005
// ... 其他错误码
```

## 7. 配置系统设计

### 7.1 配置结构

```c
typedef struct tglog_conf {
    // 网络配置
    tglog_network network;
    char host[256];
    int port;
    
    // 连接配置
    int conn_timeout;
    int max_conn_lifetime;
    int update_interval;
    
    // 批次配置
    int batching_max_publish_delay;
    int batching_max_messages;
    int batching_max_size;
    
    // 缓冲区配置
    int max_pending_messages;
    int write_high_watermark;
    int socket_send_buffer_size;
    
    // 重传配置
    int send_timeout;
    int max_retries;
    
    // 单线程特有配置
    int max_update_us;         // 单次update最大处理时间（毫秒）
    int max_messages_per_update;   // 单次update最大事件数
    
    // 回调配置
    can_log_func can_log;
    write_log_func write_log;
    void *user_data;
} tglog_conf;
```

### 7.2 默认配置

```c
void tglog_conf_set_default(tglog_conf *conf) {
    conf->network = TGLOG_NETWORK_UDP;
    conf->port = 0;
    conf->conn_timeout = 3000;
    conf->max_conn_lifetime = 300;
    conf->update_interval = 60;
    conf->batching_max_publish_delay = 10;
    conf->batching_max_messages = 100;
    conf->batching_max_size = 32 * 1024;
    conf->max_pending_messages = 1000000;  // 100万条
    conf->write_high_watermark = 32 * 1024 * 1024;
    conf->socket_send_buffer_size = 16 * 1024 * 1024;
    conf->socket_recv_buffer_size = 1 * 1024 * 1024;
    conf->send_timeout = 30000;
    conf->max_retries = 3;
    conf->stop_timeout = 3000;
    conf->max_update_us = 300;      // 300微秒
    conf->max_messages_per_update = 1000; // 最多处理1000条消息
    // 内存池配置（单线程版本容量较小）
    conf->message_pool_cap = 100000;
    conf->payload_pool_256b_cap = 2000;
    conf->payload_pool_512b_cap = 2000;
    conf->payload_pool_1kb_cap = 2000;
    conf->payload_pool_2kb_cap = 1000;
    conf->payload_pool_4kb_cap = 1000;
    conf->payload_pool_8kb_cap = 1000;
    conf->payload_pool_16kb_cap = 500;
    conf->payload_pool_32kb_cap = 500;
    conf->payload_pool_48kb_cap = 100;
    conf->payload_pool_64kb_cap = 100;
}
```

## 8. 统计监控设计

### 8.1 统计指标

与多线程版本保持一致的指标：

- **Counter类型**: 累计计数
    - request_msg_count: 请求消息数
    - succeeded_msg_count: 成功消息数
    - failed_msg_count: 失败消息数
    - timeout_msg_count: 超时消息数
    - retry_msg_count: 重传次数
    - dropped_msg_count: 丢弃消息数

- **Gauge类型**: 瞬时状态
    - msg_queue_size: 队列当前大小
    - msg_queue_capacity: 队列容量
    - endpoint_count: 端点数量
    - active_conn_count: 活跃连接数

### 8.2 统计更新

- 使用普通变量（单线程无需原子操作）
- 实时更新，低开销
- 支持JSON格式输出

## 9. API接口设计

### 9.1 初始化相关API

```c
// 创建配置对象
tglog_conf* tglog_conf_create(
    tglog_network network,
    const char *host,
    int port,
    can_log_func can_log,
    write_log_func write_log,
    void *user_data
);

// 创建客户端（包含同步连接）
tglog_client* tglog_client_create(tglog_conf *conf);
```

### 9.2 核心功能API

```c
// 发送日志
int tglog_client_send(
    tglog_client *client,
    const char *log_payload,
    tglog_send_callback callback,
    void *user_data,
    char msg_flags
);

// 更新处理（必须定期调用）
int tglog_client_update(tglog_client *client);

// 打印统计指标
int tglog_client_print_metrics(tglog_client *client, char *buffer, size_t size, bool json);

// 打印内存池统计
int tglog_client_print_mem_pool_stats(tglog_client *client, char *buffer, size_t size, bool json);

// 销毁客户端
void tglog_client_destroy(tglog_client *client);
```

## 10. 实现建议

### 10.1 保持一致性的最佳实践

#### 命名规范

```c
// 多线程版本
typedef struct tglog_client {
    // ...
} tglog_client;

// 单线程版本（完全相同的命名）
typedef struct tglog_client {
    // ...
} tglog_client;
```

#### 函数对应

```c
// 多线程版本
int tglog_client_send(tglog_client *client, const char *log_payload,
                      tglog_send_callback callback, void *user_data, char msg_flags);

// 单线程版本（完全相同的命名和签名）
int tglog_client_send(tglog_client *client, const char *log_payload,
                      tglog_send_callback callback, void *user_data, char msg_flags);
```

用户通过编译时选择不同的库来区分：

- 链接 `-ltglog` 使用多线程版本
- 链接 `-ltglog-st` 使用单线程版本

#### 文件组织

```
// 两个版本保持相同的文件组织
client.c       - 客户端主逻辑
conn.c         - 连接管理
worker.c       - 工作逻辑
discoverer.c   - DNS解析
request.c      - 请求处理
endpoint.c     - 端点管理
metrics.c      - 统计指标
debug_logger.c - 调试日志
err.c          - 错误处理
def.c          - 公共定义
```

### 10.2 维护建议

1. **同步修改**: Bug修复和功能增强时，两个版本同步修改
2. **测试对称**: 测试用例保持对称，确保功能一致性
3. **文档同步**: API文档和注释保持同步更新
4. **版本管理**: 版本号保持一致，便于用户选择

## 11. 集成方式

### 10.1 推荐集成方式（自包含模式）

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

### 9.2 高频调用场景

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

### 9.3 低延迟场景

```c
// 配置更短的批次超时和更频繁的update
conf->batching_max_publish_delay = 5;  // 5ms批次超时
conf->max_update_us = 2;             // 每次update最多2ms

while (game_running) {
    game_update();
    
    // 每帧都调用update保证低延迟
    tglog_client_st_update(client);
}
```

## 10. 测试策略

### 10.1 单元测试

- 消息队列测试
- 批次管理测试
- 连接管理测试
- DNS解析测试

### 10.2 集成测试

- TCP/UDP发送测试
- 批量处理测试
- 重连测试
- 端点切换测试

### 10.3 性能测试

- 吞吐量测试（目标10万-20万条/秒）
- 延迟测试
- CPU使用率测试
- 内存使用测试

### 10.4 稳定性测试

- 长时间运行测试
- 网络异常模拟
- 内存泄漏检测
- 压力测试

## 11. 实现计划

### 第一阶段：核心框架

1. 基础数据结构定义
2. 配置系统实现
3. 消息队列实现
4. 事件管理框架

### 第二阶段：网络功能

1. 连接管理实现
2. 批次管理实现
3. DNS解析集成
4. 端点管理实现

### 第三阶段：完善功能

1. 重传机制实现
2. 统计监控实现
3. 错误处理完善
4. 调试日志系统

### 第四阶段：测试优化

1. 单元测试编写
2. 性能测试和优化
3. 内存泄漏修复
4. 文档完善

## 12. 风险和挑战

### 12.1 技术风险

- **性能瓶颈**: 单线程可能成为瓶颈
- **延迟问题**: 处理不及时导致延迟增加
- **内存压力**: 队列堆积导致内存增长

### 12.2 应对措施

- **性能优化**: 充分优化热点代码
- **负载控制**: 实现背压和流控机制
- **降级策略**: 过载时的降级处理

## 13. 与多线程版本的兼容性

### 13.1 API兼容

- 保持相同的函数签名
- 使用相同的数据结构
- 返回相同的错误码

### 13.2 行为兼容

- 相同的批次策略
- 相同的重传机制
- 相同的统计指标

### 13.3 配置兼容

- 支持相同的配置项
- 使用相同的默认值
- 忽略多线程特有配置（如worker_num）