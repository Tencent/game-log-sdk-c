/*
 * TGLog Multi-Threaded Version - Unified Header
 *
 * This is the main header file for TGLog (multithreaded version).
 * Users only need to include this file to use all TGLog functionality.
 *
 * Usage:
 *   #include <tglog/tglog.h>
 *
 * Link with:
 *   -ltglog -levent -levent_pthreads -lck -ljemalloc -lpthread -lm
 */

#ifndef TGLOG_H
#define TGLOG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Public API headers only */
#include "def.h"        /* Common definitions and flags */
#include "err.h"        /* Error codes and messages */
#include "metrics.h"    /* Metrics and statistics */
#include "debug_logger.h"/* Debug logging */
#include "conf.h"       /* Configuration structures */
#include "client.h"     /* Main client API */

/* Version information */
#define TGLOG_VERSION_MAJOR 1
#define TGLOG_VERSION_MINOR 0
#define TGLOG_VERSION_PATCH 0
#define TGLOG_VERSION_STRING "1.0.0"

/* Thread-safety note */
#define TGLOG_THREAD_SAFE 1

#ifdef __cplusplus
}
#endif

#endif /* TGLOG_H */