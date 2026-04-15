/*
 * tglog_stack.h - Cross-platform lock-free stack with a C99-compatible API
 *
 * This header is header-only and keeps the public interface usable from C99
 * code, but its concurrent semantics rely on tglog_atomic.h, which is backed
 * by compiler-specific atomic primitives rather than any C99 standard atomic
 * facility.
 *
 * Supports:
 * - MPMC (multi-producer, multi-consumer) with ABA protection
 * - SPNC (single-producer, no consumer) / NPSC (no producer, single-consumer)
 *
 * MPMC uses tagged pointer (generation counter) for ABA protection,
 * implemented via double-width CAS (128-bit on 64-bit, 64-bit on 32-bit).
 */

#ifndef TGLOG_STACK_H
#define TGLOG_STACK_H

#include <stddef.h>

#include "tglog_atomic.h"

/*
 * Stack entry - embedded in the user's structure.
 */
struct tglog_stack_entry {
    struct tglog_stack_entry *next;
};
typedef struct tglog_stack_entry tglog_stack_entry_t;

/*
 * Stack structure with generation counter for ABA protection.
 *
 * On 64-bit platforms:
 *   head (8 bytes) + generation (8 bytes) = 16 bytes, using 128-bit CAS.
 *   Must be 16-byte aligned for cmpxchg16b / _InterlockedCompareExchange128.
 * On 32-bit platforms:
 *   head (4 bytes) + generation (4 bytes) = 8 bytes, using 64-bit CAS.
 *   Must be 8-byte aligned for cmpxchg8b.
 */
#if defined(_MSC_VER)
__declspec(align(16))
#endif
struct tglog_stack {
    struct tglog_stack_entry *head;
    uintptr_t generation;
}
#if (defined(__GNUC__) || defined(__clang__)) && __SIZEOF_POINTER__ == 8
__attribute__((aligned(16)))
#elif (defined(__GNUC__) || defined(__clang__)) && __SIZEOF_POINTER__ == 4
__attribute__((aligned(8)))
#endif
;
typedef struct tglog_stack tglog_stack_t;

#define TGLOG_STACK_INITIALIZER { NULL, 0 }

/* ======================================================================
 * Double-width CAS for ABA protection
 * ====================================================================== */

#if defined(_MSC_VER) && defined(_M_X64)
/* MSVC x86-64: _InterlockedCompareExchange128 */

#include <intrin.h>

/*
 * Double-width CAS: compare {target->head, target->generation} against
 * {old_head, old_gen}, if equal replace with {new_head, new_gen}.
 * On failure, *out_head and *out_gen are set to the current values.
 * Returns 1 on success, 0 on failure.
 */
static __forceinline int _tglog_stack_cas2(struct tglog_stack *target,
    struct tglog_stack_entry *old_head, uintptr_t old_gen,
    struct tglog_stack_entry *new_head, uintptr_t new_gen,
    struct tglog_stack_entry **out_head, uintptr_t *out_gen) {
    /* _InterlockedCompareExchange128(dest, high, low, comparand)
     * comparand is in/out: [0]=low, [1]=high
     * dest: [0]=low(head), [1]=high(generation)
     */
    __int64 comparand[2];
    comparand[0] = (__int64)old_head;
    comparand[1] = (__int64)old_gen;

    unsigned char result = _InterlockedCompareExchange128(
        (volatile __int64 *)target,
        (__int64)new_gen,
        (__int64)new_head,
        comparand);

    if (!result) {
        *out_head = (struct tglog_stack_entry *)comparand[0];
        *out_gen = (uintptr_t)comparand[1];
    }
    return result;
}

#elif defined(__GNUC__) || defined(__clang__)

#if __SIZEOF_POINTER__ == 8
/* GCC/Clang 64-bit: use __int128 CAS */

typedef unsigned __int128 _tglog_dw_t;

typedef union {
    _tglog_dw_t dw;
    struct {
        struct tglog_stack_entry *head;
        uintptr_t generation;
    } s;
} _tglog_stack_pair_t;

static inline int _tglog_stack_cas2(struct tglog_stack *target,
    struct tglog_stack_entry *old_head, uintptr_t old_gen,
    struct tglog_stack_entry *new_head, uintptr_t new_gen,
    struct tglog_stack_entry **out_head, uintptr_t *out_gen) {
    _tglog_stack_pair_t expected, desired;
    expected.s.head = old_head;
    expected.s.generation = old_gen;
    desired.s.head = new_head;
    desired.s.generation = new_gen;

    _tglog_dw_t old_val = __sync_val_compare_and_swap(
        (_tglog_dw_t *)target,
        expected.dw,
        desired.dw);

    if (old_val == expected.dw)
        return 1;

    _tglog_stack_pair_t result;
    result.dw = old_val;
    *out_head = result.s.head;
    *out_gen = result.s.generation;
    return 0;
}

#else
/* GCC/Clang 32-bit: use uint64_t CAS */

typedef union {
    uint64_t dw;
    struct {
        struct tglog_stack_entry *head;
        uintptr_t generation;
    } s;
} _tglog_stack_pair_t;

static inline int _tglog_stack_cas2(struct tglog_stack *target,
    struct tglog_stack_entry *old_head, uintptr_t old_gen,
    struct tglog_stack_entry *new_head, uintptr_t new_gen,
    struct tglog_stack_entry **out_head, uintptr_t *out_gen) {
    _tglog_stack_pair_t expected, desired;
    expected.s.head = old_head;
    expected.s.generation = old_gen;
    desired.s.head = new_head;
    desired.s.generation = new_gen;

    uint64_t old_val = __sync_val_compare_and_swap(
        (uint64_t *)target,
        expected.dw,
        desired.dw);

    if (old_val == expected.dw)
        return 1;

    _tglog_stack_pair_t result;
    result.dw = old_val;
    *out_head = result.s.head;
    *out_gen = result.s.generation;
    return 0;
}

#endif /* __SIZEOF_POINTER__ == 8 */

#elif defined(_MSC_VER) && defined(_M_IX86)
/* MSVC 32-bit: use _InterlockedCompareExchange64 */

static __forceinline int _tglog_stack_cas2(struct tglog_stack *target,
    struct tglog_stack_entry *old_head, uintptr_t old_gen,
    struct tglog_stack_entry *new_head, uintptr_t new_gen,
    struct tglog_stack_entry **out_head, uintptr_t *out_gen) {
    __int64 expected = ((__int64)(uintptr_t)old_gen << 32) | (uintptr_t)old_head;
    __int64 desired = ((__int64)(uintptr_t)new_gen << 32) | (uintptr_t)new_head;

    __int64 result = _InterlockedCompareExchange64((volatile __int64 *)target,
                                                    desired, expected);
    if (result == expected)
        return 1;

    *out_head = (struct tglog_stack_entry *)(uintptr_t)(result & 0xFFFFFFFF);
    *out_gen = (uintptr_t)(result >> 32);
    return 0;
}

#else
#error "Unsupported platform for double-width CAS"
#endif

/* ======================================================================
 * Stack initialization
 * ====================================================================== */

static inline void tglog_stack_init(struct tglog_stack *stack) {
    stack->head = NULL;
    stack->generation = 0;
}

/* ======================================================================
 * MPMC (Multi-Producer Multi-Consumer) operations
 * ====================================================================== */

/*
 * MPMC push: CAS loop on head pointer.
 * ABA is not a concern for push (only for pop).
 */
static inline void tglog_stack_push_mpmc(struct tglog_stack *target,
    struct tglog_stack_entry *entry) {
    struct tglog_stack_entry *stack;

    stack = (struct tglog_stack_entry *)tglog_atomic_load_ptr((void *volatile *)&target->head);
    entry->next = stack;
    tglog_fence_store();

    while (tglog_atomic_cas_ptr_value((void *volatile *)&target->head,
                                       stack, entry,
                                       (void **)&stack) == 0) {
        entry->next = stack;
        tglog_fence_store();
    }
}

/*
 * MPMC pop: double-width CAS with generation counter for ABA protection.
 * Returns NULL if stack is empty.
 *
 * IMPORTANT — memory reclamation constraint:
 * Between loading `head` and the successful CAS, another thread may pop the
 * same node and free it.  The dereference `head->next` would then be a
 * use-after-free.  The generation counter prevents the CAS from succeeding on
 * a recycled pointer (ABA), but it does NOT prevent the speculative read of
 * freed memory.
 *
 * This is safe ONLY when popped nodes are returned to a memory pool (or arena)
 * whose backing memory is never unmapped / returned to the OS.  In that case
 * the read of `head->next` may fetch stale data, but cannot fault; the CAS
 * will simply fail and retry with a fresh snapshot.
 *
 * Do NOT use standard free() on nodes that may still be referenced by
 * concurrent pop operations — doing so risks a segfault.
 */
static inline struct tglog_stack_entry *tglog_stack_pop_mpmc(struct tglog_stack *target) {
    struct tglog_stack_entry *head;
    uintptr_t gen;
    struct tglog_stack_entry *next;

    /* Load generation first, then head */
    gen = *(volatile uintptr_t *)&target->generation;
    tglog_fence_load();
    head = (struct tglog_stack_entry *)tglog_atomic_load_ptr((void *volatile *)&target->head);

    if (head == NULL)
        return NULL;

    /* Order with respect to next pointer dereference */
    tglog_fence_load();
    next = head->next;

    while (_tglog_stack_cas2(target, head, gen, next, gen + 1,
                              &head, &gen) == 0) {
        if (head == NULL)
            return NULL;

        tglog_fence_load();
        next = head->next;
    }

    return head;
}

/* ======================================================================
 * Single-threaded operations (no atomics needed)
 * ====================================================================== */

/* SPNC push: single producer, no concurrent consumers. */
static inline void tglog_stack_push_spnc(struct tglog_stack *target,
    struct tglog_stack_entry *entry) {
    entry->next = target->head;
    target->head = entry;
}

/* NPSC pop: no concurrent producers, single consumer. Returns NULL if empty. */
static inline struct tglog_stack_entry *tglog_stack_pop_npsc(struct tglog_stack *target) {
    struct tglog_stack_entry *n;

    if (target->head == NULL)
        return NULL;

    n = target->head;
    target->head = n->next;
    return n;
}

/* ======================================================================
 * Utility macros
 * ====================================================================== */

/*
 * TGLOG_STACK_CONTAINER - Generate a container_of function.
 *
 * Usage:
 *   TGLOG_STACK_CONTAINER(tglog_payload, stack_entry, entry_to_payload)
 *
 * Generates:
 *   static inline tglog_payload *entry_to_payload(tglog_stack_entry_t *entry)
 */
#define TGLOG_STACK_CONTAINER(T, M, N)                                        \
    static inline T *N(tglog_stack_entry_t *entry) {                          \
        return (T *)((char *)entry - offsetof(T, M));                         \
    }

#define TGLOG_STACK_ISEMPTY(m) ((m)->head == NULL)
#define TGLOG_STACK_FIRST(s)   ((s)->head)
#define TGLOG_STACK_NEXT(m)    ((m)->next)

#define TGLOG_STACK_FOREACH(stack, entry)                                     \
    for ((entry) = TGLOG_STACK_FIRST(stack);                                  \
         (entry) != NULL;                                                     \
         (entry) = TGLOG_STACK_NEXT(entry))

#endif /* TGLOG_STACK_H */
