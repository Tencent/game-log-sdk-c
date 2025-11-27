# TGLog 单线程版本 SDK 需求文档

## 1. 概述

TGLog 单线程版本 SDK 是专为游戏服务器环境设计的日志客户端库。由于游戏服务器通常采用单线程架构，无法使用多线程版本的SDK，因此需要提供一个纯单线程的实现版本。

## 2. 目标场景

- **目标用户**: 单线程架构的游戏服务器
- **使用场景**: 游戏服务器进程中的日志上报
- **架构要求**: 完全单线程，无任何后台线程
- **性能目标**: 每秒10万-20万条日志（约300字节/条）

## 3. 核心需求

### 3.1 基本要求

- **语言标准**: C99标准
- **线程模型**: 纯单线程，不创建任何后台线程
- **依赖库**: libevent（用于网络事件处理）
- **兼容性**: API接口与多线程版本保持一致

### 3.2 日志格式

- 与多线程版本完全一致
- 格式: `tglogname|v1|v2|...|vn\n`
- tglogname: 日志名称
- v1-vn: 日志字段值
- |: 字段分隔符
- \n: 日志结束符

### 3.3 传输协议

- 支持TCP和UDP两种协议
- 应用层明文传输
- 支持批量传输
- 仅请求无响应模式
- **批量限制**:
    - TCP请求最大512KB
    - UDP请求最大65507字节

### 3.4 批量处理

- **触发条件**:
    - 达到最大消息数量（可配置）
    - 达到最大批次大小（可配置）
    - 达到批次超时时间（可配置）
- **配置参数**:
    - batching_max_messages: 批次最大消息数
    - batching_max_size: 批次最大大小
    - batching_max_publish_delay: 批次超时时间

### 3.5 连接管理

- 支持域名和IP地址
- 半长连接模式（连接生命周期可配置）
- 支持CLB负载均衡场景
- 域名解析更新（可配置更新间隔）
- **重连策略**: 指数退避，初始10秒，最大60秒

### 3.6 重传机制

- 批次重传支持指数退避
- 初始间隔100ms，最大间隔10000ms
- 重传次数可配置

### 3.7 事件驱动模型

- 基于libevent的事件循环
- 内部维护独立的event_base（自包含）
- 支持定时器事件（批次超时、重传、DNS更新等）
- 非阻塞I/O操作
- 通过update API与应用程序集成

### 3.8 内存管理

- 使用简单的内存池避免频繁分配
- 预分配批次缓冲区
- 消息队列使用循环缓冲区

### 3.9 错误处理

- 发送函数支持回调通知
- 回调包含成功状态、错误码、错误消息
- 用户可通过回调知道具体哪条日志发送失败

### 3.10 调试功能

- 支持调试日志打印
- 调试日志函数可由用户注入
- 日志级别可配置

### 3.11 性能监控

- 提供与多线程版本相同的统计指标
- 支持实时查询各项性能指标
- 指标类型包括Counter和Gauge

## 4. 与多线程版本的关系

### 4.0 代码组织原则

- **完全隔离**: 单线程版本位于tglog-st目录，多线程版本位于tglog目录
- **命名一致**: 所有结构体、函数、文件名完全相同，无需_st后缀
- **逻辑对应**: 保持相同的模块划分和职责，仅实现方式不同
- **编译区分**: 通过不同的库名区分（libtglog.so vs libtglog-st.so）

### 4.1 架构差异

- **无Worker线程**: 所有处理在主线程完成
- **无Discoverer线程**: DNS解析在主线程定时处理
- **无锁数据结构**: 不需要使用ConcurrencyKit的无锁队列
- **简化的消息队列**: 使用简单的循环缓冲区

### 4.2 性能差异

- **吞吐量**: 10万-20万条/秒（多线程版本100万条/秒）
- **延迟**: 略高于多线程版本（因为单线程处理）
- **CPU使用**: 单核CPU使用率较高

### 4.3 使用方式差异

- **自包含设计**: 内部维护event_base，无需外部提供
- **定时调用**: 应用程序需定时调用update函数（多线程版本不需要）
- **无需tglog_use_thread**: 不需要初始化线程支持
- **API完全兼容**: 除了update函数外，其他API与多线程版本完全相同

## 5. 使用示例

```c
#include "tglog/client.h"

// 全局日志客户端
tglog_client *g_log_client = NULL;

// 游戏逻辑中的日志发送示例
void on_player_login(int userid, const char *username) {
    char log_buf[1024];
    snprintf(log_buf, sizeof(log_buf), 
        "player_login|userid=%d|username=%s|time=%ld\n",
        userid, username, time(NULL));
    tglog_client_send(g_log_client, log_buf, NULL, NULL);
}

void on_player_action(int userid, const char *action, int param) {
    char log_buf[1024];
    snprintf(log_buf, sizeof(log_buf),
        "player_action|userid=%d|action=%s|param=%d\n",
        userid, action, param);
    tglog_client_send(g_log_client, log_buf, NULL, NULL);
}

int main() {
    // 创建配置
    tglog_conf *conf = tglog_conf_create(
        TGLOG_NETWORK_TCP, 
        "log.server.com", 
        8080, 
        NULL, NULL, NULL
    );
    
    // 创建客户端（内部自动创建event_base）
    g_log_client = tglog_client_create(conf);
    tglog_conf_destroy(conf);
    
    // 游戏主循环
    while (game_running) {
        // 游戏逻辑处理（内部会根据游戏事件调用日志发送）
        game_logic();  // 内部会调用on_player_login、on_player_action等
        
        // 更新日志客户端（非阻塞，处理批次发送、重传等）
        tglog_client_update(g_log_client);
    }
    
    // 销毁客户端（内部会自动flush，确保所有日志发送完成）
    tglog_client_destroy(g_log_client);
    g_log_client = NULL;
    return 0;
}
```

## 6. API接口设计

### 6.1 核心API（与多线程版本兼容）

```c
// 创建客户端
tglog_client *tglog_client_create(const tglog_conf *conf);

// 发送日志
int tglog_client_send(tglog_client *client, const char *log_payload, 
                     tglog_send_callback callback, void *user_data);

// 获取统计指标
tglog_metrics *tglog_client_get_metrics(tglog_client *client);

// 销毁客户端（内部会自动flush所有待发送日志）
void tglog_client_destroy(tglog_client *client);
```

### 6.2 单线程特有API

```c
// 更新处理（需要定时调用，非阻塞）
void tglog_client_update(tglog_client *client);
```

## 7. 配置参数

### 7.1 基本配置（与多线程版本一致）

- network: 网络协议（TCP/UDP）
- host: 服务器地址
- port: 服务器端口
- update_interval: DNS更新间隔
- conn_timeout: 连接超时时间
- max_conn_lifetime: 连接生命周期

### 7.2 批量配置

- batching_max_publish_delay: 批次最大延迟
- batching_max_messages: 批次最大消息数
- batching_max_size: 批次最大大小

### 7.3 缓冲区配置

- max_pending_messages: 消息队列最大容量
- write_high_watermark: 写缓冲区大小
- socket_send_buffer_size: Socket发送缓冲区

### 7.4 重传配置

- send_timeout: 发送超时时间
- max_retries: 最大重传次数

### 7.5 单线程特有配置

- max_update_us: 单次update最大处理时间（默认1ms，避免阻塞）
- max_messages_per_update: 单次update处理的最大事件数（默认100）

## 8. 限制和约束

### 8.1 性能限制

- 吞吐量上限: 20万条/秒
- 单次处理时间: 不超过10ms
- 消息队列容量: 默认40万条

### 8.2 使用限制

- 必须在单线程环境中使用
- 需要定期调用update函数
- 不支持并发调用API

### 8.3 兼容性约束

- API接口与多线程版本保持一致
- 配置参数与多线程版本兼容
- 错误码与多线程版本一致

## 9. 测试要求

### 9.1 功能测试

- TCP/UDP协议发送测试
- 批量处理测试
- 重连和重传测试
- DNS更新测试

### 9.2 性能测试

- 吞吐量测试（目标10万-20万条/秒）
- 延迟测试
- 内存使用测试
- CPU使用率测试

### 9.3 稳定性测试

- 长时间运行测试
- 网络异常处理测试
- 内存泄漏检测

## 10. 交付物

### 10.1 源代码

- tglog-st目录下的所有源文件
- 头文件和实现文件分离
- 完整的注释和文档

### 10.2 文档

- 需求文档（本文档）
- 设计文档
- API使用文档
- 集成指南

### 10.3 示例和测试

- 使用示例代码
- 单元测试代码
- 性能测试工具

## 11. 时间计划

- 需求分析和设计: 已完成
- 核心功能实现: 2天
- 测试和优化: 1天
- 文档完善: 0.5天