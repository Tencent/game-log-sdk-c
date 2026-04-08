# TGLog SDK CMake 构建指南

本文档详细说明如何使用 CMake 构建 TGLog SDK。

## 快速开始

```bash
# 1. 创建构建目录
mkdir build
cd build

# 2. 配置项目
cmake ..

# 3. 一次性构建所有核心组件（推荐）
make -j4
```

这会自动构建：
- 依赖库（jemalloc、libevent、ConcurrencyKit）
- 核心库（tglog 和 tglog-st，包括静态库和动态库）
- 测试程序（多线程和单线程版本）
- TAPP 集成示例
- 语言绑定（Python）

**与 Makefile 完全一致**：执行 `make` 会构建所有组件，就像使用原 Makefile 一样。

## 详细说明

### 1. 配置阶段

#### 基本配置

```bash
# Debug 版本（默认）
cmake ..

# Release 版本
cmake -DRELEASE=1 ..

# 或使用 CMAKE_BUILD_TYPE
cmake -DCMAKE_BUILD_TYPE=Release ..
```

#### 配置选项

| 选项 | 说明 | 默认值 |
|------|------|--------|
| `RELEASE` | 构建 Release 版本 | OFF |
| `BUILD_SHARED` | 构建动态库 | ON |
| `CMAKE_BUILD_TYPE` | 构建类型（Debug/Release） | Debug |

#### 配置示例

```bash
# Release 版本 + 详细输出
cmake -DRELEASE=1 -DCMAKE_VERBOSE_MAKEFILE=ON ..

# 只构建静态库
cmake -DBUILD_SHARED=OFF ..

# 指定编译器
cmake -DCMAKE_C_COMPILER=gcc-12 ..
```

### 2. 构建阶段

#### 一次性构建（推荐）

**最简单的方式是直接执行 `make`**：

```bash
make -j4
```

这会自动按正确的顺序构建：
1. 依赖库（jemalloc、libevent、ConcurrencyKit）
2. 核心库（tglog 和 tglog-st）
3. 测试程序（多线程和单线程版本）
4. TAPP 集成示例
5. 语言绑定（Python）

依赖库会被安装到 `thirdparty/` 目录，后续构建会自动跳过。

**与 Makefile 行为一致**：这与直接在项目根目录执行 `make` 的效果完全相同。

#### 分步构建（可选）

如果需要更精细的控制，也可以分步构建：

```bash
# 只构建依赖库
make deps -j4

# 只构建核心库
make tglog tglog-st -j4

# 只构建测试程序
make test_programs test_st_programs -j4
```

#### 构建核心库

```bash
# 构建多线程版本
make tglog -j4

# 构建单线程版本
make tglog-st -j4

# 同时构建两个版本
make tglog tglog-st -j4
```

生成的库文件位于 `lib/` 目录：
- `lib/libtglog.a` - 多线程静态库
- `lib/libtglog.so` - 多线程动态库
- `lib/libtglog-st.a` - 单线程静态库
- `lib/libtglog-st.so` - 单线程动态库

#### 构建测试程序

```bash
# 多线程测试程序
make test_programs -j4

# 单线程测试程序
make test_st_programs -j4

# TAPP 集成示例
make test-tapp
```

#### 查看所有目标

```bash
# 查看所有可用的构建目标
make help

# 查看 TGLog 特定的帮助信息
make help_info
```

### 3. 安装阶段

```bash
# 安装到 dist/ 目录
make install
```

安装后的目录结构：
```
dist/
├── lib/                    # 库文件
│   ├── libtglog.a
│   ├── libtglog.so
│   ├── libtglog-st.a
│   └── libtglog-st.so
├── include/                # 头文件
│   ├── tglog/
│   └── tglog-st/
├── bin/                    # 测试程序
├── examples/               # 示例程序
│   └── test-tapp/
└── thirdparty/             # 依赖库
    ├── event/
    ├── ck/
    └── jemalloc/
```

### 4. 清理操作

```bash
# 清理构建产物（保留依赖库）
make clean

# 清理依赖库
make clean-deps

# 完全清理（删除整个 build 目录）
cd ..
rm -rf build
```

## 常见构建目标

| 目标 | 说明 |
|------|------|
| `deps` | 构建依赖库 (jemalloc, libevent, ck) |
| `tglog` | 构建多线程版本库 |
| `tglog-st` | 构建单线程版本库 |
| `test_programs` | 构建多线程测试程序 |
| `test_st_programs` | 构建单线程测试程序 |
| `test-tapp` | 构建 TAPP 集成示例 |
| `check-deps` | 检查依赖状态 |
| `install` | 安装到 dist/ 目录 |
| `uninstall` | 删除 dist/ 目录 |
| `clean` | 清理构建产物 |
| `clean-deps` | 清理依赖库 |

## 完整构建流程示例

### 示例 1: Debug 版本开发

```bash
# 创建 Debug 构建目录
mkdir build-debug
cd build-debug

# 配置为 Debug 版本
cmake ..

# 一次性构建所有核心组件
make -j4

# 运行测试
cd ../test
./tglog_test

cd ../test-st
./tglog_test_st
```

### 示例 2: Release 版本发布

```bash
# 创建 Release 构建目录
mkdir build-release
cd build-release

# 配置为 Release 版本
cmake -DRELEASE=1 ..

# 一次性构建所有核心组件
make -j$(nproc)

# 构建语言绑定（可选）
make bindings -j$(nproc)

# 安装
make install

# 检查安装结果
ls -la ../dist/
```

### 示例 3: 只构建静态库

```bash
mkdir build-static
cd build-static

# 配置为只构建静态库
cmake -DBUILD_SHARED=OFF -DRELEASE=1 ..

# 一次性构建
make -j4
```

## 语言绑定

语言绑定会在执行 `make` 时自动构建，与 Makefile 行为一致。

### 自动构建（默认）

```bash
# 在 build 目录中
cd build

# 执行 make 会自动构建所有组件，包括语言绑定
make -j4
```

### 单独构建语言绑定

如果只想构建语言绑定：

```bash
# 构建 Python 绑定
make bindings -j4

# 或者
make python_bindings    # 只构建 Python
```

### 验证构建结果

```bash
# 检查 Python 绑定
ls -lh ../bindings/python/*.so
```

## 故障排除

### 问题 1: 配置失败 - 路径不存在

**错误信息**:
```
Imported target "event" includes non-existent path
```

**解决方案**:
这个问题已经修复。如果仍然遇到，请确保使用最新的 CMakeLists.txt。

### 问题 2: 构建失败 - 找不到依赖库

**错误信息**:
```
/usr/bin/ld: 找不到 -levent
```

**解决方案**:
首次构建必须先构建依赖库：
```bash
make deps -j4
```

### 问题 3: 并行构建失败

**解决方案**:
减少并行度或使用串行构建：
```bash
# 减少并行度
make -j2

# 串行构建
make
```

### 问题 4: 依赖库构建超时

**解决方案**:
依赖库构建可能需要较长时间（5-10分钟），请耐心等待。如果确实超时，可以手动构建：

```bash
# 手动构建 libevent
cd event
./autogen.sh
./configure --prefix=../thirdparty/event --disable-openssl
make && make install

# 手动构建 ck
cd ../ck
./configure --prefix=../thirdparty/ck
make && make install

# 手动构建 jemalloc
cd ../jemalloc
./autogen.sh
CFLAGS="-fPIC" CXXFLAGS="-fPIC" ./configure --prefix=../thirdparty/jemalloc --with-pic --enable-static
make && make install
```

## CMake 与 Makefile 对比

| 特性 | Makefile | CMake |
|------|----------|-------|
| 配置方式 | 直接编译 | 需要先配置 |
| 构建目录 | 源码目录 | 独立构建目录 |
| IDE 支持 | 有限 | 完整（生成 compile_commands.json） |
| 跨平台 | Linux | Linux/Windows |
| Windows 支持 | 不支持 | 完整支持（MSVC） |
| 语言绑定 | 完整支持 | 需要使用 Makefile |
| 推荐场景 | 快速构建、生产环境 | IDE 开发、调试、Windows 构建 |

## Windows 构建 (MSVC)

Windows 平台使用 CMake + MSVC 构建，是 Windows 下的推荐方式。

### 前置条件

- **Visual Studio 2019+** 或 Visual Studio Build Tools（需包含"使用 C++ 的桌面开发"工作负载）
- **CMake 3.10+**（Visual Studio 自带或单独安装）
- **Git**（用于初始化子模块）

### 命令行构建

在 **Developer Command Prompt** 或 **Developer PowerShell** 中执行：

```bash
# 克隆代码库
git clone --recurse-submodules https://git.woa.com/tglog/v3/sdk-c.git
cd sdk-c

# 生成 Visual Studio 工程
mkdir out
cd out
cmake ..

# 构建（Debug 版本）
cmake --build . --config Debug

# 构建（Release 版本）
cmake --build . --config Release

# 安装到 dist 目录
cmake --install . --config Release
```

### Visual Studio IDE

**方式一：打开生成的解决方案**
```bash
cd out
cmake ..
start sdk_c.sln
```

**方式二：直接打开文件夹**
使用 Visual Studio 的"打开文件夹"功能打开项目根目录，VS 会自动识别 `CMakeLists.txt` 并配置项目。

### Windows 平台差异

| 特性 | Linux | Windows |
|------|-------------|---------|
| 构建工具 | Makefile 或 CMake | CMake + MSVC |
| 多线程版本 (tglog) | 支持 | 支持 |
| 单线程版本 (tglog-st) | 支持 | 支持 |
| jemalloc | 支持 | 不支持（使用系统分配器） |
| ConcurrencyKit | 支持 | 不支持 |
| libevent | autotools 构建 | CMake add_subdirectory 构建 |
| 系统库 | pthread, m | ws2_32 |
| 库文件格式 | .a / .so | .lib / .dll |

### 链接示例

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

### Windows 构建常见问题

#### 找不到 libevent 头文件
确保 CMake 配置步骤成功完成，libevent 会通过 `add_subdirectory` 自动构建。

#### 链接错误：无法解析的外部符号
检查是否正确链接了 `ws2_32.lib`，这是 Windows Sockets API 所需的系统库。

#### DLL 加载失败
确保 `tglog-st.dll` 和 libevent 的 DLL 在可执行程序的搜索路径中（同目录或 PATH 中）。

## IDE 集成

### Visual Studio (Windows)

Visual Studio 原生支持 CMake 项目：

1. 打开 Visual Studio，选择"打开本地文件夹"
2. 选择项目根目录（包含 `CMakeLists.txt`）
3. VS 会自动配置 CMake 并生成项目
4. 在解决方案资源管理器中切换 CMake 目标视图
5. 选择 Debug/Release 配置并构建

### CLion

CLion 原生支持 CMake 项目：

1. 打开项目目录
2. CLion 会自动识别 CMakeLists.txt
3. 配置构建类型（Debug/Release）
4. 点击构建按钮

### Visual Studio Code

1. 安装 CMake Tools 扩展
2. 打开项目目录
3. 按 `Ctrl+Shift+P`，选择 "CMake: Configure"
4. 选择构建目标并构建

### Vim/Neovim

使用 `compile_commands.json` 配置 LSP：

```bash
# 构建时生成 compile_commands.json
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..

# 链接到项目根目录
ln -s build/compile_commands.json ../
```

## 高级用法

### 自定义安装路径

```bash
cmake -DCMAKE_INSTALL_PREFIX=/usr/local ..
make install
```

### 使用 Ninja 构建系统

```bash
# 配置使用 Ninja
cmake -G Ninja ..

# 构建
ninja deps
ninja tglog tglog-st
```

## 总结

- **Linux**：推荐使用 Makefile 进行日常构建和生产环境部署，CMake 适合 IDE 开发和调试
- **Windows**：使用 CMake + MSVC 构建，支持多线程和单线程两个版本
- 首次构建必须先执行 `make deps`（Linux）或确保 CMake 配置成功（Windows）
- 语言绑定目前需要使用原 Makefile 构建（仅 Linux）
- 使用独立的构建目录（out-of-source build）保持源码目录干净
