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
 * TGLOG_IOV_MAX * sizeof(WSABUF) = 128 * 16 = 2048 bytes max. */
#define TGLOG_IOV_MAX 128

TGLOG_INLINE ssize_t writev(evutil_socket_t fd, const struct iovec *iov, int iovcnt) {
    WSABUF stack_buf[16]; /* small fast path on stack */
    WSABUF *wsa_buf;

    if (iovcnt <= 0) return 0;
    if (iovcnt > TGLOG_IOV_MAX) iovcnt = TGLOG_IOV_MAX;

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
 * InitOnceExecuteOnce to guarantee one-shot initialisation without races. */
typedef VOID (WINAPI *tglog_get_system_time_precise_as_file_time_fn)(LPFILETIME);

TGLOG_INLINE uint64_t tglog_win_filetime_to_epoch_us(const FILETIME *ft) {
    uint64_t t = ((uint64_t)ft->dwHighDateTime << 32) | ft->dwLowDateTime;
    t -= 116444736000000000ULL; /* Jan 1, 1601 -> Jan 1, 1970 */
    return t / 10ULL;
}

/* --- QPC fallback: thread-safe one-shot calibration --- */
typedef struct {
    LARGE_INTEGER freq;
    LARGE_INTEGER start_counter;
    uint64_t      start_epoch_us;
} _tglog_qpc_state_t;

static _tglog_qpc_state_t _tglog_qpc_state;
static INIT_ONCE           _tglog_qpc_once = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK _tglog_qpc_init_once(PINIT_ONCE once, PVOID param, PVOID *ctx) {
    FILETIME ft;
    _tglog_qpc_state_t *s = (_tglog_qpc_state_t *)param;
    (void)once; (void)ctx;
    QueryPerformanceFrequency(&s->freq);
    QueryPerformanceCounter(&s->start_counter);
    GetSystemTimeAsFileTime(&ft);
    s->start_epoch_us = tglog_win_filetime_to_epoch_us(&ft);
    return TRUE;
}

TGLOG_INLINE uint64_t tglog_win_qpc_time_us(void) {
    LARGE_INTEGER now;

    InitOnceExecuteOnce(&_tglog_qpc_once, _tglog_qpc_init_once,
                        &_tglog_qpc_state, NULL);

    QueryPerformanceCounter(&now);
    return _tglog_qpc_state.start_epoch_us +
        (uint64_t)((now.QuadPart - _tglog_qpc_state.start_counter.QuadPart)
                    * 1000000ULL / _tglog_qpc_state.freq.QuadPart);
}

/* --- gettimeofday: thread-safe precise-timer probe --- */
static tglog_get_system_time_precise_as_file_time_fn _tglog_precise_time_fn;
static INIT_ONCE _tglog_gtod_once = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK _tglog_gtod_init_once(PINIT_ONCE once, PVOID param, PVOID *ctx) {
    HMODULE kernel32;
    (void)once; (void)param; (void)ctx;
    kernel32 = GetModuleHandleA("kernel32.dll");
    if (kernel32 != NULL) {
        _tglog_precise_time_fn =
            (tglog_get_system_time_precise_as_file_time_fn)
                GetProcAddress(kernel32, "GetSystemTimePreciseAsFileTime");
    }
    return TRUE;
}

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
 * This avoids the ~15.6ms granularity of Sleep() without using timeBeginPeriod(). */
TGLOG_INLINE void usleep(unsigned long usec) {
    LARGE_INTEGER freq, start, now;
    double target_us;

    if (usec == 0) return;

    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    target_us = (double)usec;

    if (usec >= 2000) {
        /* Sleep the bulk, leaving 1.5ms margin for spin-wait to refine */
        DWORD sleep_ms = (DWORD)((usec - 1500) / 1000);
        if (sleep_ms > 0) Sleep(sleep_ms);
    }

    /* Spin-wait the remaining time with QPC precision */
    for (;;) {
        double elapsed;
        QueryPerformanceCounter(&now);
        elapsed = (double)(now.QuadPart - start.QuadPart) * 1000000.0 / (double)freq.QuadPart;
        if (elapsed >= target_us) break;
        _mm_pause();
    }
}

/* === sleep === */
TGLOG_INLINE unsigned int sleep(unsigned int seconds) {
    Sleep((DWORD)seconds * 1000);
    return 0;
}

/* === getopt / getopt_long === */
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

static char *optarg = NULL;
static int   optind = 1;
static int   opterr = 1;
static int   optopt = '?';

TGLOG_INLINE int getopt(int argc, char *const argv[], const char *optstring) {
    static int sp = 1;
    int c;
    const char *cp;

    if (sp == 1) {
        if (optind >= argc || argv[optind][0] != '-' || argv[optind][1] == '\0')
            return -1;
        if (strcmp(argv[optind], "--") == 0) {
            optind++;
            return -1;
        }
    }

    c = argv[optind][sp];
    cp = strchr(optstring, c);
    if (cp == NULL || c == ':') {
        if (opterr)
            fprintf(stderr, "%s: illegal option -- %c\n", argv[0], c);
        optopt = c;
        if (argv[optind][++sp] == '\0') {
            optind++;
            sp = 1;
        }
        return '?';
    }

    if (cp[1] == ':') {
        if (argv[optind][sp+1] != '\0') {
            optarg = &argv[optind++][sp+1];
        } else if (++optind >= argc) {
            if (opterr)
                fprintf(stderr, "%s: option requires an argument -- %c\n", argv[0], c);
            optopt = c;
            sp = 1;
            return '?';
        } else {
            optarg = argv[optind++];
        }
        sp = 1;
    } else {
        if (argv[optind][++sp] == '\0') {
            sp = 1;
            optind++;
        }
        optarg = NULL;
    }

    return c;
}

TGLOG_INLINE int getopt_long(int argc, char *const argv[], const char *optstring,
                              const struct option *longopts, int *longindex) {
    /* Check for long option first */
    if (optind < argc && argv[optind][0] == '-' && argv[optind][1] == '-' &&
        argv[optind][2] != '\0') {
        const char *name = argv[optind] + 2;
        int i;
        for (i = 0; longopts[i].name != NULL; i++) {
            size_t namelen = strlen(longopts[i].name);
            if (strncmp(name, longopts[i].name, namelen) == 0) {
                int match = 0;
                if (name[namelen] == '\0') {
                    match = 1;
                    optarg = NULL;
                } else if (name[namelen] == '=' && longopts[i].has_arg) {
                    match = 1;
                    optarg = (char *)(name + namelen + 1);
                }
                if (match) {
                    if (longindex)
                        *longindex = i;
                    optind++;
                    if (longopts[i].has_arg == required_argument && optarg == NULL) {
                        if (optind >= argc) {
                            if (opterr)
                                fprintf(stderr, "%s: option '--%s' requires an argument\n",
                                        argv[0], longopts[i].name);
                            return '?';
                        }
                        optarg = argv[optind++];
                    }
                    if (longopts[i].flag) {
                        *longopts[i].flag = longopts[i].val;
                        return 0;
                    }
                    return longopts[i].val;
                }
            }
        }
        /* unrecognised long option */
        if (opterr)
            fprintf(stderr, "%s: unrecognised option '--%s'\n", argv[0], name);
        optind++;
        return '?';
    }

    return getopt(argc, argv, optstring);
}

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
