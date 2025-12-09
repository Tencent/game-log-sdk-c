# 依赖下载说明

## 问题背景

TGLog SDK 依赖三个外部库：
- jemalloc 5.3.0
- libevent 2.1.12-stable
- ConcurrencyKit 0.7.2

这些依赖通常通过 git submodule 管理。但是，如果用户通过 GitHub/GitLab 的"Download ZIP"功能下载源代码，会缺少 `.git` 目录，导致 `git submodule` 命令失败。

## 解决方案

我们提供了 `download-deps.sh` 脚本，可以在没有 git 的情况下下载这些依赖。

### 使用方法

#### 方式一：使用 git clone（推荐）

```bash
git clone --recurse-submodules https://git.woa.com/tglog/v3/sdk-c.git
cd sdk-c
make
```

#### 方式二：下载 ZIP 压缩包

1. 从 GitHub/GitLab 下载 ZIP 文件并解压
2. 运行依赖下载脚本：

```bash
cd sdk-c
./download-deps.sh
```

3. 编译项目：

```bash
make deps      # 编译依赖库
make           # 编译 TGLog SDK
```

### 脚本功能

`download-deps.sh` 脚本会：

1. **自动检测环境**：
   - 如果检测到 `.git` 目录，自动使用 `git submodule`
   - 如果没有 `.git` 目录，从 GitHub 下载依赖

2. **下载三个依赖库**：
   - jemalloc 5.3.0 (从 GitHub releases)
   - libevent 2.1.12-stable (从 GitHub releases)
   - ConcurrencyKit 0.7.2 (从 GitHub tags)

3. **自动解压**：
   - 支持 `.tar.gz` 和 `.tar.bz2` 格式
   - 自动检测压缩格式

4. **跳过已存在的依赖**：
   - 如果目录已存在且不为空，跳过下载

### 依赖工具

脚本需要以下工具之一：
- `wget` (推荐)
- `curl`

大多数 Linux/macOS 系统都预装了这些工具。

### 示例输出

```
============================================
TGLog SDK - Download Dependencies
============================================

This script downloads required dependencies:
  - jemalloc 5.3.0
  - libevent 2.1.12-stable
  - ConcurrencyKit 0.7.2

Not a git repository. Downloading dependencies manually...

Downloading jemalloc 5.3.0...
✓ jemalloc downloaded and extracted

Downloading libevent 2.1.12-stable...
✓ libevent downloaded and extracted

Downloading ConcurrencyKit 0.7.2...
✓ ConcurrencyKit downloaded and extracted

============================================
✓ All dependencies downloaded successfully!
============================================

Next steps:
  1. Run: make deps      # Build dependencies
  2. Run: make           # Build TGLog SDK
```

## 构建系统集成

### Makefile 集成

Makefile 会自动检测依赖是否存在：

### 场景一：使用 git clone

```bash
$ git clone --recurse-submodules https://git.woa.com/tglog/v3/sdk-c.git
$ cd sdk-c
$ make
Checking dependencies...
  ✓ Dependencies found
  Checking out specific versions...
✓ Submodules checked out to specified versions
```

### 场景二：下载 ZIP，自动下载依赖

```bash
$ make
Checking dependencies...
  Dependencies not found, initializing...
  Running download-deps.sh to download dependencies...
============================================
TGLog SDK - Download Dependencies
============================================
...
✓ All dependencies downloaded successfully!
  ✓ Dependencies found
  ✓ Using downloaded dependencies (version check skipped)
```

**注意**：Makefile 会自动调用 `download-deps.sh` 下载依赖，无需手动运行！

### 场景三：下载 ZIP，手动下载依赖

```bash
$ ./download-deps.sh
✓ All dependencies downloaded successfully!

$ make
Checking dependencies...
  ✓ Dependencies found
  ✓ Using downloaded dependencies (version check skipped)
```

Makefile 会智能识别依赖来源（git submodule 或手动下载），并跳过不必要的版本检查。

### CMake 集成

CMake 也支持相同的智能检测：

#### 场景一：使用 git clone

```bash
$ git clone --recurse-submodules https://git.woa.com/tglog/v3/sdk-c.git
$ cd sdk-c
$ mkdir build && cd build
$ cmake ..
-- Checking dependencies...
-- Dependencies found
-- Checking out submodules to specific versions...
-- Submodules checked out to specified versions
```

#### 场景二：下载 ZIP，自动下载依赖

```bash
$ mkdir build && cd build
$ cmake ..
-- Checking dependencies...
-- Dependencies not found, initializing...
-- Running download-deps.sh to download dependencies...
============================================
TGLog SDK - Download Dependencies
============================================
...
✓ All dependencies downloaded successfully!
-- Dependencies found
-- Using downloaded dependencies (version check skipped)
```

**注意**：CMake 会自动调用 `download-deps.sh` 下载依赖，无需手动运行！

#### 场景三：下载 ZIP，手动下载依赖

```bash
$ ./download-deps.sh
✓ All dependencies downloaded successfully!

$ mkdir build && cd build
$ cmake ..
-- Checking dependencies...
-- Dependencies found
-- Using downloaded dependencies (version check skipped)
```

CMake 使用 `file(GLOB)` 检查目录是否不为空，同样智能识别依赖来源。

## 技术细节

### 下载源

所有依赖都从官方 GitHub 仓库下载：

- jemalloc: https://github.com/jemalloc/jemalloc/releases/download/5.3.0/jemalloc-5.3.0.tar.bz2
- libevent: https://github.com/libevent/libevent/releases/download/release-2.1.12-stable/libevent-2.1.12-stable.tar.gz
- ConcurrencyKit: https://github.com/concurrencykit/ck/archive/refs/tags/0.7.2.tar.gz

### 目录结构

下载后的目录结构：

```
sdk-c/
├── jemalloc/          # jemalloc 5.3.0 源代码
├── event/             # libevent 2.1.12-stable 源代码
├── ck/                # ConcurrencyKit 0.7.2 源代码
└── download-deps.sh   # 依赖下载脚本
```

### 与 git submodule 的对比

| 特性 | git submodule | download-deps.sh |
|------|---------------|------------------|
| 需要 git | 是 | 否 |
| 需要 .git 目录 | 是 | 否 |
| 版本控制 | 是 | 否 |
| 下载速度 | 较慢 | 较快 |
| 适用场景 | 开发者 | 最终用户 |

## 常见问题

### Q: 为什么不把依赖直接包含在源代码中？

A: 这些依赖库体积较大（总共约 250MB），直接包含会显著增加仓库大小。使用 submodule 或按需下载是更好的选择。

### Q: 下载失败怎么办？

A: 请检查：
1. 网络连接是否正常
2. 是否安装了 wget 或 curl
3. GitHub 是否可访问

如果 GitHub 访问受限，可以手动下载依赖并解压到对应目录。

### Q: 可以使用其他版本的依赖吗？

A: 理论上可以，但未经测试。建议使用脚本指定的版本以确保兼容性。

## 维护说明

如果需要更新依赖版本，请修改 `download-deps.sh` 中的以下部分：

```bash
# Download jemalloc
download_and_extract \
    "jemalloc" \
    "https://github.com/jemalloc/jemalloc/releases/download/5.3.0/jemalloc-5.3.0.tar.bz2" \
    "jemalloc" \
    "5.3.0"
```

更新版本号和下载 URL 即可。
