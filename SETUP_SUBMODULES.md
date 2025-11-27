# 设置 Git Submodules 指南

## 当前状态检查

你的项目中 submodules 已经在本地初始化，但可能还没有提交到 git 仓库。

### 检查方法

```bash
# 检查 .gitmodules 是否已提交
git ls-files .gitmodules

# 检查 submodules 是否已添加到 git
git ls-tree HEAD | grep "^160000"

# 如果上面两个命令都没有输出，说明 submodules 还没有提交
```

## 如何正确提交 Submodules

### 步骤 1：确保 .gitmodules 文件存在

```bash
cat .gitmodules
```

应该看到类似内容：
```
[submodule "event"]
    path = event
    url = https://github.com/libevent/libevent.git
[submodule "ck"]
    path = ck
    url = https://github.com/concurrencykit/ck.git
[submodule "jemalloc"]
    path = jemalloc
    url = https://github.com/jemalloc/jemalloc.git
```

### 步骤 2：添加 .gitmodules 到 git

```bash
git add .gitmodules
```

### 步骤 3：添加 submodule 引用

```bash
# 添加所有 submodules 的 commit 引用
git add event jemalloc ck
```

**重要**：这里 `git add` 只会添加 submodule 的 commit 引用（gitlink），不会添加 submodule 内部的文件。

### 步骤 4：提交

```bash
git commit -m "Add git submodules for third-party dependencies

- jemalloc 5.3.0
- libevent release-2.1.12-stable
- ck 0.7.2
"
```

### 步骤 5：推送到远程仓库

```bash
git push origin master
```

## 验证

提交后，验证 submodules 是否正确配置：

```bash
# 应该看到 160000 模式（gitlink）
git ls-tree HEAD | grep "^160000"

# 输出示例：
# 160000 commit 981e14290df3d4339974eade5d8820dafd506b58  ck
# 160000 commit 5df3037d10556bfcb675bc73e516978b75fc7bc7  event
# 160000 commit 54eaed1d8b56b1aa528be3bdd1877e59c56fa90c  jemalloc
```

## 如果 Submodules 已经以普通文件方式提交了

如果你之前错误地把 submodule 的内容作为普通文件提交了，需要清理：

### 步骤 1：从 git 中删除（但保留本地文件）

```bash
git rm --cached -r event jemalloc ck
```

### 步骤 2：确保 .gitignore 不包含这些目录

检查 `.gitignore`，确保没有这些行：
```
event/
jemalloc/
ck/
```

### 步骤 3：重新添加为 submodules

```bash
# 如果 .gitmodules 已存在，直接添加引用
git add .gitmodules event jemalloc ck

# 如果 .gitmodules 不存在，需要重新添加 submodules
git submodule add https://github.com/libevent/libevent.git event
git submodule add https://github.com/jemalloc/jemalloc.git jemalloc
git submodule add https://github.com/concurrencykit/ck.git ck
```

### 步骤 4：切换到指定版本

```bash
cd event && git checkout release-2.1.12-stable && cd ..
cd jemalloc && git checkout 5.3.0 && cd ..
cd ck && git checkout 0.7.2 && cd ..
```

### 步骤 5：提交更改

```bash
git add .gitmodules event jemalloc ck
git commit -m "Convert to git submodules"
git push origin master
```

## 其他开发者如何使用

一旦你提交了 submodules 配置，其他开发者可以：

### 方法 1：Clone 时自动初始化

```bash
git clone --recurse-submodules <repository-url>
```

### 方法 2：Clone 后初始化

```bash
git clone <repository-url>
cd sdk-c

# 使用构建系统自动初始化（推荐）
make init-submodules
# 或
cmake .

# 或手动初始化
git submodule update --init --recursive
```

## 常见问题

### Q: 为什么 git status 显示 submodule 目录？

如果看到：
```bash
$ git status
未跟踪的文件:
  event
  jemalloc
```

可能的原因：
1. **Submodules 还没有提交** - 按照上面的步骤提交
2. **Submodule 内部有未提交的更改** - 这是正常的，不影响主仓库
3. **Submodule 的 commit 发生了变化** - 需要决定是否更新主仓库的引用

### Q: 如何检查 submodule 内部的状态？

```bash
# 查看所有 submodules 的状态
git submodule status

# 查看某个 submodule 内部的详细状态
cd event
git status
cd ..
```

### Q: 需要在 .gitignore 中添加 submodule 目录吗？

**不需要！** 永远不要在 `.gitignore` 中添加 submodule 目录。Git 会自动正确处理它们。

## 检查清单

在提交前，确保：

- [ ] `.gitmodules` 文件存在并包含所有 submodules
- [ ] 每个 submodule 都在正确的版本/tag 上
- [ ] `.gitignore` 不包含 submodule 目录名
- [ ] 运行 `git submodule status` 确认状态正常
- [ ] 运行 `git ls-tree HEAD | grep "^160000"` 确认 submodules 以 gitlink 方式存储

## 参考

- `GIT_SUBMODULES_FAQ.md` - 详细的 FAQ 文档
- `SUBMODULES.md` - Submodules 管理文档
