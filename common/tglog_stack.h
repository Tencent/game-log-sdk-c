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
 *
 * =========================================================================
 * MEMORY RECLAMATION CONSTRAINT (MPMC mode):
 *
 * Nodes managed by MPMC operations MUST NOT be freed with free() while any
 * concurrent push/pop/traversal may be in progress.  Between loading a node
 * pointer and completing CAS, another thread may still dereference that
 * node's `next` field.  If the node's memory has been returned to the OS,
 * this causes a segfault.
 *
 * Safe usage patterns:
 *   - Memory pool / arena: nodes are returned to a pool whose backing
 *     memory is never unmapped until the pool itself is destroyed.
 *   - Epoch-based reclamation or hazard pointers (not provided here).
 *
 * Unsafe:
 *   - Calling free() on a popped node while other threads are still
 *     operating on the stack.
 * =========================================================================
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

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
/* MSVC 64-bit (x86-64 / ARM64): _InterlockedCompareExchange128 */

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

    if (__atomic_compare_exchange_n(
            (_tglog_dw_t *)target,
            &expected.dw,
            desired.dw,
            0,                  /* strong CAS */
            __ATOMIC_ACQ_REL,   /* success: acquire + release */
            __ATOMIC_ACQUIRE))  /* failure: acquire only */
        return 1;

    *out_head = expected.s.head;
    *out_gen = expected.s.generation;
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

    if (__atomic_compare_exchange_n(
            (uint64_t *)target,
            &expected.dw,
            desired.dw,
            0,                  /* strong CAS */
            __ATOMIC_ACQ_REL,   /* success: acquire + release */
            __ATOMIC_ACQUIRE))  /* failure: acquire only */
        return 1;

    *out_head = expected.s.head;
    *out_gen = expected.s.generation;
    return 0;
}

#endif /* __SIZEOF_POINTER__ == 8 */

#elif defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_ARM))
/* MSVC 32-bit (x86 / ARM): use _InterlockedCompareExchange64 */

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
 * Atomic pair load — reads {head, generation} as a consistent snapshot.
 * Uses the same double-width technique as CAS2 to avoid torn reads.
 * ====================================================================== */

static inline void _tglog_stack_load_pair(struct tglog_stack *target,
    struct tglog_stack_entry **out_head, uintptr_t *out_gen) {
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
    /* On MSVC x64/ARM64, use a CAS2 that is guaranteed to fail and returns
     * the current value in the comparand. We compare against an impossible
     * pair (NULL, ~0) so it always fails and gives us the real values. */
    __int64 comparand[2];
    comparand[0] = 0;
    comparand[1] = (__int64)(~(uintptr_t)0);
    _InterlockedCompareExchange128((volatile __int64 *)target,
                                   comparand[1], comparand[0], comparand);
    *out_head = (struct tglog_stack_entry *)comparand[0];
    *out_gen = (uintptr_t)comparand[1];
#elif (defined(__GNUC__) || defined(__clang__)) && __SIZEOF_POINTER__ == 8
    /* Cannot use __atomic_load_n for 128-bit on x86-64 (requires libatomic).
     * Use a CAS that always yields the current value: if the stack is {NULL,0}
     * the CAS succeeds as a no-op; otherwise it fails and expected is filled
     * with the actual value. Either way, expected holds the current snapshot. */
    _tglog_stack_pair_t expected;
    _tglog_stack_pair_t desired;
    expected.s.head = NULL;
    expected.s.generation = 0;
    desired = expected;
    __atomic_compare_exchange_n((_tglog_dw_t *)target,
                                &expected.dw, desired.dw,
                                1,              /* weak CAS is fine here */
                                __ATOMIC_ACQUIRE,
                                __ATOMIC_ACQUIRE);
    *out_head = expected.s.head;
    *out_gen = expected.s.generation;
#elif (defined(__GNUC__) || defined(__clang__)) && __SIZEOF_POINTER__ == 4
    _tglog_stack_pair_t expected;
    _tglog_stack_pair_t desired;
    expected.s.head = NULL;
    expected.s.generation = 0;
    desired = expected;
    __atomic_compare_exchange_n((uint64_t *)target,
                                &expected.dw, desired.dw,
                                1,
                                __ATOMIC_ACQUIRE,
                                __ATOMIC_ACQUIRE);
    *out_head = expected.s.head;
    *out_gen = expected.s.generation;
#elif defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_ARM))
    /* MSVC 32-bit: use a failing CAS64 to atomically read the pair */
    __int64 expected = 0;
    __int64 result = _InterlockedCompareExchange64((volatile __int64 *)target,
                                                    expected, expected);
    *out_head = (struct tglog_stack_entry *)(uintptr_t)(result & 0xFFFFFFFF);
    *out_gen = (uintptr_t)(result >> 32);
#else
#error "Unsupported platform for atomic pair load"
#endif
}

/* ======================================================================
 * MPMC (Multi-Producer Multi-Consumer) operations
 *
 * Push uses a single-width CAS on `head` only — ABA is not a concern for
 * push (we never reuse a popped pointer mid-push), so the generation
 * counter is left untouched and need not be sampled.
 *
 * Pop uses a double-width CAS on {head, generation} for ABA protection,
 * matching ck_stack_pop_mpmc's tagged-pointer scheme.
 *
 * Coexistence is safe: any concurrent push that changes `head` will be
 * detected by pop's CAS2 (since the head half of the pair changes), and
 * pop is the sole writer of `generation`, so push never observes a torn
 * generation value.
 * ====================================================================== */

/*
 * MPMC push: single-width CAS loop on `head` only.
 *
 * Mirrors ck_stack_push_upmc — push doesn't suffer from ABA, so we avoid
 * the cmpxchg16b microcode penalty entirely on the hot path.
 */
static inline void tglog_stack_push_mpmc(struct tglog_stack *target,
    struct tglog_stack_entry *entry) {
    struct tglog_stack_entry *head;

    head = (struct tglog_stack_entry *)tglog_atomic_load_ptr(
        (void *volatile *)&target->head);
    entry->next = head;

    while (tglog_atomic_cas_ptr_value((void *volatile *)&target->head,
                                       head, entry,
                                       (void **)&head) == 0) {
        entry->next = head;
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
 *
 * Snapshot strategy: we read `head` and `generation` with two separate
 * single-width loads (head first), instead of an expensive double-width
 * read. If the pair is torn, the CAS2 will simply fail and hand back the
 * real {head, generation} via its out parameters, which we reuse on the
 * next iteration — same approach as ck_stack_pop_mpmc.
 */
static inline struct tglog_stack_entry *tglog_stack_pop_mpmc(struct tglog_stack *target) {
    struct tglog_stack_entry *head;
    uintptr_t gen;
    struct tglog_stack_entry *next;

    head = (struct tglog_stack_entry *)tglog_atomic_load_ptr(
        (void *volatile *)&target->head);
    gen = (uintptr_t)tglog_atomic_load_ptr((void *volatile *)&target->generation);

    if (head == NULL)
        return NULL;

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
