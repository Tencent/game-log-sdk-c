/*
 * tglog_atomic.h - Cross-platform atomic operations (C99)
 *
 * GCC/Clang: __sync_* builtins
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
#define tglog_pause() _mm_pause()
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

#if defined(_MSC_VER)
/* ======================================================================
 * MSVC: _InterlockedXxx intrinsics
 * ====================================================================== */

#include <windows.h>

#pragma intrinsic(_InterlockedExchange)
#pragma intrinsic(_InterlockedCompareExchange)
#pragma intrinsic(_InterlockedExchangeAdd)
#pragma intrinsic(_InterlockedExchangeAdd64)
#pragma intrinsic(_InterlockedIncrement64)
#pragma intrinsic(_InterlockedOr64)
#pragma intrinsic(_InterlockedExchange64)
#pragma intrinsic(_InterlockedCompareExchangePointer)

/* 32-bit atomic load (acquire semantics) */
static __forceinline uint32_t tglog_atomic_load_32(const uint32_t *p) {
    uint32_t v = *((volatile uint32_t *)p);
    _ReadBarrier();
    return v;
}

/* 32-bit atomic store (release semantics) */
static __forceinline void tglog_atomic_store_32(uint32_t *p, uint32_t v) {
    _WriteBarrier();
    *((volatile uint32_t *)p) = v;
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

/* 64-bit atomic store */
static __forceinline void tglog_atomic_store_64(uint64_t *p, uint64_t v) {
    _InterlockedExchange64((volatile __int64 *)p, (__int64)v);
}

/* 64-bit atomic load (relaxed, but guaranteed atomic on x86-64) */
static __forceinline uint64_t tglog_atomic_load_64(const uint64_t *p) {
    return (uint64_t)_InterlockedOr64((volatile __int64 *)p, 0);
}

/* unsigned int atomic load (acquire semantics) */
static __forceinline unsigned int tglog_atomic_load_uint(const unsigned int *p) {
    unsigned int v = *((volatile unsigned int *)p);
    _ReadBarrier();
    return v;
}

/* unsigned int atomic store (release semantics) */
static __forceinline void tglog_atomic_store_uint(unsigned int *p, unsigned int v) {
    _WriteBarrier();
    *((volatile unsigned int *)p) = v;
}

/* unsigned int atomic CAS, returns 1 on success */
static __forceinline int tglog_atomic_cas_uint(unsigned int *p, unsigned int expected, unsigned int desired) {
    return _InterlockedCompareExchange((volatile long *)p,
                                       (long)desired,
                                       (long)expected) == (long)expected;
}

/* Pointer atomic load */
static __forceinline void *tglog_atomic_load_ptr(void *volatile *p) {
    void *v = *p;
    _ReadBarrier();
    return v;
}

/* Pointer atomic store */
static __forceinline void tglog_atomic_store_ptr(void *volatile *p, void *v) {
    _WriteBarrier();
    *p = v;
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

/* Memory barriers */
#define tglog_fence_store()   do { _WriteBarrier(); MemoryBarrier(); } while (0)
#define tglog_fence_load()    do { _ReadBarrier(); MemoryBarrier(); } while (0)
#define tglog_fence_memory()  MemoryBarrier()

#elif defined(__GNUC__) || defined(__clang__)
/* ======================================================================
 * GCC/Clang: __sync_* builtins (C99 compatible)
 * ====================================================================== */

/* 32-bit atomic load (acquire semantics) */
static inline uint32_t tglog_atomic_load_32(const uint32_t *p) {
    uint32_t v = *(volatile uint32_t *)p;
    __sync_synchronize();
    return v;
}

/* 32-bit atomic store (release semantics) */
static inline void tglog_atomic_store_32(uint32_t *p, uint32_t v) {
    __sync_synchronize();
    *(volatile uint32_t *)p = v;
}

/* 32-bit atomic CAS, returns 1 on success */
static inline int tglog_atomic_cas_32(uint32_t *p, uint32_t expected, uint32_t desired) {
    return __sync_bool_compare_and_swap(p, expected, desired);
}

/* 32-bit atomic add (no return value) */
static inline void tglog_atomic_add_32(uint32_t *p, uint32_t v) {
    __sync_fetch_and_add(p, v);
}

/* 64-bit atomic fetch-and-add, returns old value */
static inline uint64_t tglog_atomic_faa_64(uint64_t *p, uint64_t v) {
    return __sync_fetch_and_add(p, v);
}

/* 64-bit atomic increment */
static inline void tglog_atomic_inc_64(uint64_t *p) {
    __sync_fetch_and_add(p, (uint64_t)1);
}

/* 64-bit atomic add (no return value) */
static inline void tglog_atomic_add_64(uint64_t *p, uint64_t v) {
    __sync_fetch_and_add(p, v);
}

/* 64-bit atomic store */
static inline void tglog_atomic_store_64(uint64_t *p, uint64_t v) {
    __sync_synchronize();
    *(volatile uint64_t *)p = v;
}

/* 64-bit atomic load */
static inline uint64_t tglog_atomic_load_64(const uint64_t *p) {
    return __sync_fetch_and_add((uint64_t *)(uintptr_t)p, (uint64_t)0);
}

/* unsigned int atomic load */
static inline unsigned int tglog_atomic_load_uint(const unsigned int *p) {
    unsigned int v = *(volatile unsigned int *)p;
    __sync_synchronize();
    return v;
}

/* unsigned int atomic store */
static inline void tglog_atomic_store_uint(unsigned int *p, unsigned int v) {
    __sync_synchronize();
    *(volatile unsigned int *)p = v;
}

/* unsigned int atomic CAS, returns 1 on success */
static inline int tglog_atomic_cas_uint(unsigned int *p, unsigned int expected, unsigned int desired) {
    return __sync_bool_compare_and_swap(p, expected, desired);
}

/* Pointer atomic load */
static inline void *tglog_atomic_load_ptr(void *volatile *p) {
    void *v = *(void *volatile *)p;
    __sync_synchronize();
    return v;
}

/* Pointer atomic store */
static inline void tglog_atomic_store_ptr(void *volatile *p, void *v) {
    __sync_synchronize();
    *(void *volatile *)p = v;
}

/* Pointer atomic CAS, returns 1 on success */
static inline int tglog_atomic_cas_ptr(void *volatile *p, void *expected, void *desired) {
    return __sync_bool_compare_and_swap(p, expected, desired);
}

/* Pointer atomic CAS with value output, returns 1 on success */
static inline int tglog_atomic_cas_ptr_value(void *volatile *p, void *expected, void *desired, void **old) {
    void *prev = __sync_val_compare_and_swap(p, expected, desired);
    *old = prev;
    return prev == expected;
}

/* Memory barriers */
#define tglog_fence_store()   __sync_synchronize()
#define tglog_fence_load()    __sync_synchronize()
#define tglog_fence_memory()  __sync_synchronize()

#else
#error "Unsupported compiler: need GCC/Clang __sync builtins or MSVC intrinsics"
#endif

#endif /* TGLOG_ATOMIC_H */
