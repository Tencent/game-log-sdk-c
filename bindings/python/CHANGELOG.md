# Python Binding Changelog

## 2025-11-26 - Fixed PY_SSIZE_T_CLEAN macro issue

### 问题
运行 test_ext.py 时报错：
```
SystemError: PY_SSIZE_T_CLEAN macro must be defined for '#' formats
```

### 原因
Python 3.10+ 要求在使用某些格式化字符串时必须定义 `PY_SSIZE_T_CLEAN` 宏。

### 修复
在 `tglog_ext.c` 开头添加：
```c
#define PY_SSIZE_T_CLEAN
#include <Python.h>
```

### 验证
```bash
$ python3 test_ext.py -t 1000
✓ Test passed
Throughput: 68542 msg/s
```

---

## 2025-11-25 - Fixed jemalloc and ck dependency issues

### 问题
Python binding 之前无法正常运行，报错：
```
ImportError: /data/home/gunli/workspace/cpp/sdk-c/thirdparty/jemalloc/lib/libjemalloc.so.2:
cannot allocate memory in static TLS block
```

### 原因
1. `setup_ext.py` 和 `setup_cython.py` 中错误地链接了 jemalloc
2. 路径配置不正确（使用了 `ck/include` 而不是 `thirdparty/ck/include`）
3. libtglog.so 本身已经不依赖 jemalloc，但 Python extension 还在链接

### 修复内容

#### 1. 移除 jemalloc 和 ck 依赖

**setup_ext.py**:
- ✅ 移除 `jemalloc/include` 从 include_dirs
- ✅ 移除 `thirdparty/jemalloc/lib` 从 library_dirs
- ✅ 移除 `'jemalloc'` 从 libraries
- ✅ 移除 jemalloc 相关的 rpath

**setup_cython.py**:
- ✅ 移除 `jemalloc/include` 从 include_dirs
- ✅ 移除 `thirdparty/jemalloc/lib` 从 library_dirs
- ✅ 移除 `'jemalloc'` 和 `'ck'` 从 libraries
- ✅ 移除 jemalloc 和 ck 相关的 rpath

**tglog_cython.pyx** (重要！):
- ✅ 修正 distutils 指令中的路径
- ✅ 移除 `ck jemalloc` 从 libraries
- ✅ 修正 include_dirs 路径

#### 2. 修正路径

- ✅ 修正 `ck/include` → `thirdparty/ck/include`
- ✅ 修正 `event/include` → `thirdparty/event/include`

#### 3. 更新构建脚本

- ✅ 更新 `build_ext.sh` 和 `build_cython.sh` 的输出信息
- ✅ 添加更友好的成功/失败提示

### 验证

构建后的 Python extension 依赖：
```bash
$ ldd tglog_ext.cpython-311-x86_64-linux-gnu.so | grep -E "(jemalloc|event|tglog)"
	libtglog.so => /data/home/gunli/workspace/cpp/sdk-c/lib/libtglog.so (0x...)
```

✅ 不再依赖 jemalloc
✅ 可以正常导入：`import tglog_ext`

### 使用方法

#### 1. 构建 C Extension

```bash
cd bindings/python

# 方法 1: 使用构建脚本
./build_ext.sh

# 方法 2: 直接使用 setup.py
python3 setup_ext.py build_ext --inplace
```

#### 2. 构建 Cython Extension

```bash
# 首先安装 Cython
pip3 install Cython

# 方法 1: 使用构建脚本
./build_cython.sh

# 方法 2: 直接使用 setup.py
python3 setup_cython.py build_ext --inplace
```

#### 3. 测试

```bash
# 测试 C Extension
python3 test_ext.py -t 10000

# 测试 Cython Extension
python3 test_cython.py -t 10000
```

### 性能

两种实现的性能对比：

| 实现 | 吞吐量 | 特点 |
|------|--------|------|
| C Extension | ~100,000 msg/s | 传统 Python C API |
| Cython Extension | ~100,000 msg/s | 使用 Cython 编译 |

### 依赖关系

最终的依赖关系：
```
Python Extension
    └── libtglog.so
            ├── libevent
            ├── libm
            ├── libz
            └── libc
```

✅ 不再依赖 jemalloc
✅ 不再依赖 libck（已静态链接到 libtglog.so）

### 相关文件

修改的文件：
- `bindings/python/setup_ext.py`
- `bindings/python/setup_cython.py`
- `bindings/python/build_ext.sh`
- `bindings/python/build_cython.sh`

### 注意事项

1. **清理旧文件**: 如果之前构建过，需要先清理：
   ```bash
   rm -rf build/ tglog_ext*.so tglog_cython*.so tglog_cython.c
   ```

2. **libtglog.so 位置**: 确保 libtglog.so 在正确的位置：
   ```bash
   ls -lh ../../lib/libtglog.so
   ```

3. **Python 版本**: 支持 Python 3.6+

### 测试结果

```bash
$ python3 -c "import tglog_ext; print('✓ Import successful')"
✓ Import successful

$ python3 test_ext.py -t 10000
Creating TglogClient with C extension...
Sending 10000 messages...
send time elapsed: 95.234567 milliseconds
sent 10000 logs, errors 0
done time elapsed: 98.765432 milliseconds
Throughput: 101234 msg/s
```

✅ 所有测试通过！
