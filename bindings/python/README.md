# TgLog Python 绑定

TgLog 提供三种不同的 Python 绑定实现，以满足不同的性能和部署需求：

1. **ctypes** - 纯 Python 实现，无需编译
2. **C 扩展** - 原生 Python C API 扩展
3. **Cython** - 高性能 Cython 扩展

## 平台要求

- **操作系统**：Linux 或 macOS（不支持 Windows）
- **Python 版本**：3.6 或更高版本
- **C 库**：必须先构建 libtglog.so

## 快速开始

```bash
# 首先构建 C 库
cd ..
make

# 构建所有 Python 绑定
cd bindings
make python

# 或构建特定绑定
make python_ctypes    # 无需编译
make python_ext       # 构建 C 扩展
make python_cython    # 构建 Cython 扩展
```

## 绑定对比

| 特性 | ctypes | C 扩展 | Cython |
|------|--------|--------|---------|
| **吞吐量（中位）** | 39,806 msg/s | 924,485 msg/s | **1,101,937 msg/s** 🏆 |
| **需要编译** | 否 | 是 | 是 |
| **依赖项** | 无 | Python 开发头文件 | Cython + Python 开发头文件 |
| **部署难度** | 最简单 | 中等 | 中等 |
| **支持 Python 回调** | 是（每次进 GIL，吞吐受限） | 否（仅 C 层计数器） | 否（`nogil` 编译期约束） |
| **推荐用途** | 仅测试 / 原型 ❌ | 中等性能需求的生产环境 | **高性能生产系统** ⭐⭐⭐⭐⭐ |

**注意:** 性能数据基于发送 1,000,000 条真实日志消息（TCP，`dev.tglog.com:20001`，8 workers），各 binding 跑 5 次取中位数。测试日期 2026-05-20。

### 测试方法学说明

上述吞吐量数据存在两个**口径不对等**的因素，解读时请留意：

1. **三个 binding 的 C 层回调实现并不一致**：C 扩展用 `pthread_mutex_lock` 串行化计数器（`tglog_ext.c:59`），Cython 直接用 `tglog_atomic_add_64` 原子加（`tglog_cython.pyx:132`，连 mutex 都没有）。在 100 万条消息 + 8 个 worker 的并发回调场景下，Cython 比 C 扩展占便宜，"Cython 高于 C 扩展"在很大程度上反映的是这种实现差异，而不是"Cython 编译出的代码比手写 C 更快"。

2. **ctypes 跑的不是同一种回调路径**：`tglog_test.py` 的吞吐数字来自 `callback=True` 路径，走的是带 `pthread_mutex_lock` 的 Python 函数 trampoline（`tglog_client.py:222-227`），每条消息都要 `PyGILState_Ensure()`。这个数字反映的是"Python 回调 + 多 worker"的真实瓶颈，**不是 ctypes 自身的 FFI 开销上限**。如果改用 fire-and-forget（`callback=None`），ctypes 实测吞吐会高得多——但即便如此也会受限于 ctypes 每次调用 `tglog_client_send` 的参数封送开销，不会接近 C 扩展水平。

**如何选择对比维度**：
- 比较"Python 调用 SDK 的 FFI 开销" → 三种 binding 都走 fire-and-forget，对比 send 时间
- 比较"应用层能拿到回调时的端到端吞吐" → 用 ctypes 真 Python 回调 vs C 扩展/Cython 自己写的 with-gil trampoline，那才是公平比较
- **当前表格反映的是"各 binding 在自己最优路径下的吞吐"**——适合做选型决策（"想要回调用 ctypes，要吞吐用 Cython"），不适合做实现质量评判。

### 关于回调

tglog SDK 是 fire-and-forget 类的日志库，"send 成功"的含义只是消息进入了发送队列，并非服务端确认收到。**三种 binding 在回调上的处理策略不同：**

- **ctypes**：通过 `ctypes.CFUNCTYPE` 把 Python 函数包成 C 函数指针注册给 SDK，worker 线程触发回调时会自动 `PyGILState_Ensure()` 拿 GIL。**功能最完整但性能最低**——每条消息回调都串行化在 GIL 上，这就是 ctypes 吞吐只有 ~40K msg/s 的根因。
- **C 扩展 / Cython**：为了榨取性能，回调被实现为**纯 C 函数**（仅累加成功/失败计数），完全不接触 Python 解释器。`use_callback=True` 只是开关这个 C 层计数器，**不能传入 Python 函数**。如果你需要按消息粒度执行 Python 逻辑，要么改用 ctypes，要么修改 C 扩展源码自行加 `with-gil` trampoline（注意会显著降低吞吐）。

## 1. ctypes 绑定

### 概述
使用 ctypes FFI 的纯 Python 实现。无需编译，直接使用 libtglog.so。

### 安装
```bash
# 无需编译 - 只需确保 libtglog.so 可用
make python_ctypes
```

### 使用示例
```python
from tglog_client import TglogClientPy

# 创建客户端
client = TglogClientPy(protocol="tcp", host="127.0.0.1", port=20001, workers=8)

# 1) Fire-and-forget（最快，不要回调）
client.send("ctypes|1|2|3\n")

# 2) 使用内置 C 计数器统计成功/失败（推荐用于压测）
client.send("ctypes|1|2|3\n", callback=True)
sent, errors = client.get_stats()

# 3) 使用 Python 回调函数（每条消息都会进 GIL，吞吐显著下降）
def on_done(payload, err_code, err_msg):
    if err_code != 0:
        print(f"错误 {err_code}: {err_msg}")

client.send("ctypes|1|2|3\n", callback=on_done)

# 获取 SDK 内部统计信息（JSON dict）
metrics = client.get_metrics()
print(metrics)

# 清理
client.destroy()
```

**注意：** ctypes 绑定是三种实现里**唯一支持 Python 函数回调**的——但代价是每条消息的回调都要获取 GIL，多 worker 线程会被串行化，这是它吞吐远低于 C 扩展和 Cython 的主要原因。如果只需要统计成功/失败计数，请用 `callback=True` 或 `callback="global"`，二者都在 C 层完成计数，不会触发 GIL。

### 优点
- 无需编译
- 易于部署
- 适用于任何 Python 安装
- 适合原型开发

### 缺点
- 比原生扩展略慢
- Python/C 边界带来的回调开销
- 类型安全性有限

## 2. C 扩展

### 概述
使用 Python C API 的原生 Python C 扩展，性能更好。

### 安装
```bash
# 安装 Python 开发头文件（如果尚未安装）
# Ubuntu/Debian:
sudo apt-get install python3-dev

# CentOS:
yum install python3-devel

# macOS（通常已包含在 Python 中）

# 构建扩展
make python_ext
# 或直接运行:
python3 setup_ext.py build_ext --inplace
```

### 使用示例
```python
import tglog_ext

# 创建客户端
client = tglog_ext.TglogClient(protocol="tcp", host="127.0.0.1", port=20001, workers=8)

# Fire-and-forget 发送（最快）
client.send("c_ext|1|2|3\n")

# 启用内置 C 计数器（不会进 Python，性能不受影响）
for i in range(10000):
    client.send(f"c_ext|1|2|3|{i}\n", use_callback=True)

# 查询累计成功/失败计数
stats = client.get_stats()
print(f"成功: {stats['sent']}, 错误: {stats['error']}")

# 获取 SDK 内部 metrics（JSON 字符串）
print(client.get_metrics())

# 清理
client.destroy()
```

**注意：** C 扩展**不支持 Python 函数回调**。`use_callback=True` 启用的是一个纯 C 计数器（在 worker 线程里直接累加，不进 Python 解释器），这正是它能保持高吞吐的关键。如果你需要"每条消息失败时执行 Python 逻辑"这类语义，请使用 ctypes 绑定，或自行修改 `tglog_ext.c` 在 `fast_callback` 里加 `PyGILState_Ensure()` 调用 Python 函数（注意会显著降低吞吐）。

### 优点
- 比 ctypes 性能更好
- 直接 C 回调更高效
- 原生 Python 异常处理
- 比 ctypes 开销更低

### 缺点
- 需要编译
- 平台相关的二进制文件
- 需要 Python 开发头文件

## 3. Cython 扩展

### 概述
高性能 Cython 扩展，开销最小，非常适合大数据应用。

### 安装
```bash
# 安装依赖
pip install cython numpy

# 构建扩展
make python_cython
# 或直接运行:
python3 setup_cython.py build_ext --inplace
```

### 使用示例
```python
from tglog_cython import TglogClient

# 创建客户端
client = TglogClient(
    protocol="tcp",
    host="127.0.0.1",
    port=20001,
    workers=8
)

# send() 接受 bytes，第二个参数是 bool（是否启用 C 层计数器）
log = b"cython|1|2|3\n"

# 1) Fire-and-forget（最高吞吐）
for i in range(10000):
    client.send(log)

# 2) 启用 C 层原子计数器（仍然 nogil，吞吐几乎不受影响）
for i in range(10000):
    client.send(log, use_callback=True)

success, error = client.get_callback_stats()
print(f"成功: {success}, 错误: {error}")

# 获取 SDK 内部 metrics
print(client.get_metrics())

# 清理（也支持 with 语法）
client.destroy()
```

**注意：** Cython 绑定**不支持 Python 函数回调**。`fast_callback` 在 `.pyx` 中被声明为 `noexcept nogil`，编译期就限定了不能接触 Python 对象——这正是它能跑出接近 C 原生性能的原因。`use_callback=True` 启用的是一个纯 C 原子计数器（连 mutex 都没有），用 `get_callback_stats()` 查询。

### 优点
- 最佳性能
- 最小的 Python/C 开销
- 可能的无 GIL 操作
- 适合高吞吐量场景

### 缺点
- 需要安装 Cython
- 构建过程更复杂
- 二进制文件较大

## 测试

运行每个绑定的测试：

```bash
# 测试 ctypes 绑定
make run_ctypes

# 测试 C 扩展
make run_ext

# 测试 Cython 扩展
make run_cython

# 运行全部 Python 测试
make run
```

## 配置选项

各个绑定支持的配置选项有所不同：

### ctypes 绑定配置
```python
from tglog_client import TglogClientPy

# ctypes 通过 TglogClientPy 构造函数传参
client = TglogClientPy(
    protocol="tcp",        # 协议：'tcp' 或 'udp'
    host="127.0.0.1",      # 服务器地址
    port=20001,            # 服务器端口
    workers=8              # 工作线程数
)
# 注：ctypes 版本使用 C 库的默认配置，不支持自定义高级参数
```

### C 扩展配置
```python
# C 扩展支持的参数
client = tglog_ext.TglogClient(
    protocol="tcp",        # 协议：'tcp' 或 'udp'
    host="127.0.0.1",      # 服务器地址
    port=20001,            # 服务器端口
    workers=8              # 工作线程数
)
# 注：C 扩展内部预设了优化参数，不支持完全自定义
```

### Cython 扩展配置
```python
# Cython 支持的参数
client = TglogClient(
    protocol="tcp",        # 协议：'tcp' 或 'udp'
    host="127.0.0.1",      # 服务器地址
    port=20001,            # 服务器端口
    workers=8              # 工作线程数
)
# 注：Cython 版本内部预设了高性能优化参数
```

### 内部优化参数（仅供参考）
C 扩展和 Cython 扩展内部使用了以下优化参数（不可配置）：
- `batching_max_publish_delay`: 10ms
- `batching_max_messages`: 50
- `batching_max_size`: 32KB
- `max_conn_lifetime`: 300秒
- `max_pending_messages`: 409600
- `write_high_watermark`: 32MB
- `send_timeout`: 30000ms
- `stop_timeout`: 3000ms

如需完全自定义配置，建议直接使用 C API 或修改扩展源码。

## 故障排除

### 导入错误：找不到 libtglog.so
```bash
# 设置库路径
export LD_LIBRARY_PATH=/path/to/sdk-c/lib:$LD_LIBRARY_PATH  # Linux
export DYLD_LIBRARY_PATH=/path/to/sdk-c/lib:$DYLD_LIBRARY_PATH  # macOS
```

### Windows 错误
TgLog 不支持 Windows。请使用 Linux 或 macOS。

### Python 版本错误
TgLog 需要 Python 3.6+。升级您的 Python 安装：
```bash
python3 --version  # 检查版本
# 如果 < 3.6，使用包管理器升级
```

### 编译错误
```bash
# 安装所需的开发包
# Ubuntu/Debian:
sudo apt-get install python3-dev build-essential

# CentOS/RHEL:
sudo yum install python3-devel gcc

# macOS:
xcode-select --install  # 安装 Xcode 命令行工具
```

## 选择合适的绑定

- **使用 ctypes 当：**
  - 需要快速原型开发
  - 部署简单性很重要
  - 性能要求适中
  - 想要避免编译

- **使用 C 扩展当：**
  - 需要比 ctypes 更好的性能
  - 正在构建生产系统
  - 需要比 ctypes 更低的开销
  - 可以接受平台相关的部署

- **使用 Cython 当：**
  - 性能至关重要
  - 处理数百万条日志
  - 需要最小开销
  - 构建高吞吐量数据管道

## 许可证

与主 TgLog 项目相同。请参阅根目录中的 LICENSE.txt。