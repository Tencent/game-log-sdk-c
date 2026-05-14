# TGLog SDK C 更新日志

## v1.1.0 (2026-05-14)

### 🎉 重要变更

- **Windows 平台支持**: 完整支持 Windows + MSVC 构建，包括多线程和单线程版本
  - 使用 CMake + `add_subdirectory(event)` 构建 libevent，无需手动配置
  - Windows 上不依赖 ConcurrencyKit 和 jemalloc
  - 使用 Windows 原生线程 API 替代 pthread
  - 链接 `ws2_32` 实现 Windows Sockets 支持
- **移除 ConcurrencyKit 依赖**: 使用自研跨平台 C99 实现替代 CK 库
  - `tglog_atomic.h`: 原子操作（GCC/Clang `__atomic_*` + MSVC `_InterlockedXxx`）
  - `tglog_ring.h`: SPSC/MPSC 无锁环形缓冲区，带缓存行填充
  - `tglog_stack.h`: MPMC 无锁栈，双宽度 CAS 实现 ABA 防护
  - `tglog_ht.h/c`: 单线程哈希表，开放寻址法
- **ARM 架构支持**: 支持 ARM/ARM64 架构编译和运行
  - MSVC ARM/ARM64 使用 `_Interlocked*_acq/_rel` 内在函数
  - ARM 上使用 `__yield()` 替代 `_mm_pause()`
  - 修复 32 位 ARM 上 `long` 类型溢出，改用 `int64_t`
  - 链接 `-latomic` 支持 128 位 CAS 操作

### 🐛 Bug 修复

- **连接事件处理**: 忽略已退役连接上的事件回调，避免操作已释放的资源（极低概率）
- **evbuffer 泄漏**: 修复数据抢救后 evbuffer 对象未释放的内存泄漏（极低概率）
- **延迟关闭 UAF**: 防止 `close_conn_delay` 重入导致的 use-after-free（极低概率）
- **32 位溢出**: 修复 `request.c/h` 和 `worker.c` 中时间计算的 `long` 类型溢出（极低概率）
- **TCP 断连感知慢**: 服务端进程退出/重启时，客户端要等到 `max_conn_lifetime` 到期（约 2 分钟）才换连接（tglog、tglog-st）
  - 在 TCP bufferevent 上 enable `EV_READ | EV_WRITE`，让 peer FIN/RST 通过 `BEV_EVENT_EOF`/`BEV_EVENT_ERROR` 立即触发 `tglog_worker_on_conn_error`
  - `tglog_worker_on_read` 改为 drain 残余字节而不是 disable EV_READ，使 EOF 检测持续生效
  - 把 connect 超时从 `bufferevent_set_timeouts` 的 read 槽挪到 write 槽，避免开启 EV_READ 后周期性触发 `BEV_EVENT_READING | BEV_EVENT_TIMEOUT` 把空闲健康连接断掉
  - 在 `tglog_worker_on_conn_connected` 中清空 bufferevent 超时，连上之后由 `max_conn_lifetime` + TCP keepalive 管理连接生命期
  - `BEV_EVENT_EOF` 日志降级为 INFO，对端正常关闭不再被当作错误

### ✨ 新特性

- **跨平台兼容层** (`common/tglog_platform.h`):
  - 抽象平台差异（Winsock 初始化、socket 错误处理等）  

### 🔧 优化改进

- **公共代码抽取**: 将 tglog 和 tglog-st 的公共结构体和函数提取到 `common/` 目录
- **平台适配**: 使用 libevent 抽象替代 Unix 特有的 socket API 调用
- **构建配置优化**: CMake 配置改进，支持 MSVC 编译器标志隔离

### 🔨 构建与配置

- CMake 支持 Windows MSVC 构建（`add_subdirectory(event)` 集成 libevent）
- Makefile 移除 x86 特有的 `-m64` 标志，仅在 x86-64 上条件应用 `-mcx16`
- 链接依赖从 `-lck` 更新为 `-latomic`
- Windows 上 jemalloc 使用 dummy INTERFACE target
- 测试程序在 Windows 阶段暂时跳过
- Python 绑定支持 Windows DLL 路径注入

### 📚 文档更新

- 添加 Windows 构建支持文档
- 更新所有 README 中的链接依赖说明（`-lck` → `-latomic`）
- 更新 CLAUDE.md 项目指导文档

### 📝 已知限制更新

- ~~不支持 Windows 平台~~ → **已支持** Windows (MSVC 2019+)
- 新增支持架构: x86_64, ARM, ARM64

---

## v1.0.0 (2026-02-13)

### 🎉 重要变更

- **移除 Java 绑定**: 删除了 JNI 绑定实现，推荐使用独立的纯 Java 版本 [SDK](https://git.woa.com/tglog/v3/sdk-java)
  - 删除 `bindings/java/` 目录及所有相关文件
  - 删除 `bindings/ALL_BINDINGS_COMPARISON.md` 文档
  - 更新构建脚本和文档，移除 Java 相关引用
  - 保留 Python 绑定（ctypes、C Extension、Cython）

### ✨ 新特性

- **IPv6 支持**: endpoint 连接支持 IPv6 地址

### 🐛 Bug 修复

- **批次重试**：
  - 重试截止时间修改为 backoff 的累计时间
  - 重试期间也检查是否发送超时
- **内存安全**:
  - 修复 `tmp_message` 双重释放问题
  - 修复连接关闭时潜在的 fd 泄漏
  - 修复 `tglog_conn_create` 中 `strdup` 返回值未检查的问题
- **线程安全**:
  - 修复 `localtime()` 非线程安全问题，改用 `localtime_r()`（tglog、tglog-st）
  - 修复 `rand()` 非线程安全问题，改用 `rand_r()` 配合 per-worker seed（tglog、tglog-st）
- **连接管理**:
  - 修复创建 bufferevent 时 `BEV_OPT_CLOSE_ON_FREE` 导致的 socket 双重关闭（tglog、tglog-st）
  - 修复 `retry_deadline.tv_usec` 潜在溢出问题
- **信号处理**:
  - 忽略 SIGPIPE 信号，避免写入已关闭连接时进程被终止

### 🔧 优化改进

- **返回值与回调**:
  - `tglog_client_send` 返回错误码时不触发回调，如果 msg_flag 传入 `TGLOG_MSG_F_FREE` 仍然帮调用者释放内存
- **连接初始化**:
  - tglog-st 在服务端节点数小于等于 2 时，每节点连接数从 4 改为 3，减少连接数并降低内存占用
- **连接恢复机制增强**:
  - `emergency_supplement_event` 定时器周期改为 5s
- **节点更新处理顺序**:
  - 先处理增加节点再处理删除节点
- **日志输出**:
  - 将调试日志的多次 `fprintf` 合并为单次 `fputs` 写入，避免多线程下日志交错（tglog、tglog-st）
- **测试程序改进**：
  - test 程序使用令牌桶控制发送速度
  - test-st 程序使用令牌桶控制发送速度

### 📚 文档更新

- 添加 Java SDK 迁移说明，指向纯 Java 版本
- test-tapp 新增 UDP 场景测试限制说明文档
- 增加英文版教程
- 更新所有文档中的语言绑定说明

### 🔨 代码质量

- 修复静态代码检查问题
- 删除无用参数
- 删除无用函数
- 删除快捷宏定义
- 为 `debug_logger` 中的字符串字面量指针添加 `const` 修饰
- 修正 `size_t` 变量的无符号比较警告
- 修正 tglog-st client 中的类型不匹配问题

### 🔨 构建与配置

- 简化构建系统，移除 Java 相关构建目标
- 更新 Makefile 和 CMakeLists.txt
- 精简 bindings 目录结构




## v1.0.0-rc.3 (2025-12-12)

### 🐛 Bug 修复

- **内存泄漏修复**: 修复了连接创建失败时的多处内存泄漏问题
  - 修复 `tglog_worker_supplement_conn` 中 `add_connecting_conn` 失败后的连接对象泄漏（2处）
  - 修复 `tglog_worker_replace_conn` 中 `add_connecting_conn` 失败后的连接对象泄漏
  - 修复 `tglog_worker_add_conn_by_endpoint` 中 `add_connecting_conn` 失败后的连接对象泄漏
  - 修复 `tglog_worker_init_conns` 中 `add_connecting_conn` 失败后的连接对象泄漏和统计不准确问题
  - 修复 `tglog_worker_recover_endpoints` 中 `add_connecting_conn` 失败后的连接对象和 `user_data` 泄漏

### 🔧 优化改进

- **连接恢复机制增强**: 新增紧急连接补充机制，提升连接恢复速度
  - 添加 `emergency_supplement_event` 定时器（每 5 秒检查一次），在 worker READY 后自动启动
  - 在 `tglog_worker_get_conn` 中添加频率限制（2 秒间隔），避免无可用连接时频繁尝试创建
  - 添加 `last_emergency_supplement_time` 字段记录上次紧急补充时间
  - 三层连接恢复机制：
    1. 发送时检查（2秒间隔）- 在 `tglog_worker_get_conn` 中触发
    2. 定时主动检查（5秒间隔）- 通过 `emergency_supplement_event` 定时器触发
    3. Endpoint 恢复（30秒间隔，指数退避）- 通过 `recover_endpoint_event` 定时器触发
  - 解决了在 batch retry backoff 期间服务器恢复但连接无法及时重建的问题

- **过期连接替换机制优化**: 避免总是从第0个节点开始替换，其他节点最终无连接处于饥饿状态
  - 使用轮循代替固定从0号节点开始的替换策略，避免总是替换到0号节点，其他节点最终无连接处于饥饿状态

- **配置优化**: 调整默认配置参数以提升性能
  - `batching_max_messages`: 从 50 调整为 20，降低单次发送调用消耗
  - `max_update_us`: 从 200µs 调整为 300µs，减少更新超时
  - `max_messages_per_update`: 从 1000 调整为 200，降低单次发送调用消耗
  - 优化 `tglog_worker_handle_msg` 中的更新时间检查频率：从每 10 条消息或 8192 字节检查改为每 5 条消息或 2048 字节检查

- **日志增强**:
  - 添加更多调试日志，提升问题排查能力
  - 调整部分日志级别，减少不必要的日志输出

- **测试程序改进**：
  - test-tapp 统计每次发送的时间并获取最大值


### 📚 文档更新

- 新增限制说明文档，明确系统使用限制和注意事项

### 🔨 代码质量

- 改进错误处理逻辑，确保所有资源正确释放
- 统一连接创建失败后的清理流程
- 增强代码注释，提升可维护性

### 🔨 构建与配置

- 支持自动下载依赖，便于非git用户编译


## v1.0.0-rc.2 (2025-12-05)

### ✨ 新特性

- **测试程序增强**: test-tapp 支持发送不同大小的日志（250B-63KB），95% 为 1KB 以内，5% 为大于 1KB，模拟真实场景并测试内存池性能
- **新增错误码**: `TGLOG_ERR_BAD_FLAG` (10017) - 用于标识消息标志参数无效的情况
- **新增指标**: `rescued_msg_bytes` - 用于统计连接异常关闭时抢救的字节数，通常极少发生

### 🔧 优化改进

- **连接管理优化**: 当发送缓冲区超过高水位线时，不再关闭连接，避免关闭连接造成脏数据
- **连接管理优化**: 连接过期时间加一[20,30]随机数，检测周期为生命周期1/10，最多25s，避免关闭连接同时切换
- **重试策略优化**: 将默认最大重试次数从 3 次增加到 10 次，提高在网络不稳定情况下的可靠性
- **日志增强**:  
  - 在关键路径添加更多调试日志，提升可观测性
- **API 改进**:
  - 将部分返回 `int` 的函数改为返回 `bool`，使语义更清晰
  - 将 `msg_flags` 参数重命名为 `msg_flag`，保持命名一致性，只允许使用一个值
  - 增加对 `msg_flag` 参数的合法性校验
  - msg_flag改为enum类型
- **测试程序改进**：
  - test-tapp 将 tglog client 和 tlog API 逻辑隔离，避免同时使用
  - test-tapp 调试打印最大日志长度，大日志数量，最大API调用耗时，方便对比 tglog client 和 tlog API的调用延迟
  - 优化 test-tapp 调试日志回调，增加文件名、行号、函数名信息，便于问题定位，也用于展示如何实现可插拨的调试日志
### 🐛 Bug 修复

- **指标统计修复**: 修复了多个指标统计相关的 bug，确保统计数据的准确性
- **test-tapp**: 修复发送完成就不调用`tglog_client_update()`的 bug，确保 client 持续更新

### 📚 文档更新

- 新增教程文档 (TUTORIAL_TGLOG.md 和 TUTORIAL_TGLOG-ST.md)，帮助用户快速上手
- 添加 sysctl.conf 配置说明，指导用户优化系统参数以获得更好的性能

### 🔨 构建与配置

- test-tapp 默认使用 TCP 协议，而非 UDP
- 使用默认配置简化初始化流程
- 添加 tsf4g 库支持，便于与 TAPP 框架集成

## v1.0.0-rc.1 (2025-11-27)

### 🎉 首个候选发布版本

这是 TGLog SDK C 语言客户端的第一个候选发布版本（Release Candidate），提供了完整的高性能日志传输功能。

### ✨ 核心特性

**双版本架构**
- ✅ 多线程版本 (tglog): 支持 100万条/秒的高吞吐量日志传输
- ✅ 单线程版本 (tglog-st): 支持 10-20万条/秒，适用于游戏服务器和单线程应用
- ✅ API 完全兼容，两个版本可以无缝切换

**网络协议支持**
- ✅ TCP 协议: 可靠传输，支持批次最大 512KB
- ✅ UDP 协议: 高性能传输，支持批次最大 65507 字节
- ✅ 自动重连机制，支持指数退避策略
- ✅ DNS 定时更新，支持域名变更和负载均衡

**性能优化**
- ✅ 无锁消息队列 (基于 ConcurrencyKit)
- ✅ 批量发送优化，减少系统调用
- ✅ 零拷贝支持 (TGLOG_MSG_F_NONE)
- ✅ 内存池管理，支持 jemalloc 集成
- ✅ 可配置的批次大小、延迟和重传策略

**监控与调试**
- ✅ 丰富的性能统计指标 (Counter 和 Gauge 类型)
- ✅ JSON 格式输出，便于监控系统集成
- ✅ 可插拔的调试日志系统
- ✅ 全局和 Worker 粒度的指标统计

**语言绑定**
- ✅ Python 三种实现: ctypes、C 扩展、Cython
- ✅ 完整的配置和统计 API
- ✅ 线程安全的回调机制

> 说明：如需使用Java，请使用纯Java版本的[SDK](https://git.woa.com/tglog/v3/sdk-java)

**构建系统**
- ✅ Makefile 构建支持，简单易用
- ✅ CMake 构建支持，适合 IDE 集成
- ✅ Git Submodules 自动管理依赖库
- ✅ 一键构建所有组件 (库、测试、绑定)

**第三方依赖**
- ✅ libevent 2.1.12-stable: 高性能事件库
- ✅ jemalloc 5.3.0: 高性能内存分配器
- ✅ ConcurrencyKit 0.7.2: 无锁数据结构库

### 📦 交付内容

**核心库**
- `libtglog.a` / `libtglog.so` - 多线程版本（静态/动态库）
- `libtglog-st.a` / `libtglog-st.so` - 单线程版本（静态/动态库）

**测试程序**
- `test/tglog_test` - 多线程版本测试程序
- `test-st/tglog_test_st` - 单线程版本测试程序
- `test-st/simple_perf_test_st` - 性能测试程序
- `test-tapp/tglog_tapp` - TAPP 框架集成示例

**语言绑定**
- Python 三种实现（ctypes、C 扩展、Cython）

**文档**
- 完整的 README 和使用指南
- 架构设计文档 (DESIGN.md)
- Git Submodules 管理指南
- CMake 构建指南
- 语言绑定文档

### 🔧 技术规格

**系统要求**
- C99 标准编译器 (GCC 4.9+, Clang 3.9+)
- Linux, Windows
- x86_64 架构

**性能指标**
- 多线程版本: ~100万条/秒 (8 workers)
- 单线程版本: ~10-20万条/秒
- Python Cython: ~128万条/秒 (105% of C baseline)

### 📝 已知限制

- 不支持 Windows 平台（ConcurrencyKit 限制）
- 单条日志消息除了结尾不能包含 `\n` 字符，如果日志内部要包含`\n`或`|`，需要把这2个字符转义
- UDP 模式下单个批次不能超过 65507 字节
- 因为协议是只有请求没有响应的，发送成功仅表示数据写入网络缓冲区成功，不表示服务器一定收到

### 🚀 快速开始

```bash
# 克隆并构建
git clone --recurse-submodules <repository-url>
cd sdk-c
make

# 安装到 dist 目录
make install

# 运行测试
cd test
./tglog_test
```

### 📚 文档

- [README.md](README.md) - 项目概述和快速开始
- [tglog/DESIGN.md](tglog/DESIGN.md) - 多线程版本设计文档
- [tglog-st/DESIGN.md](tglog-st/DESIGN.md) - 单线程版本设计文档
- [bindings/python/README.md](bindings/python/README.md) - Python 绑定文档

### 🙏 致谢

感谢以下开源项目：
- [libevent](https://libevent.org/) - 高性能事件通知库
- [jemalloc](http://jemalloc.net/) - 高性能内存分配器
- [ConcurrencyKit](http://concurrencykit.org/) - 并发编程原语库

---

## 0.0.0 (2025-11-27)

### 特性

- 项目初始化

### 改进

无

### Bug修复

无