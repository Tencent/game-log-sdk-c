# CK (Concurrency Kit) 替换方案

## 背景

tglog SDK 的多线程版本 (`tglog/`) 和单线程版本 (`tglog-st/`) 依赖 CK (Concurrency Kit) 库提供无锁数据结构和原子操作。CK 库使用 GCC 内联汇编和 GCC 特有的内建函数，**无法在 MSVC（Windows）上编译**，导致 SDK 不能跨平台构建。

本方案用纯 C99 自包含的实现替换所有 CK 依赖，同时支持 GCC/Clang 和 MSVC 编译器。

## CK 使用分析

### 1. CK Ring Buffer (`ck_ring`)

| 模块 | 使用模式 | 说明 |
|------|---------|------|
| `tglog/worker.c` | MPSC (多生产者单消费者) | 多个业务线程写入，worker 线程消费 |
| `tglog-st/worker.c` | SPSC (单生产者单消费者) | 单线程应用，同一线程读写 |

用于消息传递队列，容量为 2 的幂次，使用类型安全的宏生成函数。

### 2. CK Hash Table (`ck_ht`)

| 模块 | 使用模式 | 说明 |
|------|---------|------|
| `tglog/worker.c` | 单线程访问 | 仅 worker 线程内部使用 |
| `tglog-st/worker.c` | 单线程访问 | 单线程环境 |

用于存储连接到 endpoint 的映射关系，不需要并发安全。

### 3. CK Stack (`ck_stack`)

| 模块 | 使用模式 | 说明 |
|------|---------|------|
| `tglog/pool.c` | MPMC (多生产者多消费者) | 内存池的空闲对象链表 |
| `tglog-st/pool.c` | SPNC/NPSC | 单线程环境下的对象池 |

多线程版本需要 ABA 保护来避免无锁栈的 ABA 问题。

### 4. CK Atomics (`ck_pr_*`)

| 函数 | 使用位置 | 说明 |
|------|---------|------|
| `ck_pr_load_64` | metrics.c, worker.c | 原子加载 64 位值 |
| `ck_pr_store_64` | metrics.c | 原子存储 64 位值 |
| `ck_pr_add_64` | metrics.c | 原子加 64 位值 |
| `ck_pr_faa_64` | client.c | fetch-and-add（返回旧值）|
| `ck_pr_inc_64` | worker.c | 原子自增 |
| `ck_pr_load_uint` | worker.c | 原子加载 unsigned int |
| `ck_pr_store_uint` | worker.c | 原子存储 unsigned int |
| `ck_pr_add_32` | test/tglog_test.c | 测试中的原子计数器 |
| `ck_pr_load_32` | test/tglog_test.c | 测试中的原子读取 |

## 替换设计

所有替换实现放在 `common/` 目录下，供两个 SDK 变体共用。

### `common/tglog_atomic.h` — 跨平台原子操作

**编译器分支**：
- **MSVC**: 使用 `_InterlockedXxx` 系列内建函数
- **GCC/Clang**: 使用 `__sync_*` 系列内建函数（C99 兼容，不依赖 C11 `<stdatomic.h>`）

**提供的操作**：

```
tglog_atomic_load_32 / store_32 / cas_32 / add_32     -- 32 位
tglog_atomic_load_64 / store_64 / cas_32              -- 64 位
tglog_atomic_faa_64 / add_64 / inc_64                 -- 64 位算术
tglog_atomic_load_uint / store_uint                   -- unsigned int
tglog_atomic_fence                                    -- 内存屏障
```

**设计选择**：
- 选择 `__sync_*` 而非 `__atomic_*`，因为 `__sync_*` 在更老的 GCC 版本上也可用
- MSVC 路径使用 `__forceinline` + `#pragma intrinsic` 确保零函数调用开销
- 所有函数为 `static inline`，头文件即实现

### `common/tglog_ring.h` — 无锁环形缓冲区

**Header-only**，使用宏 `TGLOG_RING_PROTOTYPE(name, type)` 生成类型安全函数。

**支持的模式**：
- **SPSC**：`tglog_ring_enqueue_spsc` / `tglog_ring_dequeue_spsc` — 单生产者单消费者，无需 CAS
- **MPSC**：`tglog_ring_enqueue_mpsc` / `tglog_ring_dequeue_spsc` — 多生产者用 CAS 竞争写入

**实现细节**：
- 容量必须是 2 的幂次，用位掩码代替取模
- `p_head` / `p_tail`（生产者）和 `c_head` / `c_tail`（消费者）分别用 cache line 对齐填充，避免伪共享
- MPSC 的 enqueue 使用 `__sync_bool_compare_and_swap` 在 `p_head` 上自旋

### `common/tglog_stack.h` — 无锁栈

**Header-only**。

**操作**：
- `tglog_stack_push_mpmc` / `tglog_stack_pop_mpmc` — 多线程安全（tglog 使用）
- `tglog_stack_push_spnc` / `tglog_stack_pop_npsc` — 单线程（tglog-st 使用）

**ABA 保护**（仅 MPMC 模式需要）：

ABA 问题：线程 A 读到栈顶 `X`，被挂起；线程 B pop `X`、push `Y`、再 push `X` 回来；线程 A 恢复后 CAS 仍然成功但栈结构已损坏。

解决方案：使用 **tagged pointer**（generation counter）+ **double-width CAS**：

| 平台 | 栈结构大小 | CAS 宽度 | 实现 |
|------|-----------|----------|------|
| MSVC 64-bit | 16 字节 (ptr + gen) | 128-bit | `_InterlockedCompareExchange128` |
| GCC/Clang 64-bit | 16 字节 | 128-bit | `__sync_val_compare_and_swap` on `__int128` |
| 32-bit | 8 字节 (ptr + gen) | 64-bit | `__sync_val_compare_and_swap` on `uint64_t` |

每次 push/pop 时递增 generation counter，使得即使指针值相同，tagged pointer 也不同，CAS 会正确失败。

> **注意**：x86-64 上 128-bit CAS 需要编译器标志 `-mcx16`，已在 `tglog/Makefile` 和 `tglog/CMakeLists.txt` 中添加。

### `common/tglog_ht.h` + `common/tglog_ht.c` — 哈希表

因为哈希表仅在单线程上下文中使用，实现为**简单的开放寻址哈希表**（线性探测）。

**两种 key 模式**：
- `TGLOG_HT_MODE_DIRECT`：key 是整数/指针值，直接比较
- `TGLOG_HT_MODE_BYTESTRING`：key 是字节序列，用 `memcmp` 比较

**API**:
```c
tglog_ht_init(ht, mode, initial_entries)   // 初始化
tglog_ht_destroy(ht)                       // 销毁
tglog_ht_hash(hash_out, key, key_len)      // 计算哈希
tglog_ht_set(ht, hash, key, key_len, val)  // 插入/更新
tglog_ht_get(ht, hash, key, key_len, val)  // 查找
tglog_ht_remove(ht, hash, key, key_len)    // 删除
tglog_ht_count(ht)                         // 元素数量
tglog_ht_reset(ht)                         // 清空
tglog_ht_iterator_init(iter, ht)           // 迭代器初始化
tglog_ht_next(iter, entry)                 // 迭代下一个
```

这是唯一需要 `.c` 文件的模块（其余均为 header-only），因为实现较长且包含动态内存分配。

## 替换映射表

### 头文件替换

| CK 头文件 | 替换为 |
|----------|--------|
| `ck_pr.h` | `tglog_atomic.h` |
| `ck_ring.h` | `tglog_ring.h` |
| `ck_stack.h` | `tglog_stack.h` |
| `ck_ht.h` | `tglog_ht.h` |

### API 替换

| CK API | 替换 API |
|--------|---------|
| `ck_pr_load_64` | `tglog_atomic_load_64` |
| `ck_pr_store_64` | `tglog_atomic_store_64` |
| `ck_pr_add_64` | `tglog_atomic_add_64` |
| `ck_pr_faa_64` | `tglog_atomic_faa_64` |
| `ck_pr_inc_64` | `tglog_atomic_inc_64` |
| `ck_pr_load_uint` | `tglog_atomic_load_uint` |
| `ck_pr_store_uint` | `tglog_atomic_store_uint` |
| `ck_pr_add_32` | `tglog_atomic_add_32` |
| `ck_pr_load_32` | `tglog_atomic_load_32` |
| `ck_pr_fence_store` | `tglog_atomic_fence` |
| `ck_ring_init` | `tglog_ring_init` |
| `ck_ring_enqueue_spsc` | `tglog_ring_enqueue_spsc_*` |
| `ck_ring_dequeue_spsc` | `tglog_ring_dequeue_spsc_*` |
| `ck_ring_enqueue_mpsc` | `tglog_ring_enqueue_mpsc_*` |
| `ck_stack_push_mpmc` | `tglog_stack_push_mpmc` |
| `ck_stack_pop_mpmc` | `tglog_stack_pop_mpmc` |
| `ck_stack_push_spnc` | `tglog_stack_push_spnc` |
| `ck_stack_pop_npsc` | `tglog_stack_pop_npsc` |
| `ck_ht_init` | `tglog_ht_init` |
| `ck_ht_set` | `tglog_ht_set` |
| `ck_ht_get` | `tglog_ht_get` |
| `ck_ht_remove` | `tglog_ht_remove` |

## 修改的文件

### 新增文件

| 文件 | 说明 |
|------|------|
| `common/tglog_atomic.h` | 跨平台原子操作 |
| `common/tglog_ring.h` | 无锁环形缓冲区 |
| `common/tglog_stack.h` | 无锁栈 |
| `common/tglog_ht.h` | 哈希表头文件 |
| `common/tglog_ht.c` | 哈希表实现 |

### 修改的源文件

| 文件 | 修改内容 |
|------|---------|
| `tglog/worker.c` | 替换 ring、stack、ht、atomic 的所有 CK 调用 |
| `tglog/client.c` | `ck_pr_faa_64` → `tglog_atomic_faa_64` |
| `tglog/pool.c` | CK stack → tglog_stack |
| `tglog/request.c` | CK ring 类型和宏 → tglog_ring |
| `tglog/metrics.c` | CK atomic → tglog_atomic |
| `tglog/conf.c` | CK include → tglog include |
| `tglog-st/worker.c` | 替换 ring、stack、ht、atomic 的所有 CK 调用 |
| `tglog-st/client.c` | CK include → tglog include |
| `tglog-st/pool.c` | CK stack → tglog_stack |
| `tglog-st/request.c` | CK ring 类型和宏 → tglog_ring |
| `tglog-st/metrics.c` | CK atomic → tglog_atomic |
| `tglog-st/conf.c` | CK include → tglog include |
| `test/tglog_test.c` | `ck_pr_add_32/load_32` → `tglog_atomic_add_32/load_32` |
| `bindings/python/tglog_cython.pyx` | `ck_pr_add_64` → `tglog_atomic_add_64` |

### 修改的构建文件

| 文件 | 修改内容 |
|------|---------|
| `Makefile` (顶层) | 移除 CK 变量、构建规则、安装复制、pkg-config、README 生成、help |
| `CMakeLists.txt` (顶层) | 移除 CK ExternalProject、imported library、安装规则 |
| `tglog/Makefile` | 移除 CK include/lib 路径，添加 `common/`，添加 `-mcx16` |
| `tglog/CMakeLists.txt` | 同上 |
| `tglog-st/Makefile` | 移除 CK include/lib 路径，添加 `common/` |
| `tglog-st/CMakeLists.txt` | 同上 |
| `test/Makefile` | 移除 CK，添加 `common/` |
| `test/CMakeLists.txt` | 同上，添加 `-mcx16` |
| `test-st/Makefile` | 移除 CK，添加 `common/` |
| `test-st/CMakeLists.txt` | 移除 CK |
| `test-tapp/Makefile` | 移除 CK，添加 `common/` |
| `test-tapp/CMakeLists.txt` | 移除 CK |
| `bindings/python/setup_ext.py` | 移除 CK include/lib/rpath |
| `bindings/python/setup_cython.py` | 同上 |

## 验证结果

### Makefile 构建

```
make clean && make tglog      # 多线程库 — 通过
make tglog-st                 # 单线程库 — 通过
make test                     # 测试程序 — 通过
make test-tapp                # TAPP 集成测试 — 通过
```

### CMake 构建

```
rm -rf build && cmake -S . -B build && cmake --build build -j4
```

所有目标通过：`libtglog.a/.so`、`libtglog-st.a/.so`、测试、test-tapp、Python bindings（ctypes + Cython）。
