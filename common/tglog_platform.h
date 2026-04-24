/*
 * tglog_platform.h - Cross-platform compatibility layer
 *
 * Provides POSIX-to-Windows shims for:
 *   ssize_t, struct iovec, writev(), gettimeofday(),
 *   localtime_r(), rand_r(), strdup, MSG_NOSIGNAL,
 *   errno mappings, socket error formatting,
 *   usleep(), sleep(), getopt(), getopt_long()
 */

#ifndef TGLOG_PLATFORM_H
#define TGLOG_PLATFORM_H

/* === Compiler-portable inline / TLS macros === */
#if defined(_MSC_VER)
    #define TGLOG_INLINE       static __forceinline
    #define TGLOG_THREAD_LOCAL __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
    #define TGLOG_INLINE       static inline __attribute__((always_inline))
    #define TGLOG_THREAD_LOCAL __thread
#else
    #define TGLOG_INLINE       static inline
    #define TGLOG_THREAD_LOCAL _Thread_local  /* C11 fallback; unreachable for MSVC/GCC/Clang */
#endif

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

/* Windows includes */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <BaseTsd.h>
#include <intrin.h>
#include <time.h>
#include <stdint.h>
#include <malloc.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "event2/util.h"  /* for evutil_socket_t */
#include "tglog_atomic.h" /* for tglog_pause() used in usleep() spin-wait */

/* === ssize_t === */
typedef SSIZE_T ssize_t;

/* === struct iovec === */
struct iovec {
    void  *iov_base;
    size_t iov_len;
};

/* === writev via WSASend (scatter/gather I/O, single syscall) ===
 * TGLOG_IOV_MAX caps the maximum iovec count to bound stack allocation.
 * For iovcnt <= 16, a fixed stack buffer is used (fast path).
 * For 17..TGLOG_IOV_MAX, _alloca() is used — this is safe because
 * TGLOG_IOV_MAX * sizeof(WSABUF) = 128 * 16 = 2048 bytes max.
 *
 * NOTE: If iovcnt > TGLOG_IOV_MAX, we fail hard with EINVAL instead of
 * silently truncating.  Silent truncation would produce corrupted log
 * frames (especially dangerous in UDP mode where each writev is one
 * datagram). */
#define TGLOG_IOV_MAX 128

TGLOG_INLINE ssize_t writev(evutil_socket_t fd, const struct iovec *iov, int iovcnt) {
    WSABUF stack_buf[16]; /* small fast path on stack */
    WSABUF *wsa_buf;

    if (iovcnt <= 0) return 0;
    if (iovcnt > TGLOG_IOV_MAX) {
        /* POSIX-style: refuse rather than truncate. */
        WSASetLastError(WSAEINVAL);
        errno = EINVAL;
        return -1;
    }

    if (iovcnt <= 16) {
        wsa_buf = stack_buf;
    } else {
        wsa_buf = (WSABUF *)_alloca(iovcnt * sizeof(WSABUF));
    }

    for (int i = 0; i < iovcnt; i++) {
        wsa_buf[i].buf = (CHAR *)iov[i].iov_base;
        wsa_buf[i].len = (ULONG)iov[i].iov_len;
    }

    DWORD bytes_sent = 0;
    int ret = WSASend((SOCKET)fd, wsa_buf, iovcnt, &bytes_sent, 0, NULL, NULL);
    if (ret == SOCKET_ERROR) {
        return -1;
    }
    return (ssize_t)bytes_sent;
}

/* === gettimeofday ===
 * Prefer GetSystemTimePreciseAsFileTime when available. On older Windows,
 * fall back to a QueryPerformanceCounter-based wall clock calibrated against
 * GetSystemTimeAsFileTime to provide microsecond-resolution timestamps.
 *
 * Thread safety: both tglog_win_qpc_time_us() and gettimeofday() use
 * InitOnceExecuteOnce to guarantee one-shot initialisation without races.
 *
 * NOTE: these process-global structures (_tglog_qpc_state, _tglog_gtod_once,
 * _tglog_precise_time_fn, _tglog_qpc_once) are DEFINED in tglog_platform.c
 * and only DECLARED here — declaring them `static` in a header would give
 * each translation unit its own private copy and race-init them independently. */
typedef VOID (WINAPI *tglog_get_system_time_precise_as_file_time_fn)(LPFILETIME);

TGLOG_INLINE uint64_t tglog_win_filetime_to_epoch_us(const FILETIME *ft) {
    uint64_t t = ((uint64_t)ft->dwHighDateTime << 32) | ft->dwLowDateTime;
    t -= 116444736000000000ULL; /* Jan 1, 1601 -> Jan 1, 1970 */
    return t / 10ULL;
}

/* Safe (diff * 1e6 / freq) via muldiv; bare multiply overflows u64 at
 * ~21 days when freq = 10 MHz (common on modern x86). */
TGLOG_INLINE uint64_t tglog_win_qpc_ticks_to_us(uint64_t diff, uint64_t freq) {
    return (diff / freq) * 1000000ULL + ((diff % freq) * 1000000ULL) / freq;
}

/* --- QPC fallback: thread-safe one-shot calibration --- */
typedef struct {
    LARGE_INTEGER freq;
    LARGE_INTEGER start_counter;
    uint64_t      start_epoch_us;
} _tglog_qpc_state_t;

extern _tglog_qpc_state_t _tglog_qpc_state;
extern INIT_ONCE          _tglog_qpc_once;
BOOL CALLBACK _tglog_qpc_init_once(PINIT_ONCE once, PVOID param, PVOID *ctx);

TGLOG_INLINE uint64_t tglog_win_qpc_time_us(void) {
    LARGE_INTEGER now;

    InitOnceExecuteOnce(&_tglog_qpc_once, _tglog_qpc_init_once,
                        &_tglog_qpc_state, NULL);

    QueryPerformanceCounter(&now);
    {
        uint64_t diff = (uint64_t)(now.QuadPart - _tglog_qpc_state.start_counter.QuadPart);
        uint64_t freq = (uint64_t)_tglog_qpc_state.freq.QuadPart;
        return _tglog_qpc_state.start_epoch_us + tglog_win_qpc_ticks_to_us(diff, freq);
    }
}

/* --- gettimeofday: thread-safe precise-timer probe --- */
extern tglog_get_system_time_precise_as_file_time_fn _tglog_precise_time_fn;
extern INIT_ONCE _tglog_gtod_once;
BOOL CALLBACK _tglog_gtod_init_once(PINIT_ONCE once, PVOID param, PVOID *ctx);

TGLOG_INLINE int gettimeofday(struct timeval *tv, void *tz) {
    uint64_t epoch_us;
    (void)tz;

    InitOnceExecuteOnce(&_tglog_gtod_once, _tglog_gtod_init_once, NULL, NULL);

    if (_tglog_precise_time_fn != NULL) {
        FILETIME ft;
        _tglog_precise_time_fn(&ft);
        epoch_us = tglog_win_filetime_to_epoch_us(&ft);
    } else {
        epoch_us = tglog_win_qpc_time_us();
    }

    tv->tv_sec = (long)(epoch_us / 1000000ULL);
    tv->tv_usec = (long)(epoch_us % 1000000ULL);
    return 0;
}

/* === localtime_r === */
TGLOG_INLINE struct tm *localtime_r(const time_t *timep, struct tm *result) {
    return localtime_s(result, timep) == 0 ? result : NULL;
}

/* === rand_r (LCG implementation) === */
TGLOG_INLINE int rand_r(unsigned int *seed) {
    *seed = (*seed) * 1103515245 + 12345;
    return (int)((*seed >> 16) & 0x7fff);
}

/* === strdup for MSVC === */
#ifndef strdup
#define strdup _strdup
#endif

/* === Socket error classification for Winsock ===
 * On Windows, socket errors come from WSAGetLastError() (e.g. WSAEWOULDBLOCK
 * = 10035) while C <errno.h> defines EAGAIN=11 / EWOULDBLOCK=140.  The two
 * numbering spaces do NOT overlap, so `sock_err == EAGAIN` never matches a
 * Winsock error.  We provide macros that check BOTH value spaces so callers
 * can keep writing `TGLOG_ERR_IS_EAGAIN(sock_err)` portably.
 *
 * NOTE: The old `#ifndef EAGAIN / #define EAGAIN WSAEWOULDBLOCK` guards were
 * dead code because <errno.h> already defines EAGAIN on MSVC. */
#define TGLOG_ERR_IS_EAGAIN(e)     ((e) == EAGAIN || (e) == EWOULDBLOCK || \
                                    (e) == WSAEWOULDBLOCK)
#define TGLOG_ERR_IS_ENOBUFS(e)    ((e) == ENOBUFS || (e) == WSAENOBUFS)

/* === MSG_NOSIGNAL (no-op on Windows) === */
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

/* === Socket error helper === */
TGLOG_INLINE const char *tglog_socket_error_str(int error) {
    static TGLOG_THREAD_LOCAL char buf[256];
    int err = (error == 0) ? WSAGetLastError() : error;
    snprintf(buf, sizeof(buf), "WSA Error %d", err);
    return buf;
}

/* === usleep (high-precision via QPC + Sleep hybrid) ===
 * For delays < 2ms: pure spin-wait using QueryPerformanceCounter (~sub-us accuracy).
 * For delays >= 2ms: Sleep() handles the bulk, spin-wait covers the remainder.
 * This avoids the ~15.6ms granularity of Sleep() without using timeBeginPeriod().
 *
 * WARNING: sub-2ms precision comes at the cost of a 100%-CPU spin. Do not call
 * this for long/frequent waits on many threads — prefer Sleep() directly there. */
TGLOG_INLINE void usleep(unsigned long usec) {
    LARGE_INTEGER freq, start, now;
    uint64_t target_ticks;
    uint64_t elapsed_ticks;

    if (usec == 0) return;

    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);

    /* target_ticks = usec * freq / 1e6, with overflow guard for huge usec.
     * This is bounded: caller sends a small microsecond value. */
    target_ticks = ((uint64_t)usec / 1000000ULL) * (uint64_t)freq.QuadPart
                 + ((uint64_t)usec % 1000000ULL) * (uint64_t)freq.QuadPart / 1000000ULL;

    if (usec >= 2000) {
        /* Sleep the bulk, leaving 1.5ms margin for spin-wait to refine */
        DWORD sleep_ms = (DWORD)((usec - 1500) / 1000);
        if (sleep_ms > 0) Sleep(sleep_ms);
    }

    /* Spin-wait the remaining time with QPC precision */
    for (;;) {
        QueryPerformanceCounter(&now);
        elapsed_ticks = (uint64_t)(now.QuadPart - start.QuadPart);
        if (elapsed_ticks >= target_ticks) break;
        tglog_pause();
    }
}

/* === sleep === */
TGLOG_INLINE unsigned int sleep(unsigned int seconds) {
    Sleep((DWORD)seconds * 1000);
    return 0;
}

/* === getopt / getopt_long ===
 *
 * NOTE: optarg/optind/opterr/optopt are process-global in POSIX. The
 * storage and function bodies live in tglog_platform.c. Declaring them
 * `static` in this header (the previous design) gave each translation
 * unit its own copy, so any TU that called getopt() couldn't see the
 * state another TU updated — a silent correctness bug. */
#ifndef no_argument
#define no_argument       0
#define required_argument 1
#define optional_argument 2
#endif

struct option {
    const char *name;
    int         has_arg;
    int        *flag;
    int         val;
};

extern char *optarg;
extern int   optind;
extern int   opterr;
extern int   optopt;

int getopt(int argc, char *const argv[], const char *optstring);
int getopt_long(int argc, char *const argv[], const char *optstring,
                const struct option *longopts, int *longindex);

/* === bool / true / false (C99 compat when <stdbool.h> unavailable) === */
#ifndef __cplusplus
#ifndef true
#define true  1
#define false 0
typedef int bool;
#endif
#endif

#else /* POSIX */

#include <sys/time.h>
#include <sys/uio.h>
#include <string.h>
#include <errno.h>

/* === Socket error helper (POSIX) === */
#define tglog_socket_error_str(e) strerror((e) == 0 ? errno : (e))

/* === Socket error classification (POSIX) === */
#if EAGAIN == EWOULDBLOCK
#define TGLOG_ERR_IS_EAGAIN(e)   ((e) == EAGAIN)
#else
#define TGLOG_ERR_IS_EAGAIN(e)   ((e) == EAGAIN || (e) == EWOULDBLOCK)
#endif
#define TGLOG_ERR_IS_ENOBUFS(e)  ((e) == ENOBUFS)

#endif /* _WIN32 */
#endif /* TGLOG_PLATFORM_H */
