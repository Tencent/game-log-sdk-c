# CMake 目录说明

## 你的项目中的 cmake/ 目录

### 位置
```
cmake/
└── tglog.pc.in
```

### tglog.pc.in 文件

这是一个 **pkg-config 模板文件**，用于生成 `.pc` 文件（pkg-config 配置文件）。

#### 什么是 pkg-config？

pkg-config 是一个帮助编译和链接库的工具。它提供：
- 库的头文件路径（`-I` 标志）
- 库的链接路径和库名（`-L` 和 `-l` 标志）
- 库的版本信息
- 依赖关系

#### tglog.pc.in 的内容

```
prefix=@CMAKE_CURRENT_SOURCE_DIR@/dist
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
includedir=${prefix}/include

Name: TGLog
Description: High-performance logging library
Version: 1.0.0
Libs: -L${libdir} -ltglog -L${prefix}/thirdparty/event/lib -levent ...
Cflags: -I${includedir} -I${prefix}/thirdparty/event/include ...
```

**关键点**：
- `@CMAKE_CURRENT_SOURCE_DIR@` - CMake 变量，会被替换为实际路径
- `prefix` - 安装前缀
- `Libs` - 链接时需要的库和路径
- `Cflags` - 编译时需要的头文件路径

#### 如何使用

**生成 .pc 文件**：

目前你的 CMakeLists.txt 中**没有使用**这个模板文件，而是直接在 Makefile 中生成 .pc 文件（Makefile 第 407-438 行）。

如果要在 CMake 中使用，需要添加：

```cmake
# 配置 pkg-config 文件
configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/tglog.pc.in
    ${CMAKE_BINARY_DIR}/tglog.pc
    @ONLY
)

# 安装 pkg-config 文件
install(FILES ${CMAKE_BINARY_DIR}/tglog.pc
        DESTINATION lib/pkgconfig)
```

**使用生成的 .pc 文件**：

```bash
# 设置 PKG_CONFIG_PATH
export PKG_CONFIG_PATH=/path/to/dist/lib/pkgconfig:$PKG_CONFIG_PATH

# 编译时使用
gcc myapp.c $(pkg-config --cflags --libs tglog) -o myapp

# 查看配置
pkg-config --cflags tglog    # 显示编译标志
pkg-config --libs tglog      # 显示链接标志
pkg-config --modversion tglog # 显示版本
```

---

## 常见的 CMake 相关目录

### 1. `cmake/` 目录（项目特定）

**你的项目有这个目录**，包含：
- `tglog.pc.in` - pkg-config 模板文件

**典型用途**：
- CMake 模块文件 (`.cmake`)
- Find 模块 (`FindXXX.cmake`)
- Config 模板 (`XXXConfig.cmake.in`)
- pkg-config 模板 (`*.pc.in`)

**示例结构**：
```
cmake/
├── tglog.pc.in              # pkg-config 模板（你的项目有）
├── FindJemalloc.cmake       # 查找 jemalloc（可选）
├── CompilerWarnings.cmake   # 编译器警告（可选）
└── TglogConfig.cmake.in     # CMake config 模板（可选）
```

---

### 2. `build/` 目录（构建目录）

**用途**：CMake 的默认构建目录（out-of-source build）

**特点**：
- 包含所有构建生成的文件
- 不应该提交到 git（已在 `.gitignore` 中）
- 可以随时删除并重新生成

**使用方式**：
```bash
# 创建并进入构建目录
mkdir build && cd build

# 配置
cmake ..

# 编译
make

# 清理：直接删除整个目录
cd .. && rm -rf build
```

**你的 .gitignore**：
```gitignore
build          # 忽略 build 目录
```

---

### 3. `cmake-build-*` 目录（IDE 构建目录）

**用途**：IDE（如 CLion）自动创建的构建目录

**常见命名**：
- `cmake-build-debug` - Debug 构建
- `cmake-build-release` - Release 构建
- `cmake-build-relwithdebinfo` - 带调试信息的 Release
- `cmake-build-minsizerel` - 最小体积 Release

**特点**：
- IDE 自动管理
- 不应该提交到 git
- 每个构建类型一个目录

**你的 .gitignore**：
```gitignore
cmake-build-*  # 忽略所有 cmake-build- 开头的目录
```

---

### 4. CMake 生成的文件

这些文件由 CMake 在配置和构建过程中生成：

#### `CMakeCache.txt`
- **用途**：缓存 CMake 配置变量
- **位置**：构建目录根目录
- **内容**：所有 CMake 变量的值
- **删除**：强制重新配置时需要删除

```bash
# 重新配置
rm CMakeCache.txt
cmake ..
```

#### `CMakeFiles/` 目录
- **用途**：CMake 内部使用的临时文件
- **内容**：
  - 编译器测试结果
  - 依赖关系文件
  - 中间构建文件
- **不应该手动修改**

#### `cmake_install.cmake`
- **用途**：安装规则脚本
- **使用**：`make install` 时执行

#### `compile_commands.json`
- **用途**：编译命令数据库
- **使用者**：
  - IDE（如 VSCode、CLion）
  - 静态分析工具（clang-tidy、cppcheck）
  - Language Server（clangd）
- **生成**：
  ```cmake
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
  ```

**你的项目已启用**（CMakeLists.txt 第 7 行）：
```cmake
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
```

---

## 你的项目配置

### 当前 .gitignore 配置

```gitignore
# CMake build directories
build                      # 通用构建目录
cmake-build-*             # IDE 构建目录（所有变体）
CMakeCache.txt            # CMake 缓存
CMakeFiles/               # CMake 内部文件
cmake_install.cmake       # 安装脚本
Makefile.cmake            # CMake 生成的 Makefile（如果有）
compile_commands.json     # 编译命令数据库
```

**注意**：`cmake/` 目录**不应该**在 `.gitignore` 中，因为它包含源代码（模板文件）。

### 推荐的构建方式

#### 方式 1：使用 build 目录（推荐）

```bash
# 创建构建目录
mkdir build && cd build

# Debug 构建
cmake ..
make

# Release 构建
cmake -DRELEASE=1 ..
make

# 清理
cd .. && rm -rf build
```

#### 方式 2：使用 IDE（CLion）

IDE 会自动创建 `cmake-build-debug` 等目录，你不需要手动管理。

#### 方式 3：多个构建目录

```bash
# Debug 构建
mkdir build-debug && cd build-debug
cmake ..
make
cd ..

# Release 构建
mkdir build-release && cd build-release
cmake -DRELEASE=1 ..
make
cd ..
```

---

## pkg-config 的改进建议

### 当前状态

- ✅ 有 `cmake/tglog.pc.in` 模板文件
- ❌ CMakeLists.txt 中没有使用它
- ✅ Makefile 中直接生成 .pc 文件（第 407-438 行）

### 改进方案

如果你想在 CMake 中也使用这个模板，可以在 CMakeLists.txt 中添加：

```cmake
# 在安装部分之前添加（约第 330 行附近）

# 配置 pkg-config 文件
configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/tglog.pc.in
    ${CMAKE_BINARY_DIR}/tglog.pc
    @ONLY
)

# 安装 pkg-config 文件
install(FILES ${CMAKE_BINARY_DIR}/tglog.pc
        DESTINATION dist/lib/pkgconfig)
```

这样就可以：
1. 保持 Makefile 和 CMake 的一致性
2. 只维护一个模板文件
3. 避免重复代码

---

## 常见操作

### 完全清理 CMake 构建

```bash
# 删除所有构建目录
rm -rf build cmake-build-* CMakeCache.txt CMakeFiles/

# 重新配置
mkdir build && cd build
cmake ..
make
```

### 查看 CMake 缓存变量

```bash
cd build
cmake -L ..              # 列出所有缓存变量
cmake -LA ..             # 列出所有变量（包括高级）
cmake -LH ..             # 列出变量和帮助信息
```

### 使用 pkg-config

```bash
# 查看编译标志
pkg-config --cflags tglog

# 查看链接标志
pkg-config --libs tglog

# 编译应用
gcc myapp.c $(pkg-config --cflags --libs tglog) -o myapp
```

---

## 总结

### 你的项目中

- ✅ **有 `cmake/` 目录** - 包含 `tglog.pc.in` 模板文件
- ✅ **`.gitignore` 配置正确** - 忽略构建产物，但不忽略 `cmake/` 源代码目录
- ✅ **启用了 `compile_commands.json`** - 方便 IDE 和工具使用
- ⚠️ **CMakeLists.txt 未使用模板** - 可以改进以使用 `tglog.pc.in`

### 建议

1. **保留 `cmake/` 目录** - 它包含重要的模板文件
2. **考虑在 CMake 中使用模板** - 避免重复维护 .pc 文件内容
3. **使用 out-of-source build** - 始终在 `build/` 目录中构建
4. **定期清理** - 遇到问题时删除构建目录重新配置

### 参考

- [pkg-config 官方文档](https://www.freedesktop.org/wiki/Software/pkg-config/)
- [CMake configure_file](https://cmake.org/cmake/help/latest/command/configure_file.html)
- [CMake 最佳实践](https://cliutils.gitlab.io/modern-cmake/)
