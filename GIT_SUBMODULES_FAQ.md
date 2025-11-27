# Git Submodules FAQ

## Q: Submodules 的代码需要提交到我的仓库吗？

**答：不需要！** Git Submodules 的设计就是为了避免这个问题。

### Git Submodules 的工作原理

当你使用 git submodules 时，你的仓库中**只存储**：

1. **`.gitmodules` 文件** - 记录 submodule 的配置
   ```
   [submodule "event"]
       path = event
       url = https://github.com/libevent/libevent.git
   ```

2. **Commit 引用** - 一个指向 submodule 特定 commit 的指针
   ```bash
   $ git ls-tree HEAD event
   160000 commit 5df3037d10556bfcb675bc73e516978b75fc7bc7  event
   ```
   注意这里是 `160000` 模式（gitlink），不是普通文件。

3. **不存储实际代码** - submodule 目录中的所有文件都不会被提交到主仓库

### 验证方法

```bash
# 查看主仓库中 submodule 的存储方式
$ git ls-tree HEAD | grep -E "(event|jemalloc|ck)"
160000 commit 981e14290df3d4339974eade5d8820dafd506b58  ck
160000 commit 5df3037d10556bfcb675bc73e516978b75fc7bc7  event
160000 commit 54eaed1d8b56b1aa528be3bdd1877e59c56fa90c  jemalloc

# 160000 表示这是一个 gitlink（submodule 引用），不是文件或目录
```

## Q: 需要在 .gitignore 中忽略 submodules 吗？

**答：不需要！** 实际上**不应该**在 `.gitignore` 中添加 submodule 目录。

### 原因

1. **Git 自动处理**：Git 知道这些是 submodules，会自动正确处理它们
2. **添加到 .gitignore 会导致问题**：
   - Git 会忽略 submodule 的状态变化
   - 无法正确跟踪 submodule 的 commit 引用更新
   - 其他开发者 clone 时可能无法正确初始化 submodules

### 正确的配置

你的 `.gitignore` 应该忽略的是：

```gitignore
# 编译生成的第三方库（已配置）
thirdparty/*

# 但不要添加这些（错误示例）：
# event/          ❌ 错误！
# jemalloc/       ❌ 错误！
# ck/             ❌ 错误！
```

## Q: 为什么 git status 显示 submodule 有未跟踪文件？

这是正常的！例如：

```bash
$ git status
位于分支 master
未跟踪的文件:
  event
  jemalloc
```

这通常表示：
1. **Submodule 内部有未提交的文件**（如编译生成的文件）
2. **Submodule 的 commit 发生了变化**

### 检查 submodule 内部状态

```bash
# 查看 submodule 详细状态
$ git submodule status
 981e14290df3d4339974eade5d8820dafd506b58 ck (0.7.2)
 5df3037d10556bfcb675bc73e516978b75fc7bc7 event (release-2.1.12-stable)
 54eaed1d8b56b1aa528be3bdd1877e59c56fa90c jemalloc (5.3.0)

# 前面有空格表示正常，有 + 表示 commit 变化，有 - 表示未初始化

# 查看 submodule 内部的未跟踪文件
$ cd event && git status --short
?? build-aux/        # 编译生成的文件
?? configure~        # 临时文件
```

### 清理 submodule 内部的未跟踪文件

```bash
# 清理单个 submodule
cd event
git clean -fd        # 删除未跟踪的文件和目录
cd ..

# 或者清理所有 submodules
git submodule foreach 'git clean -fd'
```

## Q: 如何更新 submodule 的版本？

### 方法 1：使用构建系统（推荐）

只需修改版本号，构建系统会自动处理：

**Makefile:**
```makefile
JEMALLOC_VERSION = 5.3.0
EVENT_VERSION = release-2.1.12-stable
CK_VERSION = 0.7.2
```

**CMakeLists.txt:**
```cmake
set(JEMALLOC_VERSION "5.3.0")
set(EVENT_VERSION "release-2.1.12-stable")
set(CK_VERSION "0.7.2")
```

然后运行：
```bash
make init-submodules
# 或
cmake .
```

### 方法 2：手动更新

```bash
# 进入 submodule 目录
cd jemalloc

# 切换到新版本
git checkout 5.3.0

# 返回主仓库
cd ..

# 提交 submodule 引用的变化
git add jemalloc
git commit -m "Update jemalloc to 5.3.0"
```

## Q: 其他开发者如何获取 submodules？

### 方法 1：Clone 时自动初始化

```bash
git clone --recurse-submodules <repository-url>
```

### 方法 2：Clone 后初始化

```bash
git clone <repository-url>
cd sdk-c

# 方式 A：使用构建系统（推荐）
make init-submodules
# 或
cmake .

# 方式 B：手动初始化
git submodule update --init --recursive
```

## Q: Submodule 的 .git 目录在哪里？

从 Git 1.7.8 开始，submodule 的 `.git` 不再是目录，而是一个文件：

```bash
$ cat event/.git
gitdir: ../.git/modules/event
```

实际的 git 数据存储在主仓库的 `.git/modules/` 目录中：

```
.git/
├── modules/
│   ├── event/          # event submodule 的 git 数据
│   ├── jemalloc/       # jemalloc submodule 的 git 数据
│   └── ck/             # ck submodule 的 git 数据
```

这样设计的好处：
- 删除 submodule 目录不会丢失 git 历史
- 更容易管理和备份

## Q: 如何完全删除一个 submodule？

```bash
# 1. 从 .gitmodules 中删除配置
git config -f .gitmodules --remove-section submodule.event

# 2. 从 .git/config 中删除配置
git config -f .git/config --remove-section submodule.event

# 3. 从 git 索引中删除
git rm --cached event

# 4. 删除目录
rm -rf event

# 5. 删除 .git/modules 中的数据
rm -rf .git/modules/event

# 6. 提交更改
git commit -m "Remove event submodule"
```

## 总结

✅ **正确做法：**
- 让 Git 自动处理 submodules
- 不要在 `.gitignore` 中添加 submodule 目录
- 使用构建系统自动管理 submodule 版本
- 只提交 `.gitmodules` 和 submodule 的 commit 引用

❌ **错误做法：**
- 在 `.gitignore` 中添加 submodule 目录
- 手动复制 submodule 的代码到主仓库
- 尝试提交 submodule 内部的文件到主仓库

## 参考资料

- [Git Submodules 官方文档](https://git-scm.com/book/en/v2/Git-Tools-Submodules)
- 项目文档：`SUBMODULES.md`
