/*
 * win_compat.h - Windows compatibility layer for POSIX APIs
 *
 * Provides Windows implementations of:
 *   - gettimeofday()
 *   - usleep() / sleep()
 *   - getopt_long()
 *   - struct timeval (via winsock2)
 */

#ifndef WIN_COMPAT_H
#define WIN_COMPAT_H

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* -----------------------------------------------------------------------
 * gettimeofday
 * ----------------------------------------------------------------------- */
typedef VOID (WINAPI *tglog_get_system_time_precise_as_file_time_fn)(LPFILETIME);

static inline uint64_t tglog_win_filetime_to_epoch_us(const FILETIME *ft) {
    uint64_t t = ((uint64_t)ft->dwHighDateTime << 32) | ft->dwLowDateTime;
    t -= 116444736000000000ULL;
    return t / 10ULL;
}

static inline uint64_t tglog_win_qpc_time_us(void) {
    static LARGE_INTEGER freq = {0};
    static LARGE_INTEGER start_counter = {0};
    static uint64_t start_epoch_us = 0;
    LARGE_INTEGER now;

    if (freq.QuadPart == 0) {
        FILETIME ft;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&start_counter);
        GetSystemTimeAsFileTime(&ft);
        start_epoch_us = tglog_win_filetime_to_epoch_us(&ft);
    }

    QueryPerformanceCounter(&now);
    return start_epoch_us + (uint64_t)((now.QuadPart - start_counter.QuadPart) * 1000000ULL / freq.QuadPart);
}

static inline int gettimeofday(struct timeval *tv, void *tz) {
    (void)tz;
    if (tv) {
        static tglog_get_system_time_precise_as_file_time_fn precise_time_fn = NULL;
        static int precise_time_checked = 0;
        uint64_t epoch_us;

        if (!precise_time_checked) {
            HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
            if (kernel32 != NULL) {
                precise_time_fn = (tglog_get_system_time_precise_as_file_time_fn)GetProcAddress(kernel32, "GetSystemTimePreciseAsFileTime");
            }
            precise_time_checked = 1;
        }

        if (precise_time_fn != NULL) {
            FILETIME ft;
            precise_time_fn(&ft);
            epoch_us = tglog_win_filetime_to_epoch_us(&ft);
        } else {
            epoch_us = tglog_win_qpc_time_us();
        }

        tv->tv_sec  = (long)(epoch_us / 1000000ULL);
        tv->tv_usec = (long)(epoch_us % 1000000ULL);
    }
    return 0;
}

/* -----------------------------------------------------------------------
 * usleep / sleep
 * ----------------------------------------------------------------------- */
static inline void usleep(unsigned long usec) {
    /* Sleep() takes milliseconds; minimum is 1ms for non-zero values */
    if (usec == 0) return;
    DWORD ms = (DWORD)((usec + 999) / 1000);
    Sleep(ms);
}

static inline unsigned int sleep(unsigned int seconds) {
    Sleep((DWORD)seconds * 1000);
    return 0;
}

/* -----------------------------------------------------------------------
 * getopt / getopt_long
 * ----------------------------------------------------------------------- */
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

static inline int getopt(int argc, char *const argv[], const char *optstring) {
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

static inline int getopt_long(int argc, char *const argv[], const char *optstring,
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

/* Prevent redefinition of 'true' if <stdbool.h> is not available */
#ifndef __cplusplus
#ifndef true
#define true  1
#define false 0
typedef int bool;
#endif
#endif

#endif /* _WIN32 */

#endif /* WIN_COMPAT_H */
