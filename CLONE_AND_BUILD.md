# Clone 和构建指南

## 别人从远程仓库 Clone 你的项目

### 场景说明

当你把项目推送到远程仓库（如 GitHub）后，其他开发者 clone 时会发生什么？

---

## 方法 1：自动初始化 Submodules（推荐）

### Clone 时自动初始化

```bash
# 使用 --recurse-submodules 选项
git clone --recurse-submodules https://github.com/your-username/sdk-c.git
cd sdk-c

# 此时 ck、event、jemalloc 目录已经存在并在正确的版本上
ls -d ck event jemalloc
# 输出: ck  event  jemalloc

# 直接构建
make
# 或
cmake . && make
```

**优点**：
- 一步到位，自动下载所有 submodules
- 不需要额外操作

---

## 方法 2：Clone 后手动初始化

### 普通 Clone

```bash
# 普通 clone（不会下载 submodules）
git clone https://github.com/your-username/sdk-c.git
cd sdk-c

# 此时 submodule 目录是空的
ls -d ck event jemalloc 2>/dev/null
# 可能没有输出，或者目录存在但是空的
```

### 使用构建系统自动初始化（推荐）

```bash
# 方式 A：使用 Makefile（会自动初始化 submodules）
make

# 方式 B：使用 CMake（会自动初始化 submodules）
cmake .
make
```

**你的构建系统已经配置了自动初始化和错误检测**：
- `Makefile` 的 `deps` 目标依赖 `init-submodules`
- `CMakeLists.txt` 在配置阶段自动初始化
- **如果初始化失败会立即停止并显示错误信息**

**如果初始化失败**，你会看到清晰的错误提示：
```
✗ Failed to initialize submodules
  Please check your network connection and git configuration
  You can manually run: git submodule update --init --recursive
make: *** [Makefile:101: init-submodules] Error 1
```

### 手动初始化

```bash
# 如果不想使用构建系统，可以手动初始化
git submodule update --init --recursive

# 或者使用项目提供的目标
make init-submodules
```

---

## 方法 3：已经 Clone 但忘记初始化

### 症状

```bash
# 尝试构建时出错
make
# 错误: No such file or directory: jemalloc/...
# 或: event/... not found
```

### 解决方案

```bash
# 使用构建系统自动修复
make init-submodules

# 或手动初始化
git submodule update --init --recursive

# 然后重新构建
make clean && make
```

---

## 完整的工作流程示例

### 新开发者的完整步骤

```bash
# 1. Clone 项目（推荐使用 --recurse-submodules）
git clone --recurse-submodules https://github.com/your-username/sdk-c.git
cd sdk-c

# 2. 验证 submodules
git submodule status
# 输出应该类似：
#  981e14290df3d4339974eade5d8820dafd506b58 ck (0.7.2)
#  5df3037d10556bfcb675bc73e516978b75fc7bc7 event (release-2.1.12-stable)
#  54eaed1d8b56b1aa528be3bdd1877e59c56fa90c jemalloc (5.3.0)

# 3. 构建项目
make
# 或
cmake . && make

# 4. 运行测试
cd test-st
./simple_perf_test_st
```

### 如果使用普通 Clone

```bash
# 1. 普通 clone
git clone https://github.com/your-username/sdk-c.git
cd sdk-c

# 2. 直接构建（构建系统会自动初始化 submodules）
make
# 输出会显示:
# Initializing git submodules...
#   Checking out specific versions...
# ✓ Submodules initialized and checked out to specified versions

# 3. 继续构建
# make 会自动继续构建依赖和库
```

---

## 验证 Submodules 状态

### 检查 Submodules 是否正确初始化

```bash
# 方法 1：使用 git submodule status
git submodule status
# 正常输出（前面有空格）：
#  981e14290df3d4339974eade5d8820dafd506b58 ck (0.7.2)
#  5df3037d10556bfcb675bc73e516978b75fc7bc7 event (release-2.1.12-stable)
#  54eaed1d8b56b1aa528be3bdd1877e59c56fa90c jemalloc (5.3.0)

# 未初始化的输出（前面有减号）：
# -981e14290df3d4339974eade5d8820dafd506b58 ck
# -5df3037d10556bfcb675bc73e516978b75fc7bc7 event
# -54eaed1d8b56b1aa528be3bdd1877e59c56fa90c jemalloc

# 方法 2：检查目录是否存在且非空
ls -la ck event jemalloc | grep -E "^d"

# 方法 3：检查 .git 文件
cat ck/.git event/.git jemalloc/.git
# 应该输出类似: gitdir: ../.git/modules/ck
```

---

## 常见问题

### Q1: Clone 后 submodule 目录是空的

**原因**：没有使用 `--recurse-submodules` 选项

**解决**：
```bash
# 初始化 submodules
git submodule update --init --recursive

# 或使用构建系统
make init-submodules
```

### Q2: 构建时提示找不到 jemalloc/event/ck

**原因**：Submodules 未初始化

**解决**：
```bash
# 让构建系统自动处理
make clean && make

# 或手动初始化
git submodule update --init --recursive
make
```

### Q3: Submodules 版本不对

**原因**：Submodules 被手动切换到其他版本

**解决**：
```bash
# 使用构建系统重置到正确版本
make init-submodules

# 或手动重置
git submodule update --init --recursive
```

### Q4: 网络问题无法下载 Submodules

**症状**：
```
fatal: unable to access 'https://github.com/libevent/libevent.git/':
Failed to connect to github.com
```

**解决方案**：

1. **使用代理**：
   ```bash
   git config --global http.proxy http://proxy.example.com:8080
   ```

2. **使用 SSH 而不是 HTTPS**：
   ```bash
   # 修改 .gitmodules 中的 URL
   git config submodule.event.url git@github.com:libevent/libevent.git
   git config submodule.ck.url git@github.com:concurrencykit/ck.git
   git config submodule.jemalloc.url git@github.com:jemalloc/jemalloc.git

   # 然后初始化
   git submodule update --init --recursive
   ```

3. **手动下载**：
   如果实在无法访问 GitHub，可以手动下载 tar.gz 文件并解压到对应目录。

---

## 自动化测试

### 测试脚本

你可以创建一个测试脚本来验证 clone 后的构建流程：

```bash
#!/bin/bash
# test_clone.sh

set -e  # 遇到错误立即退出

echo "=== Testing Clone and Build ==="

# 1. 模拟 clone
TEMP_DIR=$(mktemp -d)
echo "Cloning to: $TEMP_DIR"
git clone --recurse-submodules . "$TEMP_DIR"

# 2. 验证 submodules
cd "$TEMP_DIR"
echo "Checking submodules..."
git submodule status

# 3. 构建
echo "Building..."
make

# 4. 验证构建产物
echo "Checking build artifacts..."
test -f lib/libtglog.a && echo "✓ libtglog.a exists"
test -f lib/libtglog-st.a && echo "✓ libtglog-st.a exists"

# 5. 清理
cd -
rm -rf "$TEMP_DIR"

echo "=== Test Passed ==="
```

---

## 推荐的 README.md 内容

在你的 `README.md` 中添加以下内容：

```markdown
## 快速开始

### 克隆项目

```bash
# 推荐：自动初始化 submodules
git clone --recurse-submodules https://github.com/your-username/sdk-c.git

# 或者普通 clone（构建时会自动初始化 submodules）
git clone https://github.com/your-username/sdk-c.git
```

### 构建

```bash
cd sdk-c

# 使用 Makefile
make

# 或使用 CMake
cmake . && make
```

**注意**：首次构建时，系统会自动下载并初始化第三方依赖（jemalloc、libevent、ck）。

### 依赖管理

本项目使用 git submodules 管理第三方依赖：
- jemalloc 5.3.0
- libevent release-2.1.12-stable
- ck 0.7.2

构建系统会自动处理这些依赖，无需手动操作。
```

---

## 总结

### 你的配置是正确的

- ✅ `.gitmodules` 已提交
- ✅ Submodule 引用已提交
- ✅ 构建系统会自动初始化 submodules
- ✅ 不会提交 submodule 的实际代码

### 别人 Clone 后

**最简单的方式**：
```bash
git clone --recurse-submodules <your-repo-url>
cd sdk-c
make
```

**如果忘记 --recurse-submodules**：
```bash
git clone <your-repo-url>
cd sdk-c
make  # 会自动初始化 submodules
```

### 关键点

1. **不需要担心**：你的配置是正确的，别人 clone 后可以正常构建
2. **自动初始化**：你的 Makefile 和 CMakeLists.txt 已经配置了自动初始化
3. **推荐方式**：建议在 README 中告诉用户使用 `--recurse-submodules`

### 参考文档

- `GIT_SUBMODULES_FAQ.md` - 详细的 FAQ
- `SUBMODULES.md` - Submodules 管理文档
- `SETUP_SUBMODULES.md` - 设置指南
