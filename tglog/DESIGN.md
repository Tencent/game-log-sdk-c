# TGLog SDK 设计文档

## 1. 概述

TGLog SDK 是一个高性能的C语言日志客户端库，设计目标是每秒发送100万条约300字节的日志。该SDK严格遵循C99标准，使用libevent和ConcurrencyKit库实现高并发、低延迟的日志传输。

## 2. 架构设计

### 2.1 整体架构

```
Application
     |
     v
tglog_client (主入口)
     |
     +-- tglog_discoverer (DNS解析)
     |
     +-- tglog_worker[] (多工作线程)
     |        |
     |        +-- ck_ring_t (无锁消息队列)
     |        +-- tglog_conn_list (连接池)
     |        +-- event_base (事件循环)
     |
     +-- tglog_metrics (统计指标)
     |
     +-- tglog_debug_logger (调试日志)
```

### 2.2 核心组件

#### 2.2.1 tglog_client (客户端主控制器)
- **职责**: 统一管理所有组件，提供对外API接口
- **主要功能**:
  - 初始化和配置管理
  - Worker线程调度 (轮询方式分发消息)
  - 统计信息聚合
  - 生命周期管理

#### 2.2.2 tglog_worker (工作线程)
- **职责**: 处理日志发送的核心逻辑
- **关键特性**:
  - 基于libevent的事件驱动架构
  - 无锁环形队列接收消息 (ck_ring_t)
  - 连接池管理
  - 批量处理和重传机制
  - 独立的统计指标

#### 2.2.3 tglog_discoverer (服务发现)
- **职责**: DNS域名解析和服务端点管理
- **核心功能**:
  - 定时DNS解析 (可配置间隔)
  - 端点变更检测 (增加/删除IP)
  - 异步回调通知Worker
  - 支持负载均衡场景

#### 2.2.4 tglog_conn (连接管理)
- **职责**: 单个网络连接的封装
- **特性**:
  - 支持TCP/UDP协议
  - 半长连接模式 (可配置生命周期)
  - 指数退避重连策略
  - 发送缓冲区管理

#### 2.2.5 tglog_metrics (性能监控)
- **职责**: 实时统计和性能监控
- **统计维度**:
  - 全局聚合指标
  - 单Worker粒度指标
  - Counter类型: 计数累加
  - Gauge类型: 瞬时状态

## 3. 性能设计

### 3.1 无锁设计
- **消息队列**: 使用ConcurrencyKit的ck_ring实现无锁环形队列
- **统计更新**: 使用原子操作 (ck_pr_*) 避免锁竞争
- **内存管理**: 预分配缓冲区，减少动态分配

### 3.2 批量处理
- **触发条件**:
  - 达到最大消息数量 (默认50条)
  - 达到最大批次大小 (默认40KB)
  - 超时时间到达 (默认10ms)
- **协议限制**:
  - TCP: 最大512KB
  - UDP: 最大65507字节

### 3.3 连接复用
- **连接池**: 每个Worker维护独立连接池
- **负载均衡**: 轮询选择可用连接
- **生命周期**: 半长连接，支持CLB场景

## 4. 数据流设计

### 4.1 消息流转路径

```
应用调用 tglog_client_send()
    ↓
选择Worker (轮询)
    ↓
消息入队 (无锁队列)
    ↓
Worker事件循环处理
    ↓
批量聚合
    ↓
选择连接发送
    ↓
回调通知结果
```

### 4.2 重传机制
- **触发条件**: 网络错误、连接断开、超时
- **策略**: 指数退避，初始100ms，最大10秒
- **限制**: 可配置最大重传次数 (默认3次)

### 4.3 错误处理
- **连接级错误**: 标记连接不可用，尝试其他连接
- **端点级错误**: 标记端点不可用，定时尝试恢复
- **消息级错误**: 通过回调函数通知应用层

## 5. 配置系统

### 5.1 tglog_conf 结构体
```c
typedef struct tglog_conf {
    tglog_network network;              // TCP/UDP协议选择
    char host[256];                     // 服务器地址
    int port;                           // 服务器端口
    int update_interval;                // DNS更新间隔
    int conn_timeout;                   // 连接超时
    int write_high_watermark;           // 写缓冲区高水位
    int socket_send_buffer_size;        // socket发送缓冲区大小
    int socket_recv_buffer_size;        // socket接收缓冲区大小
    int max_conn_lifetime;              // 连接生命周期
    int worker_num;                     // Worker线程数
    int batching_max_messages;          // 批次最大消息数
    int batching_max_size;              // 批次最大大小
    int batching_max_publish_delay;     // 批次超时时间
    int max_pending_messages;           // 队列最大容量
    int send_timeout;                   // 发送超时
    int max_retries;                    // 最大重传次数
    int stop_timeout;                   // 停止超时时间
    can_log_func can_log;               // 调试日志过滤
    write_log_func write_log;           // 调试日志输出
    void *user_data;                    // 调试日志回调的用户数据
    size_t message_pool_cap;            // 消息池容量
    size_t payload_pool_256b_cap;       // 256字节负载池容量
    size_t payload_pool_512b_cap;       // 512字节负载池容量
    size_t payload_pool_1kb_cap;        // 1KB负载池容量
    size_t payload_pool_2kb_cap;        // 2KB负载池容量
    size_t payload_pool_4kb_cap;        // 4KB负载池容量
    size_t payload_pool_8kb_cap;        // 8KB负载池容量
    size_t payload_pool_16kb_cap;       // 16KB负载池容量
    size_t payload_pool_32kb_cap;       // 32KB负载池容量
    size_t payload_pool_48kb_cap;       // 48KB负载池容量
    size_t payload_pool_64kb_cap;       // 64KB负载池容量
} tglog_conf;
```

### 5.2 默认配置
- Worker线程数: 8个
- 批次最大消息数: 100条
- 批次最大大小: 32KB
- 批次超时: 10ms
- 连接生命周期: 300秒
- DNS更新间隔: 60秒
- 写缓冲区高水位: 32MB
- Socket发送缓冲区: 16MB
- Socket接收缓冲区: 1MB
- 消息池容量: 1000000
- 各级负载池容量: 20000~1000不等

## 6. 内存管理

### 6.1 内存布局
- **静态分配**: 配置结构体、统计结构体
- **预分配**: 消息队列缓冲区、批次缓冲区
- **动态分配**: 连接对象、端点列表
- **内存池**: 消息对象池和多级负载池

### 6.2 内存池设计
- **消息池**: 复用消息对象，默认容量1000000
- **多级负载池**: 根据消息大小分级管理
  - 256B池: 20000容量
  - 512B池: 20000容量
  - 1KB池: 20000容量
  - 2KB池: 10000容量
  - 4KB池: 10000容量
  - 8KB池: 10000容量
  - 16KB池: 5000容量
  - 32KB池: 5000容量
  - 48KB池: 1000容量
  - 64KB池: 1000容量

### 6.3 零拷贝支持
- **TGLOG_MSG_F_NONE**: 零拷贝模式，不复制不释放
- **TGLOG_MSG_F_FREE**: 客户端负责释放，减少拷贝
- **TGLOG_MSG_F_COPY**: 自动拷贝模式，安全但有性能开销

## 7. 线程模型

### 7.1 线程架构
- **主线程**: 应用线程，调用API接口
- **Discoverer线程**: DNS解析线程 (libevent驱动)
- **Worker线程**: 日志处理线程 (多个，libevent驱动)

### 7.2 线程间通信
- **消息队列**: 主线程到Worker线程 (无锁队列)
- **端点通知**: Discoverer到Worker (事件通知)
- **回调通知**: Worker到主线程 (回调函数)

## 8. 错误码设计

### 8.1 错误码分类
```c
#define TGLOG_ERR_OK                        0
#define TGLOG_ERR_SEND_TIMEOUT              10001
#define TGLOG_ERR_SEND_FAILED               10002
#define TGLOG_ERR_PRODUCER_CLOSED           10003
#define TGLOG_ERR_SEND_QUEUE_IS_FULL        10004
#define TGLOG_ERR_NO_AVAILABLE_CONN         10005
#define TGLOG_ERR_NEW_CONN_FAILED           10006
#define TGLOG_ERR_CONN_WRITE_FAILED         10007
#define TGLOG_ERR_CONN_READ_FAILED          10008
#define TGLOG_ERR_LOG_TOO_LONG              10009
#define TGLOG_ERR_BAD_LOG                   10010
#define TGLOG_ERR_BATCH_SIZE_EXCEEDED       10011
#define TGLOG_ERR_MEMORY_ALLOC_FAILED       10012
#define TGLOG_ERR_WORKER_BUSY               10013
#define TGLOG_ERR_NO_MATCH_REQ_FOR_RSP      10014
#define TGLOG_ERR_CONN_CLOSED_BY_PEER       10015
#define TGLOG_ERR_CONN_EAGAIN               10016
#define TGLOG_ERR_UNKNOWN                   20001
```

### 8.2 错误处理策略
- **可恢复错误**: 自动重试 (网络错误、超时)
- **配置错误**: 立即返回错误码
- **资源错误**: 记录日志，尝试清理资源

## 9. 调试和监控

### 9.1 调试日志系统
- **可插拔设计**: 支持用户自定义日志函数
- **分级过滤**: 支持日志级别过滤
- **线程安全**: 所有日志函数必须线程安全

### 9.2 性能指标
- **实时统计**: 原子操作保证数据一致性
- **多维度**: 全局+Worker维度统计
- **JSON输出**: 便于监控系统集成

## 10. 协议兼容性

### 10.1 TGLog协议格式
```
格式: tglogname|v1|v2|...|vn\n
示例: login|2025-06-06 12:00:00|alice|success\n
```

### 10.2 网络协议
- **TCP**: 可靠传输，支持大批次
- **UDP**: 高性能，适合小批次
- **应用层**: 明文传输，仅请求无响应

## 11. 扩展性设计

### 11.1 模块化架构
- **组件解耦**: 各模块独立，便于测试和维护
- **接口抽象**: 标准化的内部接口
- **配置驱动**: 行为可通过配置调整

### 11.2 性能扩展
- **水平扩展**: 增加Worker线程数
- **连接扩展**: 支持连接池大小调整
- **批次优化**: 可调整批次参数适应不同场景

## 12. 安全考虑

### 12.1 内存安全
- **边界检查**: 所有缓冲区操作都有边界检查
- **资源清理**: 确保所有资源正确释放
- **空指针检查**: 防御性编程，检查空指针

### 12.2 网络安全
- **输入验证**: 验证日志格式和长度
- **连接管理**: 防止连接泄漏
- **错误处理**: 优雅处理各种异常情况