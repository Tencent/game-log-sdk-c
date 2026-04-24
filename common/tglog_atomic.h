/*
 * tglog_atomic.h - Cross-platform atomic operations with a C99-compatible API
 *
 * This header keeps the public interface usable from C99 code, but the
 * implementation relies on compiler-specific atomic primitives rather than
 * any C99 standard atomic facility.
 *
 * GCC/Clang: __atomic_* builtins
 * MSVC: _InterlockedXxx intrinsics
 */

#ifndef TGLOG_ATOMIC_H
#define TGLOG_ATOMIC_H

#include <stdint.h>

/* ======================================================================
 * Cross-platform pause hint for spin-wait loops
 * ====================================================================== */

#if defined(_MSC_VER)
#include <intrin.h>
#if defined(_M_ARM64) || defined(_M_ARM)
#define tglog_pause() __yield()
#else
#define tglog_pause() _mm_pause()
#endif
#elif defined(__GNUC__) || defined(__clang__)
#if defined(__x86_64__) || defined(__i386__)
#define tglog_pause() __builtin_ia32_pause()
#elif defined(__aarch64__)
#define tglog_pause() __asm__ volatile("yield" ::: "memory")
#else
#define tglog_pause() ((void)0)
#endif
#else
#define tglog_pause() ((void)0)
#endif

/* ======================================================================
 * Cross-platform thread yield for back-off in spin loops
 * ====================================================================== */

#if defined(_MSC_VER)
/* windows.h is included early so that SwitchToThread() is available
 * for tglog_yield() below, and for the _Interlocked* intrinsics. */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
/* SwitchToThread() is lighter than Sleep(0): it only yields to threads on
 * the same processor, and returns immediately if there are none. */
#define tglog_yield() SwitchToThread()
#elif defined(__GNUC__) || defined(__clang__)
#include <sched.h>
#define tglog_yield() sched_yield()
#else
#define tglog_yield() ((void)0)
#endif

#if defined(_MSC_VER)
/* ======================================================================
 * MSVC: _InterlockedXxx intrinsics
 *
 * On x86/x64, the hardware provides Total Store Ordering (TSO), so
 * volatile reads/writes + compiler barriers (_ReadBarrier/_WriteBarrier)
 * are sufficient for acquire/release semantics.
 *
 * On ARM/ARM64, the memory model is weak — compiler barriers alone do NOT
 * emit hardware barrier instructions (dmb/ldar/stlr). We must use
 * _Interlocked* intrinsics for all atomic loads and stores, as MSVC
 * generates the correct acquire/release instructions for ARM targets.
 * ====================================================================== */

#pragma intrinsic(_InterlockedExchange)
#pragma intrinsic(_InterlockedCompareExchange)
#pragma intrinsic(_InterlockedExchangeAdd)
#pragma intrinsic(_InterlockedExchangeAdd64)
#pragma intrinsic(_InterlockedIncrement64)
#pragma intrinsic(_InterlockedOr64)
#pragma intrinsic(_InterlockedExchange64)
#pragma intrinsic(_InterlockedCompareExchangePointer)

/* ----------------------------------------------------------------------
 * 32-bit atomics
 * ---------------------------------------------------------------------- */

/* 32-bit atomic load (acquire semantics) */
static __forceinline uint32_t tglog_atomic_load_32(const uint32_t *p) {
#if defined(_M_ARM64) || defined(_M_ARM)
    /* _InterlockedOr_acq(p, 0) performs: tmp = *p | 0 (== *p), store back,
     * return old value — with acquire semantics. The write is idempotent
     * (OR with 0 doesn't change the value), so this is safe as a load. */
    return (uint32_t)_InterlockedOr_acq((volatile long *)p, 0);
#else
    uint32_t v = *((volatile uint32_t *)p);
    _ReadBarrier();
    return v;
#endif
}

/* 32-bit atomic store (release semantics) */
static __forceinline void tglog_atomic_store_32(uint32_t *p, uint32_t v) {
#if defined(_M_ARM64) || defined(_M_ARM)
    _InterlockedExchange_rel((volatile long *)p, (long)v);
#else
    _WriteBarrier();
    *((volatile uint32_t *)p) = v;
#endif
}

/* 32-bit atomic CAS, returns 1 on success */
static __forceinline int tglog_atomic_cas_32(uint32_t *p, uint32_t expected, uint32_t desired) {
    return _InterlockedCompareExchange((volatile long *)p,
                                       (long)desired,
                                       (long)expected) == (long)expected;
}

/* 32-bit atomic add (no return value) */
static __forceinline void tglog_atomic_add_32(uint32_t *p, uint32_t v) {
    _InterlockedExchangeAdd((volatile long *)p, (long)v);
}

/* ----------------------------------------------------------------------
 * 64-bit atomics
 * ---------------------------------------------------------------------- */

/* 64-bit atomic fetch-and-add, returns old value */
static __forceinline uint64_t tglog_atomic_faa_64(uint64_t *p, uint64_t v) {
    return (uint64_t)_InterlockedExchangeAdd64((volatile __int64 *)p, (__int64)v);
}

/* 64-bit atomic increment */
static __forceinline void tglog_atomic_inc_64(uint64_t *p) {
    _InterlockedIncrement64((volatile __int64 *)p);
}

/* 64-bit atomic add (no return value) */
static __forceinline void tglog_atomic_add_64(uint64_t *p, uint64_t v) {
    _InterlockedExchangeAdd64((volatile __int64 *)p, (__int64)v);
}

/* 64-bit atomic store (release semantics) */
static __forceinline void tglog_atomic_store_64(uint64_t *p, uint64_t v) {
#if defined(_M_ARM64) || defined(_M_ARM)
    _InterlockedExchange64_rel((volatile __int64 *)p, (__int64)v);
#else
    _InterlockedExchange64((volatile __int64 *)p, (__int64)v);
#endif
}

/* 64-bit atomic load (acquire semantics) */
static __forceinline uint64_t tglog_atomic_load_64(const uint64_t *p) {
#if defined(_M_ARM64) || defined(_M_ARM)
    return (uint64_t)_InterlockedOr64_acq((volatile __int64 *)p, 0);
#else
    /* On x86-64, aligned 64-bit loads are naturally atomic; volatile read +
     * compiler barrier is sufficient and avoids the write side-effect of
     * _InterlockedOr64, which would break on truly const/read-only memory. */
    uint64_t v = *((volatile uint64_t *)p);
    _ReadBarrier();
    return v;
#endif
}

/* ----------------------------------------------------------------------
 * unsigned int atomics
 * ---------------------------------------------------------------------- */

/* unsigned int atomic load (acquire semantics) */
static __forceinline unsigned int tglog_atomic_load_uint(const unsigned int *p) {
#if defined(_M_ARM64) || defined(_M_ARM)
    return (unsigned int)_InterlockedOr_acq((volatile long *)p, 0);
#else
    unsigned int v = *((volatile unsigned int *)p);
    _ReadBarrier();
    return v;
#endif
}

/* unsigned int atomic store (release semantics) */
static __forceinline void tglog_atomic_store_uint(unsigned int *p, unsigned int v) {
#if defined(_M_ARM64) || defined(_M_ARM)
    _InterlockedExchange_rel((volatile long *)p, (long)v);
#else
    _WriteBarrier();
    *((volatile unsigned int *)p) = v;
#endif
}

/* unsigned int atomic CAS, returns 1 on success */
static __forceinline int tglog_atomic_cas_uint(unsigned int *p, unsigned int expected, unsigned int desired) {
    return _InterlockedCompareExchange((volatile long *)p,
                                       (long)desired,
                                       (long)expected) == (long)expected;
}

/* ----------------------------------------------------------------------
 * Pointer atomics
 * ---------------------------------------------------------------------- */

/* Pointer atomic load (acquire semantics) */
static __forceinline void *tglog_atomic_load_ptr(void *volatile *p) {
#if defined(_M_ARM64) || defined(_M_ARM)
    return _InterlockedCompareExchangePointer_acq(p, NULL, NULL);
#else
    void *v = *p;
    _ReadBarrier();
    return v;
#endif
}

/* Pointer atomic store (release semantics) */
static __forceinline void tglog_atomic_store_ptr(void *volatile *p, void *v) {
#if defined(_M_ARM64) || defined(_M_ARM)
    _InterlockedExchangePointer_rel(p, v);
#else
    _WriteBarrier();
    *p = v;
#endif
}

/* Pointer atomic CAS, returns 1 on success */
static __forceinline int tglog_atomic_cas_ptr(void *volatile *p, void *expected, void *desired) {
    return _InterlockedCompareExchangePointer(p, desired, expected) == expected;
}

/* Pointer atomic CAS with value output, returns 1 on success */
static __forceinline int tglog_atomic_cas_ptr_value(void *volatile *p, void *expected, void *desired, void **old) {
    void *prev = _InterlockedCompareExchangePointer(p, desired, expected);
    *old = prev;
    return prev == expected;
}

/* ----------------------------------------------------------------------
 * Memory barriers
 * ---------------------------------------------------------------------- */
#define tglog_fence_store()   MemoryBarrier()
#define tglog_fence_load()    MemoryBarrier()
#define tglog_fence_memory()  MemoryBarrier()

#elif defined(__GNUC__) || defined(__clang__)
/* ======================================================================
 * GCC/Clang: __sync_* / __atomic_* builtins
 * These are compiler extensions used to provide a C99-compatible API.
 * ====================================================================== */

/* 32-bit atomic load (acquire semantics) */
static inline uint32_t tglog_atomic_load_32(const uint32_t *p) {
    return __atomic_load_n(p, __ATOMIC_ACQUIRE);
}

/* 32-bit atomic store (release semantics) */
static inline void tglog_atomic_store_32(uint32_t *p, uint32_t v) {
    __atomic_store_n(p, v, __ATOMIC_RELEASE);
}

/* 32-bit atomic CAS, returns 1 on success */
static inline int tglog_atomic_cas_32(uint32_t *p, uint32_t expected, uint32_t desired) {
    return __atomic_compare_exchange_n(p, &expected, desired, 0,
                                       __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

/* 32-bit atomic add (no return value) */
static inline void tglog_atomic_add_32(uint32_t *p, uint32_t v) {
    (void)__atomic_fetch_add(p, v, __ATOMIC_SEQ_CST);
}

/* 64-bit atomic fetch-and-add, returns old value */
static inline uint64_t tglog_atomic_faa_64(uint64_t *p, uint64_t v) {
    return __atomic_fetch_add(p, v, __ATOMIC_SEQ_CST);
}

/* 64-bit atomic increment */
static inline void tglog_atomic_inc_64(uint64_t *p) {
    (void)__atomic_fetch_add(p, (uint64_t)1, __ATOMIC_SEQ_CST);
}

/* 64-bit atomic add (no return value) */
static inline void tglog_atomic_add_64(uint64_t *p, uint64_t v) {
    (void)__atomic_fetch_add(p, v, __ATOMIC_SEQ_CST);
}

/* 64-bit atomic store (release semantics).
 * On x86-64: plain aligned 64-bit write is atomic; __atomic_store_n emits
 * it directly.
 * On 32-bit x86: GCC/Clang lower __atomic_store_n(uint64_t) to either
 * SSE2 movq or a lock cmpxchg8b — both are atomic and don't require
 * libatomic. The previous CAS-based read-then-swap loop burned a locked
 * bus cycle on every call unnecessarily. */
static inline void tglog_atomic_store_64(uint64_t *p, uint64_t v) {
    __atomic_store_n(p, v, __ATOMIC_RELEASE);
}

/* 64-bit atomic load (acquire semantics).
 * Use __atomic_load_n to avoid the RMW side effects of fetch-and-add(0)
 * and preserve const-correctness for the source pointer. */
static inline uint64_t tglog_atomic_load_64(const uint64_t *p) {
    return __atomic_load_n(p, __ATOMIC_ACQUIRE);
}

/* unsigned int atomic load (acquire semantics) */
static inline unsigned int tglog_atomic_load_uint(const unsigned int *p) {
    return __atomic_load_n(p, __ATOMIC_ACQUIRE);
}

/* unsigned int atomic store (release semantics) */
static inline void tglog_atomic_store_uint(unsigned int *p, unsigned int v) {
    __atomic_store_n(p, v, __ATOMIC_RELEASE);
}

/* unsigned int atomic CAS, returns 1 on success */
static inline int tglog_atomic_cas_uint(unsigned int *p, unsigned int expected, unsigned int desired) {
    return __atomic_compare_exchange_n(p, &expected, desired, 0,
                                       __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

/* Pointer atomic load (acquire semantics) */
static inline void *tglog_atomic_load_ptr(void *volatile *p) {
    return __atomic_load_n(p, __ATOMIC_ACQUIRE);
}

/* Pointer atomic store (release semantics) */
static inline void tglog_atomic_store_ptr(void *volatile *p, void *v) {
    __atomic_store_n(p, v, __ATOMIC_RELEASE);
}

/* Pointer atomic CAS, returns 1 on success */
static inline int tglog_atomic_cas_ptr(void *volatile *p, void *expected, void *desired) {
    return __atomic_compare_exchange_n(p, &expected, desired, 0,
                                       __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

/* Pointer atomic CAS with value output, returns 1 on success */
static inline int tglog_atomic_cas_ptr_value(void *volatile *p, void *expected, void *desired, void **old) {
    void *prev = expected;
    int success = __atomic_compare_exchange_n(p, &prev, desired, 0,
                                              __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    *old = prev;
    return success;
}

/* Memory barriers */
#define tglog_fence_store()   __atomic_thread_fence(__ATOMIC_RELEASE)
#define tglog_fence_load()    __atomic_thread_fence(__ATOMIC_ACQUIRE)
#define tglog_fence_memory()  __atomic_thread_fence(__ATOMIC_SEQ_CST)

#else
#error "Unsupported compiler: need GCC/Clang __sync builtins or MSVC intrinsics"
#endif

#endif /* TGLOG_ATOMIC_H */
