# TGLog SDK C 更新日志

## v1.0.0-rc.2 (2025-12-03)

### ✨ 新特性

- **测试程序增强**: test-tapp 支持发送不同大小的日志（250B-63KB），95% 为 1KB 以内，5% 为大于 1KB，模拟真实场景并测试内存池性能
- **新增错误码**: `TGLOG_ERR_BAD_FLAG` (10017) - 用于标识消息标志参数无效的情况

### 🔧 优化改进

- **连接管理优化**: 当发送缓冲区超过高水位线时，不再关闭连接，避免关闭连接造成脏数据
- **重试策略优化**: 将默认最大重试次数从 3 次增加到 10 次，提高在网络不稳定情况下的可靠性
- **日志增强**:
  - 优化 test-tapp 调试日志回调，增加文件名、行号、函数名信息，便于问题定位，也用于展示如何实现可插拨的调试日志
  - 在关键路径添加更多调试日志，提升可观测性
- **API 改进**:
  - 将部分返回 `int` 的函数改为返回 `bool`，使语义更清晰
  - 将 `msg_flags` 参数重命名为 `msg_flag`，保持命名一致性，只允许使用一个值
  - 增加对 `msg_flag` 参数的合法性校验

### 🐛 Bug 修复

- **指标统计修复**: 修复了多个指标统计相关的 bug，确保统计数据的准确性

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
- ✅ Java JNI 绑定: 支持 DirectByteBuffer 零拷贝，性能达到 C 版本的 56%
- ✅ Python 三种实现: ctypes、C 扩展、Cython
- ✅ 完整的配置和统计 API
- ✅ 线程安全的回调机制

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
- Java JNI 库和示例代码
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
- Linux, macOS, FreeBSD
- x86_64, ARM64 架构

**性能指标**
- 多线程版本: ~100万条/秒 (8 workers)
- 单线程版本: ~10-20万条/秒
- Java DirectByteBuffer: ~50-100万条/秒
- Python Cython: ~90万条/秒

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
- [bindings/java/README.md](bindings/java/README.md) - Java 绑定文档
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