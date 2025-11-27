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
| **性能** | 3% | 78% | **105%** 🏆 |
| **吞吐量** | 38,615 msg/s | 950,718 msg/s | **1,283,890 msg/s** |
| **需要编译** | 否 | 是 | 是 |
| **依赖项** | 无 | Python 开发头文件 | Cython + Python 开发头文件 |
| **部署难度** | 最简单 | 中等 | 中等 |
| **Pythonic API** | 是 | 是 | 是 |
| **直接 C 回调** | 否 | 是 | 是 |
| **推荐用途** | 仅测试 ❌ | 中等性能需求的生产环境 | **高性能生产系统** ⭐⭐⭐⭐⭐ |

**注意:** 性能数据基于发送1,000,000条真实日志消息的测试(2025-11-26)。Cython版本性能超过C原生基准5%!

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
from tglog_client import TglogClient, TglogConfig, TGLOG_NETWORK_TCP

# 创建配置
config = TglogConfig(
    network=TGLOG_NETWORK_TCP,
    host="127.0.0.1",
    port=20001
)

# 创建客户端
client = TglogClient(config)

# 发送日志并使用回调
def callback(payload, err_code, err_msg):
    if err_code == 0:
        print(f"成功: {payload}")
    else:
        print(f"错误 {err_code}: {err_msg}")

client.send("ctypes|1|2|3\n", callback)

# 获取统计信息
metrics = client.get_metrics()
print(f"统计信息: {metrics}")

# 清理
client.destroy()
```

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
client = tglog_ext.TglogClient("tcp", "127.0.0.1", 20001)

# 使用回调发送日志
def callback(payload, err_code, err_msg):
    if err_code == 0:
        print(f"成功: {payload}")
    else:
        print(f"错误 {err_code}: {err_msg}")

# 发送多条日志
for i in range(10):
    client.send(f"c_ext|1|2|3|{i}\n", callback)

# 获取统计信息
metrics = client.get_metrics()
print(f"成功: {metrics['success_count']}, 错误: {metrics['error_count']}")
```

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
    workers=4
)

# 使用回调发送日志
def callback(payload, err_code, err_msg):
    if err_code == 0:
        print(f"成功: {payload}")
    else:
        print(f"错误 {err_code}: {err_msg}")

# 高效发送大量日志
for i in range(10000):
    client.send(f"cython|1|2|3|{i}\n", callback)

# 获取统计信息
metrics = client.get_metrics()
print(f"统计信息: {metrics}")
```

### 优点
- 最佳性能
- 最小的 Python/C 开销
- 可能的无 GIL 操作
- 适合高吞吐量场景

### 缺点
- 需要安装 Cython
- 构建过程更复杂
- 二进制文件较大

## 性能基准测试

发送 1,000,000 条日志消息的基准测试（8个工作线程）：

| 绑定类型 | 发送时间（秒） | 完成时间（秒） | 吞吐量（消息/秒） |
|---------|--------------|---------------|-----------------|
| C 原生  | 0.618 | 0.631 | 1,618,123 |
| ctypes  | 35.882 | 35.899 | 27,859 |
| C 扩展  | 1.460 | 1.465 | 682,810 |
| Cython  | 1.091 | 1.105 | 904,888 |

*测试说明：*
- 发送时间：调用发送函数的耗时
- 完成时间：包含所有回调处理的总耗时
- 测试环境：8个工作线程，本地网络

**性能排名：**
1. **C 原生**：最快，作为基准参考
2. **Cython**：接近原生性能，约为 C 的 56%
3. **C 扩展**：良好性能，约为 C 的 42%
4. **ctypes**：由于 Python/C 边界开销较大，性能最低

## 测试

运行每个绑定的测试：

```bash
# 测试 ctypes 绑定
make run_python_ctypes

# 测试 C 扩展
make run_python_ext

# 测试 Cython 扩展
make run_python_cython

# 运行所有 Python 测试
make run_python
```

## 配置选项

各个绑定支持的配置选项有所不同：

### ctypes 绑定配置
```python
# ctypes 仅支持基本配置
config = TglogConfig(
    network=TGLOG_NETWORK_TCP,  # 或 TGLOG_NETWORK_UDP
    host="127.0.0.1",
    port=20001
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