# common/ 目录代码审查报告

审查日期：2026-04-08

审查范围：`common/` 目录下的无锁数据结构实现及 Windows 平台兼容层

---

## 一、tglog_atomic.h — 原子操作

### 问题 1（严重）：GCC 32 位平台 `tglog_atomic_store_64` 非原子

**位置**：第 192-195 行

```c
static inline void tglog_atomic_store_64(uint64_t *p, uint64_t v) {
    __sync_synchronize();
    *(volatile uint64_t *)p = v;  // 32 位平台上这是两条 mov 指令，非原子
}
```

**影响**：在 32 位 x86 上，64 位写入会被拆成两条 32 位 `mov` 指令，读取端可能看到"撕裂"的值（高 32 位和低 32 位来自不同的写入）。

**建议修复**：

```c
static inline void tglog_atomic_store_64(uint64_t *p, uint64_t v) {
    uint64_t old = *p;
    while (!__sync_bool_compare_and_swap(p, old, v)) {
        old = *p;
    }
}
```

---

### 问题 2（中等）：MSVC `tglog_atomic_load_64` 丢弃 const 并对只读地址执行写操作

**位置**：第 94-96 行

```c
static __forceinline uint64_t tglog_atomic_load_64(const uint64_t *p) {
    return (uint64_t)_InterlockedOr64((volatile __int64 *)p, 0);
}
```

**影响**：`_InterlockedOr64` 是一个 read-modify-write 操作（`*p |= 0`），会对目标地址执行写操作。传入 `const uint64_t *` 被强转后丢弃了 const，若 `p` 指向只读内存（如 ROM 映射区域），会触发访问违规。此外在 x86-64 上对齐的 64 位 load 本身就是原子的，使用 RMW 是多余的开销。

**建议修复**：与 `load_32` 保持一致，使用 volatile 读取 + 编译器屏障：

```c
static __forceinline uint64_t tglog_atomic_load_64(const uint64_t *p) {
    uint64_t v = *((volatile uint64_t *)p);
    _ReadBarrier();
    return v;
}
```

---

### 问题 3（轻微）：`_ReadBarrier()` / `_WriteBarrier()` 已被 MSVC 标记为 deprecated

**位置**：多处

**影响**：功能上在 x86/x64 配合 volatile 是正确的，但微软已标记为 deprecated。

**建议**：长期考虑迁移到 `_Compiler_barrier()` 或 C11 `<stdatomic.h>`。

---

### 问题 4（轻微）：GCC/Clang 所有 fence 都是 `__sync_synchronize()`（全屏障）

**位置**：第 246-248 行

```c
#define tglog_fence_store()   __sync_synchronize()
#define tglog_fence_load()    __sync_synchronize()
#define tglog_fence_memory()  __sync_synchronize()
```

**影响**：在 ARM/AArch64 等弱内存序架构上编译为 `dmb ish`（全屏障），而 store fence 只需 `dmb ishst`，load fence 可用 acquire。x86 上无影响，但若未来需支持 ARM 服务器，性能会有损失。

---

## 二、tglog_ring.h — 无锁环形缓冲区

### 问题 5（严重）：MPSC 模式下 producer 被抢占会导致全系统阻塞

**位置**：第 185-186 行

```c
while (tglog_atomic_load_uint(&ring->p_tail) != producer)
    tglog_pause();
```

**影响**：Producer A 通过 CAS 预留了 slot N，随后被 OS 抢占（调度走/页错误/信号等）。此时：

- 所有后续 producer（已预留 slot N+1, N+2, ...）全部卡在这个 spin-wait
- Consumer 看不到任何新数据（因为 `p_tail` 停在 N 不前进）
- 一个被抢占的线程可以让整个日志系统停滞

这是 "两阶段提交" MPSC ring 的固有局限。

**建议**：
1. 至少在代码中添加注释说明此局限
2. 考虑添加超时退避机制（spin N 次后 yield，再超时后标记 slot 为无效）
3. 或者改用其他不会阻塞的 MPSC 方案（如链表队列）

---

### 问题 6（轻微）：`tglog_ring_size` 返回值非精确

**位置**：第 73-79 行

```c
static inline unsigned int tglog_ring_size(const struct tglog_ring *ring) {
    unsigned int c, p;
    c = tglog_atomic_load_uint(&ring->c_head);
    p = tglog_atomic_load_uint(&ring->p_tail);
    return (p - c) & ring->mask;
}
```

**影响**：两次 load 不是原子快照，之间可能发生变化。返回值仅供参考。

**建议**：添加注释说明这是 approximate size，不应用于精确流控判断。

---

### 问题 7（轻微）：`tglog_ring_init` 不验证 size 是否为 2 的幂

**位置**：第 64-70 行

```c
static inline void tglog_ring_init(struct tglog_ring *ring, unsigned int size) {
    ring->size = size;
    ring->mask = size - 1;  // 若 size 不是 2 的幂，mask 计算错误
```

**影响**：调用者若传入非 2 的幂（如 100），所有 `& mask` 索引运算会完全错乱。

**建议**：添加 assert 验证，或自动向上取整到 2 的幂：

```c
assert((size & (size - 1)) == 0 && "size must be a power of 2");
```

---

## 三、tglog_stack.h — 无锁栈

### 问题 8（中等）：MPMC pop 中 `head->next` 存在 use-after-free 风险

**位置**：第 248-249 行

```c
tglog_fence_load();
next = head->next;   // 此时 head 可能已被其他线程 pop 并 free
```

**时序**：
1. 线程 A 读到 `head = X`，`gen = 5`
2. 线程 B pop X，free X，将 X 的内存归还 OS
3. 线程 A 读 `X->next` — use-after-free

Generation counter 只能保证 CAS 不会错误成功（ABA 保护），但不能阻止 CAS 之前对已释放节点的解引用。

**项目中的缓解措施**：项目使用内存池（`pool.c/h`），pop 出的节点不会被归还 OS，因此 `head->next` 的读取虽然可能读到过期数据，但不会 segfault，CAS 会失败并重试。

**风险**：如果有任何路径使用了标准 `free()` 而非归还内存池，就是真实的 crash 风险。建议在代码注释中明确标注此约束。

---

### 问题 9（补充说明）：MPMC push 只做指针 CAS 不更新 generation

**位置**：第 221-226 行

这是正确的设计选择：push 操作不受 ABA 影响（只需保证 head 一致即可），generation 只在 pop 时递增。此处无需修改，仅记录说明。

---

## 四、tglog_platform.h — Windows 平台兼容层

### 问题 10（严重）：`gettimeofday` 和 `tglog_win_qpc_time_us` 的静态变量初始化存在竞态条件

**位置**：第 100-116 行，第 121-129 行

```c
// tglog_win_qpc_time_us 中
static LARGE_INTEGER freq = {0};
static LARGE_INTEGER start_counter = {0};
static uint64_t start_epoch_us = 0;

if (freq.QuadPart == 0) {
    // 多个线程可能同时进入这里
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start_counter);
    GetSystemTimeAsFileTime(&ft);
    start_epoch_us = tglog_win_filetime_to_epoch_us(&ft);
}
```

```c
// gettimeofday 中
static tglog_get_system_time_precise_as_file_time_fn precise_time_fn = NULL;
static int precise_time_checked = 0;

if (!precise_time_checked) {
    // 多个线程可能同时进入这里
    ...
    precise_time_checked = 1;
}
```

**影响**：
- `tglog_win_qpc_time_us`：两个线程读到 `freq == 0`，各自调用 `QueryPerformanceCounter` 获得不同的 `start_counter`，后写入的覆盖先写入的，导致时间戳计算不一致
- `gettimeofday`：编译器可能将 `precise_time_checked = 1` 重排到 `precise_time_fn` 赋值之前（没有 barrier），其他线程可能读到 `checked=1` 但 `fn=NULL`

**建议修复**：
1. 使用 `InitOnceExecuteOnce` API 做线程安全的一次性初始化
2. 或者提供显式的 `tglog_platform_init()` 函数，在程序启动时（单线程阶段）调用
3. 或者使用 `InterlockedCompareExchange` 手动实现 double-checked locking 并加入内存屏障

---

### 问题 11（中等）：errno 兼容宏实际上不生效

**位置**：第 163-171 行

```c
#ifndef EAGAIN
#define EAGAIN       WSAEWOULDBLOCK
#endif
#ifndef EWOULDBLOCK
#define EWOULDBLOCK  WSAEWOULDBLOCK
#endif
#ifndef ENOBUFS
#define ENOBUFS      WSAENOBUFS
#endif
```

**影响**：MSVC 的 `<errno.h>` 已经定义了 `EAGAIN`（值 11）、`EWOULDBLOCK`（值 140）、`ENOBUFS`，所以 `#ifndef` 条件为假，这些宏完全不生效。而 Winsock 错误码通过 `WSAGetLastError()` 返回（如 `WSAEWOULDBLOCK = 10035`），与 C 标准 errno 值完全不同。

如果 SDK 代码中有 `if (errno == EAGAIN)` 来判断 socket 阻塞，在 Windows 上会永远不匹配。

**建议修复**：
1. 在 socket 错误检查处统一封装一个跨平台宏，Windows 上使用 `WSAGetLastError()`
2. 或 `#undef` 后重新定义（但可能影响非 socket 的 errno 使用）

---

### 问题 12（轻微）：`getopt` / `optarg` 等变量是 file-scope `static`

**位置**：第 236-239 行

```c
static char *optarg = NULL;
static int   optind = 1;
static int   opterr = 1;
static int   optopt = '?';
```

**影响**：每个 `#include "tglog_platform.h"` 的翻译单元都会得到独立的一份 `optarg`/`optind`。如果 `getopt` 在不同 `.c` 文件中被调用，状态不共享，解析会错乱。

**建议**：将这些变量声明放到单独的 `.c` 文件中，头文件中用 `extern` 声明。实际上 getopt 通常只在 main 所在的 TU 中调用，所以风险较低。

---

### 问题 13（轻微）：`usleep` 短延迟纯自旋等待浪费 CPU

**位置**：第 207-212 行

```c
for (;;) {
    QueryPerformanceCounter(&now);
    elapsed = ...;
    if (elapsed >= target_us) break;
}
```

**影响**：< 2ms 的 delay 纯自旋 100% 占用 CPU 核心。

**建议**：在自旋循环中加入 `_mm_pause()` / `YieldProcessor()` 减少功耗和超线程干扰：

```c
for (;;) {
    QueryPerformanceCounter(&now);
    elapsed = ...;
    if (elapsed >= target_us) break;
    _mm_pause();
}
```

---

## 五、tglog_ht.c/h — 哈希表

哈希表为单线程设计，无并发安全问题。

### 问题 14（轻微）：`tglog_ht_entry` 结构体字段排列存在 padding 浪费

**位置**：`tglog_ht.h` 第 42-48 行

```c
struct tglog_ht_entry {
    uintptr_t key;        // 8B
    uintptr_t value;      // 8B
    uint16_t key_length;  // 2B + 6B padding（对齐到 hash）
    uint64_t hash;        // 8B
    uint8_t state;        // 1B + 7B padding（对齐到结构体尾部）
};                        // 总计 40B
```

**建议**：重排为 `{key, value, hash, key_length, state}` 可减到 32 字节：

```c
struct tglog_ht_entry {
    uintptr_t key;        // 8B
    uintptr_t value;      // 8B
    uint64_t hash;        // 8B
    uint16_t key_length;  // 2B
    uint8_t state;        // 1B + 5B padding
};                        // 总计 32B
```

每个 entry 节省 8 字节，对大哈希表缓存友好度有显著提升。

---

### 问题 15（轻微）：`_next_power_of_2` 对边界值溢出

**位置**：`tglog_ht.c` 第 46-56 行

当 `n = UINT64_MAX` 时，经过 `n--` 和所有 OR 移位后，`n + 1` 溢出为 0，导致分配 0 字节的表。实际场景几乎不会传入如此大的值。

**建议**：如需健壮可加上限检查。

---

## 汇总

| 严重程度 | 问题 | 文件 | 状态 | 影响 |
|---------|------|------|------|------|
| 严重 | GCC 32 位 `store_64` 非原子 | `tglog_atomic.h` | **已修复** | 数据撕裂 |
| 严重 | Windows 静态初始化竞态 | `tglog_platform.h` | **已修复** | 多线程时间戳不一致 |
| 严重 | MPSC ring producer 被抢占导致阻塞 | `tglog_ring.h` | **已修复** | 日志系统卡死 |
| 中等 | MSVC `load_64` 丢弃 const 执行 RMW | `tglog_atomic.h` | **已修复** | 潜在写入只读内存 |
| 中等 | MPMC stack pop use-after-free 风险 | `tglog_stack.h` | **已加注释** | 依赖内存池缓解 |
| 中等 | errno 映射宏不生效 | `tglog_platform.h` | **已修复** | socket 错误检查失效 |
| 轻微 | 全屏障替代弱屏障 | `tglog_atomic.h` | 暂不修复 | ARM 性能损失 |
| 轻微 | ring_init 不验证 power-of-2 | `tglog_ring.h` | **已修复** | 误用导致逻辑错误 |
| 轻微 | hash entry 结构体 padding 浪费 | `tglog_ht.h` | **已修复** | 内存/缓存效率 |
| 轻微 | usleep 自旋无 pause 指令 | `tglog_platform.h` | **已修复** | CPU 浪费 |
| 轻微 | getopt 变量 file-scope static | `tglog_platform.h` | 暂不修复 | 多 TU 状态不共享 |
| 轻微 | `_next_power_of_2` 边界溢出 | `tglog_ht.c` | 暂不修复 | 极端输入下异常 |
