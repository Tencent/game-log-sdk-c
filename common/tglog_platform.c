/*
 * tglog_platform.c - Process-global state for POSIX-to-Windows shims.
 *
 * This file only has a body on Windows. On POSIX it is intentionally empty
 * so it can be added to the build without Makefile/CMake branching.
 *
 * Purpose: hold the storage and function bodies for the shims whose state
 * must be process-global (getopt's optarg/optind, one-time-init structures
 * for gettimeofday). Putting these in a header with `static` would give
 * each translation unit its own copy and silently break callers that span
 * multiple TUs.
 */

#ifdef _WIN32

#include "tglog_platform.h"

/* ---------------------------------------------------------------------
 * gettimeofday: one-shot calibration state
 * ------------------------------------------------------------------- */

_tglog_qpc_state_t _tglog_qpc_state;
INIT_ONCE          _tglog_qpc_once = INIT_ONCE_STATIC_INIT;

BOOL CALLBACK _tglog_qpc_init_once(PINIT_ONCE once, PVOID param, PVOID *ctx) {
    FILETIME ft;
    _tglog_qpc_state_t *s = (_tglog_qpc_state_t *)param;
    (void)once; (void)ctx;
    QueryPerformanceFrequency(&s->freq);
    QueryPerformanceCounter(&s->start_counter);
    GetSystemTimeAsFileTime(&ft);
    s->start_epoch_us = tglog_win_filetime_to_epoch_us(&ft);
    return TRUE;
}

tglog_get_system_time_precise_as_file_time_fn _tglog_precise_time_fn;
INIT_ONCE _tglog_gtod_once = INIT_ONCE_STATIC_INIT;

BOOL CALLBACK _tglog_gtod_init_once(PINIT_ONCE once, PVOID param, PVOID *ctx) {
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

/* ---------------------------------------------------------------------
 * getopt / getopt_long: POSIX-style process-global state
 * ------------------------------------------------------------------- */

char *optarg = NULL;
int   optind = 1;
int   opterr = 1;
int   optopt = '?';

int getopt(int argc, char *const argv[], const char *optstring) {
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

int getopt_long(int argc, char *const argv[], const char *optstring,
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

#else  /* !_WIN32 */

/* Keep the TU non-empty so some toolchains don't complain about an empty
 * object module. */
typedef int tglog_platform_posix_placeholder;

#endif /* _WIN32 */
