#ifndef TGLOG_WORKER_H
#define TGLOG_WORKER_H

/* Standard library headers */

/* System headers */
#ifdef _WIN32
#else
#include <sys/uio.h>
#endif

/* Third-party library headers */
#include "ck_ht.h"
#include "ck_ring.h"

/* Project headers */
#include "client.h"
#include "request.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tglog_client tglog_client;
typedef struct tglog_conn_list tglog_conn_list;
typedef struct tglog_debug_logger tglog_debug_logger;

typedef struct tglog_worker {
    int id;                                         /* worker id */
    const tglog_client *client;                     /* client */
    tglog_debug_logger *logger;                     /* logger */
    tglog_metrics *metrics;                         /* metrics */

    ck_ht_t delay_close_conns;                      /* delay close connections, key: tglog_conn*, value: int */
    ck_ht_t unavailable_endpoints;                  /* unavailable endpoints, key: char*, value: time_t, when endpoint became unavailable */
    ck_ht_t endpoint_retry_counts;                  /* unavailable endpoint recover try counts, key: char*, value: int, retry count */

    ck_ring_t message_queue;                        /* message queue */
    tglog_message **message_queue_buffer;           /* message queue buffer */

    ck_ring_t ep_notify_queue;                      /* endpoint notify queue */
    tglog_endpoint_notify **ep_notify_queue_buffer; /* endpoint notify queue buffer */

    tglog_endpoint_list *endpoints;                 /* endpoints */
    tglog_conn_list *connecting_conns;              /* connecting connections */
    int init_conn_count;                            /* init connection count, use at init time only */
    int ready_conn;                                 /* worker init ready conn, use at init time only */
    int failed_conn;                                /* worker init failed conn, use at init time only */

    tglog_conn_list *conns;                         /* connections */
    uint64_t conn_index;                            /* connection index */
    int conns_per_endpoint;                         /* connections per endpoint, init at create time */

    tglog_message *tmp_message;                     /* temporary message */
    tglog_batch *tmp_batch;                         /* temporary batch */
    char tmp_batch_buf[512 * 1024];                 /* temporary batch buffer */
    struct iovec *tmp_iov;                          /* temporary iovec */

    struct event_base *base;                        /* event base */
    struct event *recover_endpoint_event;           /* recover unavailable endpoint timer event */
    struct event *dump_event;                       /* worker info dump timer event */
    struct event *replace_conns_event;              /* replace expired connections timer event */

    struct event *msg_event;                        /* message timer event */
    struct event *batch_timeout_event;              /* batch timeout timer event */
    struct event *batch_retry_event;                /* batch retry timer event */

    struct event *notify_event;                     /* notify event */

    uint32_t state;                                 /* worker state */
    uint64_t drain_and_stop_time;                   /* time when drain and stop started */
} tglog_worker;

/**
 * Create a worker.
 *
 * @param id The ID of the worker (ignored in single-threaded, always 0).
 * @param client The owner client.
 * @return A worker or NULL on errors
 */
tglog_worker *tglog_worker_create(int id, tglog_client *client);

/**
 * Destroy a worker.
 *
 * @param worker The worker to destroy.
 */
void tglog_worker_destroy(tglog_worker *worker);

/**
 * Send data with a worker.
 *
 * @param worker The worker.
 * @param log_payload The log payload.
 * @param callback The callback function.
 * @param user_data The user data passed to the callback.
 * @param msg_flag The message flag.
 * @return 0 on success, others on errors, see def.h to reference the error codes.
 */
int tglog_worker_send(tglog_worker *worker, const char *log_payload, tglog_send_callback callback, void *user_data, char msg_flag);

/**
 * On endpoint updated.
 *
 * @param worker The worker.
 * @param all The current endpoints.
 * @param added The added endpoints.
 * @param removed The removed endpoints.
 */
void tglog_worker_on_endpoint_updated(tglog_worker *worker,
                                      const tglog_endpoint_list *all,
                                      const tglog_endpoint_list *added,
                                      const tglog_endpoint_list *removed);

/**
 * Get the message queue size.
 *
 * @param worker The worker.
 * @return The message queue size.
 */
uint64_t tglog_worker_get_msg_queue_size(tglog_worker *worker);

/**
 * Get the message queue capacity.
 *
 * @param worker The worker.
 * @return The message queue capacity.
 */
uint64_t tglog_worker_get_msg_queue_capacity(tglog_worker *worker);

/**
 * Handle connection closed event for metrics.
 *
 * @param worker The worker (can be NULL in some cases).
 */
void tglog_worker_on_conn_closed(tglog_worker *worker);

/**
 * Handle connection EAGAIN event for metrics.
 *
 * @param worker The worker (can be NULL in some cases).
 */
void tglog_worker_on_conn_eagain(tglog_worker *worker);

/* Single-threaded specific function */
/**
 * Process worker tasks (called by client update).
 *
 * @param worker The worker.
 * @param max_msgs Maximum message number to process.
 * @param start_us Start time in microseconds.
 * @return Number of messages processed.
 */
int tglog_worker_process(tglog_worker *worker, int max_msgs, uint64_t start_us);

#ifdef __cplusplus
};
#endif

#endif /* TGLOG_WORKER_H */