# CK 替换方案 Review

## 结论

整体方案设计合理，替换方向正确，当前实现已经能在现有 Linux/GCC 构建链路下通过编译与集成验证。

本次 review 里只有 **1 个必须修复的问题**，其余主要是性能和工程细节优化。

---

## 必须修复

### 1. `common/tglog_stack.h:35`

**问题**：`tglog_stack` 在 64 位平台上使用 128-bit CAS（`cmpxchg16b` / `_InterlockedCompareExchange128` / `__sync_val_compare_and_swap` on `__int128`）做 ABA 保护，但结构体本身没有显式声明 16 字节对齐。

当前定义：

```c
struct tglog_stack {
    struct tglog_stack_entry *head;
    uintptr_t generation;
};
```

**风险**：
- x86-64 上 `cmpxchg16b` 要求目标地址 16 字节对齐
- 如果 `tglog_stack_t` 被嵌入到别的结构中，最终地址只有 8 字节对齐，就可能在运行时触发崩溃或未定义行为

**建议**：为 `struct tglog_stack` 增加 16 字节对齐声明。

示例：

```c
#if defined(_MSC_VER)
__declspec(align(16))
#endif
struct tglog_stack {
    struct tglog_stack_entry *head;
    uintptr_t generation;
}
#if defined(__GNUC__) || defined(__clang__)
__attribute__((aligned(16)))
#endif
;
```

---

## 建议优化

### 2. `common/tglog_ring.h:219`

**问题**：MPSC enqueue 在等待前序 producer 提交时使用空转：

```c
while (tglog_atomic_load_uint(&ring->p_tail) != producer)
    ;
```

**影响**：
- 在高竞争下会增加 CPU 占用
- 对 SMT/超线程场景不友好
- 比 CK 原实现少了 `stall/pause` 提示

**建议**：在 spin loop 中加入 pause/yield 指令，例如 `_mm_pause()` 或 `__builtin_ia32_pause()`。

---

### 3. `common/tglog_atomic.h:93` 与 `common/tglog_atomic.h:229`

**问题**：64 位 load 当前通过 RMW 方式实现：
- MSVC: `_InterlockedOr64(..., 0)`
- GCC/Clang: `__sync_fetch_and_add(..., 0)`

**影响**：
- 正确性没问题
- 但会引入额外 cache line 竞争和性能损耗

**建议**：
- 如果目标平台限定为 64 位且保证自然对齐，可考虑改成更轻量的 load + barrier
- 如果要优先保守正确性，也可以维持现状

---

## 已确认没有明显问题的部分

### Ring Buffer
- `common/tglog_ring.h` 中 SPSC/MPSC 的核心流程和 fence 位置整体正确
- `TGLOG_RING_PROTOTYPE` 的实际展开是合法的，当前 `tglog/request.h:44`、`tglog/request.h:209` 的使用方式可正常工作

### Stack
- `common/tglog_stack.h` 的 SPNC/NPSC 单线程路径没有明显问题
- MPMC 路径使用 generation counter + double-width CAS 做 ABA 保护，思路正确

### Atomic
- 32 位 atomic 的 load/store/CAS/add 映射没有明显问题
- pointer CAS 相关封装可以满足当前替换需求

### Hash Table
- `common/tglog_ht.h` / `common/tglog_ht.c` 作为单线程 worker 内部表使用，开放寻址方案是合理的
- `tglog/worker.c:66` 起的 endpoint retry / unavailable endpoint / delayed close 等调用方式，与当前哈希表接口能对上

### 构建集成
- `tglog/Makefile:20`、`tglog/CMakeLists.txt:19` 已补 `-mcx16`
- `common/tglog_ht.c` 已被加入 `tglog` / `tglog-st` 构建输入
- 本地 `make clean && make tglog tglog-st test` 已能通过

---

## 总体评价

这次 CK 替换从方案到代码落地都比较扎实：
- 替换边界清楚
- API 适配基本完整
- 并发结构的实现思路和原使用场景匹配
- 构建系统也已经联动更新

**建议在提交前优先修掉 `common/tglog_stack.h:35` 的 16 字节对齐问题。**

修完这一点后，这套替换方案整体上就是可提交状态了。
