/*
 * TGLog Single-Threaded Version - Unified Header
 *
 * This is the main header file for TGLog-ST (single-threaded version).
 * Users only need to include this file to use all TGLog-ST functionality.
 *
 * Usage:
 *   #include <tglog-st/tglog-st.h>
 *
 * Link with:
 *   -ltglog-st -levent -lck -ljemalloc -lpthread -lm
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
#define TGLOG_ST_VERSION_MAJOR 1
#define TGLOG_ST_VERSION_MINOR 0
#define TGLOG_ST_VERSION_PATCH 0
#define TGLOG_ST_VERSION_STRING "1.0.0"

#ifdef __cplusplus
}
#endif

#endif /* TGLOG_H */