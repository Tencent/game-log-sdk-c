# MSVC 构建改造方案 (Rev 2)

## 目标

让项目支持使用原生 **VC++ / MSVC** 在 Windows 上构建，并满足以下约束：
- libevent 使用仓库内的 **CMake 原生构建**
- Windows 下 **不接入 jemalloc**，改用系统分配器
- 先验证 **静态库**，再打通 **动态库**
- **Python bindings** 同步支持 Windows/MSVC
- 尽量不影响现有 Linux/macOS 构建路径

---

## 总体策略

以 **CMake 作为 Windows 的一等构建入口**，通过 `if(WIN32)` / `if(MSVC)` 增加平台分支：
- Windows 下不再走 autotools / shell / make 流程
- Windows 下直接 `add_subdirectory(event)` 构建 libevent
- Windows 下跳过 jemalloc 依赖和相关链接
- Windows 下先关闭测试子目录中的 Unix-only 构建逻辑
- Python bindings 在 Windows 下改为直接调用 `setuptools`，不再依赖 `make`

---

## 实施步骤

### 1. 改造根构建入口

**文件：** `CMakeLists.txt`

#### 需要修改
1. **全局编译参数拆分**：
   - MSVC：使用 `/W3`、`/O2`、`/Od`、`/Zi`、`/D_CRT_SECURE_NO_WARNINGS`
   - 非 MSVC：保留现有 `-std=c99 -D_GNU_SOURCE -D_REENTRANT ...`
2. **依赖构建分流**：
   - **Windows**：`add_subdirectory(event)`，禁用 libevent tests/benchmarks/samples/openssl
   - **非 Windows**：保留现有 `ExternalProject_Add` + autotools 逻辑
3. **jemalloc 占位**：
   - Windows 下不构建 jemalloc，只保留一个空依赖目标，避免下游 `add_dependencies(... jemalloc_build ...)` 报错
4. **清理 Unix 强依赖命令**：
   - `clean-deps`：Windows 下使用 CMake 原生命令清理，非 Windows 下将 `make clean` 移入 `POST_BUILD` 或保留 `cd ... && make clean || true`
   - `check-deps`：移除 `test -f` 和 `.a/.so` 假设，Windows 下改用 `if(EXISTS ...)` 检查 `.lib/.dll`，或直接跳过
   - `download-deps.sh`：Windows 下不可用，通过 `if(WIN32 AND NOT EXISTS ...)` 提示用户运行 `git submodule update`
5. **pkg-config 隔离**：
   - 使用 `if(NOT WIN32)` 包裹 `tglog.pc` 和 `tglog-st.pc` 的生成与安装逻辑
6. **DLL 导出验证（第一阶段）**：
   - 在 `if(WIN32)` 分支中临时开启 `set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)`，快速验证 DLL 构建链接

---

### 2. 改造 `tglog` 模块构建

**文件：** `tglog/CMakeLists.txt`

#### 需要修改
1. **编译参数拆分**：
   - 删除 MSVC 下不兼容的 `target_compile_options` 参数：`-std=c99`、`-fPIC`、`-m64`、`-pipe`、`-mcx16`
2. **libevent 链接与 Include 修正**：
   - 链接名：Windows 下使用 libevent 实际导出的目标名称（如 `event_core_static`、`event_static` 等），不要硬编码 `event`
   - 路径：如果链接了 CMake target，include 路径会自动传递；否则需补充 `${CMAKE_BINARY_DIR}/event/include`（生成的 `event-config.h` 所在目录）
3. **多线程依赖修正**：
   - Windows 下不需要链接 `event_pthreads`（代码已使用 `evthread_use_windows_threads()`），只需链接 `ws2_32` 及 libevent 的核心库（加上可能的 `iphlpapi`）
4. **安装规则与输出路径**：
   - 输出目录同时兼容 `.dll` + import `.lib`
   - 安装规则摒弃硬编码的 `libtglog.a` / `libtglog.so`，改用标准的：
     ```cmake
     install(TARGETS tglog_static tglog_shared
         ARCHIVE DESTINATION dist/lib
         LIBRARY DESTINATION dist/lib
         RUNTIME DESTINATION dist/bin
     )
     ```

#### 额外建议
- 第二阶段（精确控制）：引入 `TGLOG_API`（基于 `__declspec(dllexport/dllimport)`）宏修饰公开 API，取代 `CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS`。

---

### 3. 改造 `tglog-st` 模块构建

**文件：** `tglog-st/CMakeLists.txt`

#### 需要修改
与 `tglog/CMakeLists.txt` 同步处理，且特别注意：
1. **清理全局污染**：
   - `tglog-st/CMakeLists.txt:8` 目前直接追加 `CMAKE_C_FLAGS`，这在 MSVC 下会报错。
   - 需要改为 MSVC 条件分支，或统一采用 `target_compile_options`（如 `if(MSVC) target_compile_options(...) else() ... endif()`）
2. **链接调整**：
   - 同样在 Windows 下链接 `ws2_32` 及匹配的 libevent 静态库目标
3. **安装规则补充**：
   - `install(TARGETS ...)` 中补充 `RUNTIME DESTINATION dist/bin`

---

### 4. 暂时禁用 Windows 下测试构建

**文件：**
- `test/CMakeLists.txt`
- `test-st/CMakeLists.txt`
- `test-tapp/CMakeLists.txt`

#### 建议做法
第一阶段在 Windows 下先直接跳过：
```cmake
if(WIN32)
    return()
endif()
```
或等价方式禁用 target 创建。

#### 原因
1. 当前测试 CMake 里仍有 `-Wl,-dn`、`-Wl,-dy`、`pthread` 以及 jemalloc 假设
2. `test-tapp` 还有额外 Unix/外部环境依赖
3. 当前目标应优先让 SDK 本体和 Python bindings 跑通

---

### 5. 改造 bindings 的 CMake 入口

**文件：** `bindings/CMakeLists.txt`

#### 需要修改
1. Windows 下不要再调用 `${CMAKE_MAKE_PROGRAM} -C ...`
2. 改为直接调用 Python：
   - `python setup_ext.py build_ext --inplace`
3. 让 `python_bindings` 依赖 `tglog_shared`（必要时也依赖 `tglog_static`）
4. 非 Windows 保留现有逻辑

---

### 6. 改造 Python 扩展构建脚本

**文件：**
- `bindings/python/setup_ext.py`
- `bindings/python/setup_cython.py`

#### 需要修改
1. **链接库匹配**：
   - Windows 下不再写死 `event`，改为匹配实际生成的 libevent 产物（如 `event_core_static.lib` 或直接链接 `tglog.dll` 导出的符号，推荐后者或静态链接 libevent 以简化 DLL 依赖）
2. **移除 Unix 假设**：
   - Windows 下不再假设 jemalloc 存在
3. **编译参数**：
   - 保持 MSVC 编译参数：`/O2`、`/DNDEBUG`
   - 非 Windows 继续保留现有 `-O3 -march=native -ffast-math -std=c99`

---

### 7. 修复 Python 运行时 DLL 加载

**文件：** `bindings/python/tglog_client.py`

#### 需要修改
1. **DLL 搜索路径**：
   - Windows 下在加载 `tglog.dll` 前，针对 Python 3.8+ 增加 `os.add_dll_directory()`
   - 确保 `tglog.dll` 和 libevent 的 DLL（若使用动态链接）都在搜索路径或同一目录下
   - 推荐方案：Windows 下将 libevent **静态链接**进 `tglog.dll`，Python 只需加载一个 DLL。

代码示例：
```python
if sys.platform == "win32":
    _lib_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "../lib"))
    if hasattr(os, "add_dll_directory"):
        os.add_dll_directory(_lib_dir)
    os.environ["PATH"] = _lib_dir + os.pathsep + os.environ.get("PATH", "")
```

---

## 推荐落地顺序

根据 Review 意见，调整为"先跑通静态库，再验证 DLL"的策略：

1. **根 CMakeLists.txt**
   - 打通 libevent `add_subdirectory` + jemalloc 空目标 + 编译参数分支 + 屏蔽 pkg-config/download-deps.sh 的 Unix 特性
2. **Windows 下禁用 `test*`**
   - `if(WIN32) return() endif()`
3. **验证 tglog 静态库**
   - 修改 `tglog/CMakeLists.txt`，链接 `event_core_static` + `ws2_32`，不处理 DLL
4. **验证 tglog-st 静态库**
   - 修改 `tglog-st/CMakeLists.txt`，重点修复 `CMAKE_C_FLAGS` 污染问题
5. **验证动态库 (DLL) 构建**
   - 在根 CMake 加 `CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON`
   - 补全 tglog / tglog-st 的 `RUNTIME DESTINATION` 安装规则
6. **改 bindings/CMakeLists.txt**
   - 切换为调用 Python setuptools
7. **改 Python setup + DLL 加载**
   - 完善 `setup_ext.py` 和 `tglog_client.py` 中的路径与库名

---

## 分阶段验收

### 验收 1：核心库
Windows + MSVC 下成功生成：
- `tglog` 静态库 / 动态库
- `tglog-st` 静态库 / 动态库

### 验收 2：Python bindings
Windows + MSVC 下成功生成 Python 扩展，并能通过 `ctypes` 或扩展模块加载 SDK DLL。

### 验收 3：安装产物
`dist/` 或对应输出目录中有可用的：
- headers
- `.lib`
- `.dll`
- Python binding 文件
