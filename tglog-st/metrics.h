#ifndef TGLOG_METRICS_H
#define TGLOG_METRICS_H

/* Standard library headers */
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

/* System headers */

/* Third-party library headers */

/* Project headers */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * tglog_metrics
 *
 * In single-threaded version, no atomic operations are needed.
 * All metrics are updated directly without synchronization.
 */
typedef struct tglog_metrics {
    uint64_t request_msg_count;                     /* request message count, application request to send */
    uint64_t enqueued_msg_count;                    /* enqueued message count, message successfully enqueued */
    uint64_t dequeued_msg_count;                    /* dequeued message count, message successfully dequeued */
    uint64_t succeeded_msg_count;                   /* succeeded message count, message successfully sent */
    uint64_t failed_msg_count;                      /* failed message count, message failed to send, include timeout and dropped */
    uint64_t timeout_msg_count;                     /* timeout message count, message failed due to timeout */
    uint64_t retry_msg_count;                       /* retry message count, count every retry */
    uint64_t dropped_msg_count;                     /* dropped message count, request but not enqueued */
    uint64_t eagain_err_count;                      /* EAGAIN error count */
    uint64_t rescued_msg_bytes;                     /* rescued message bytes */
    uint64_t opened_conn_count;                     /* opened connection count */
    uint64_t closed_conn_count;                     /* closed connection count */
    uint64_t error_conn_count;                      /* error connection count */
    uint64_t endpoint_count;                        /* endpoint count gauge */
    uint64_t add_endpoint_count;                    /* add endpoint count */
    uint64_t remove_endpoint_count;                 /* remove endpoint count */
    uint64_t msg_queue_size;                        /* message queue size gauge */
    uint64_t msg_queue_capacity;                    /* message queue capacity gauge */
    uint64_t total_update_count;                    /* total `tglog_client_update` call count */
    uint64_t timeout_update_count;                  /* timeout `tglog_client_update` call count */
    uint64_t idle_update_count;                     /* idle `tglog_client_update` call count, no message dispatched in an update call */
    uint64_t max_update_time_us;                    /* max time a `tglog_client_update` call took in microseconds */
    uint64_t limit_update_time_us;                  /* limit time a `tglog_client_update` call can take in config in microseconds */
    uint64_t max_event_dispatch_time_us;            /* max time event dispatch took in microseconds in a single update call */
    uint64_t max_msg_dispatch_time_us;            /* max time message dispatch took in microseconds in a single update call */
    uint64_t max_msg_dispatch_count;                /* max message dispatch count in a single update call */
} tglog_metrics;

/**
 * Create a metrics.
 *
 * @return A metrics instance or NULL on errors.
 */
tglog_metrics *tglog_metrics_create(void);

/**
 * Destroy a metrics.
 *
 * @param metrics The metrics to destroy.
 */
void tglog_metrics_destroy(tglog_metrics *metrics);

/**
 * Increment the request message count.
 *
 * @param metrics The metrics instance.
 * @param count The count to increment.
 */
void tglog_metrics_inc_request_msg_count(tglog_metrics *metrics, int count);

/**
 * Increment the enqueued message count.
 *
 * @param metrics The metrics instance.
 * @param count The count to increment.
 */
void tglog_metrics_inc_enqueue_msg_count(tglog_metrics *metrics, int count);

/**
 * Increment the dequeued message count.
 *
 * @param metrics The metrics instance.
 * @param count The count to increment.
 */
void tglog_metrics_inc_dequeue_msg_count(tglog_metrics *metrics, int count);

/**
 * Increment the succeeded message count.
 *
 * @param metrics The metrics instance.
 * @param count The count to increment.
 */
void tglog_metrics_inc_succeeded_msg_count(tglog_metrics *metrics, int count);

/**
 * Increment the failed message count.
 *
 * @param metrics The metrics instance.
 * @param count The count to increment.
 */
void tglog_metrics_inc_failed_msg_count(tglog_metrics *metrics, int count);

/**
 * Increment the timeout message count.
 *
 * @param metrics The metrics instance.
 * @param count The count to increment.
 */
void tglog_metrics_inc_timeout_msg_count(tglog_metrics *metrics, int count);

/**
 * Increment the retry message count.
 *
 * @param metrics The metrics instance.
 * @param count The count to increment.
 */
void tglog_metrics_inc_retry_msg_count(tglog_metrics *metrics, int count);

/**
 * Increment the dropped message count.
 *
 * @param metrics The metrics instance.
 * @param count The count to increment.
 */
void tglog_metrics_inc_dropped_msg_count(tglog_metrics *metrics, int count);

/**
 * Increment the EAGAIN error count.
 *
 * @param metrics The metrics instance.
 * @param count The count to increment.
 */
void tglog_metrics_inc_eagain_err_count(tglog_metrics *metrics, int count);

/**
 * Increment the rescued message bytes.
 *
 * @param metrics The metrics instance.
 * @param bytes The bytes to increment.
 */
void tglog_metrics_inc_rescued_msg_bytes(tglog_metrics *metrics, size_t bytes);

/**
 * Increment the opened connection count.
 *
 * @param metrics The metrics instance.
 * @param count The count to increment.
 */
void tglog_metrics_inc_opened_conn_count(tglog_metrics *metrics, int count);

/**
 * Increment the closed connection count.
 *
 * @param metrics The metrics instance.
 * @param count The count to increment.
 */
void tglog_metrics_inc_closed_conn_count(tglog_metrics *metrics, int count);

/**
 * Increment the error connection count.
 *
 * @param metrics The metrics instance.
 * @param count The count to increment.
 */
void tglog_metrics_inc_error_conn_count(tglog_metrics *metrics, int count);

/**
 * Set the endpoint count.
 *
 * @param metrics The metrics instance.
 * @param count The endpoint count.
 */
void tglog_metrics_set_endpoint_count(tglog_metrics *metrics, uint64_t count);

/**
 * Increment the add endpoint count.
 *
 * @param metrics The metrics instance.
 * @param count The count to increment.
 */
void tglog_metrics_inc_add_endpoint_count(tglog_metrics *metrics, int count);

/**
 * Increment the remove endpoint count.
 *
 * @param metrics The metrics instance.
 * @param count The count to increment.
 */
void tglog_metrics_inc_remove_endpoint_count(tglog_metrics *metrics, int count);

/**
 * Set the message queue size gauge.
 *
 * @param metrics The metrics instance.
 * @param size The message queue size.
 */
void tglog_metrics_set_msg_queue_size(tglog_metrics *metrics, uint64_t size);

/**
 * Set the message queue capacity gauge.
 *
 * @param metrics The metrics instance.
 * @param capacity The message queue capacity.
 */
void tglog_metrics_set_msg_queue_capacity(tglog_metrics *metrics, uint64_t capacity);

/**
 * Increment the total update count.
 *
 * @param metrics The metrics instance.
 * @param count The count to increment.
 */
void tglog_metrics_inc_total_update_count(tglog_metrics *metrics, int count);

/**
 * Increment the timeout update count.
 *
 * @param metrics The metrics instance.
 * @param count The count to increment.
 */
void tglog_metrics_inc_timeout_update_count(tglog_metrics *metrics, int count);

/**
 * Increment the idle update count.
 *
 * @param metrics The metrics instance.
 * @param count The count to increment.
 */
void tglog_metrics_inc_idle_update_count(tglog_metrics *metrics, int count);

/**
 * Set the max update time gauge.
 *
 * @param metrics The metrics instance.
 * @param time The max update time.
 */
void tglog_metrics_set_max_update_time(tglog_metrics *metrics, uint64_t time);

/**
 * Set the limit update time gauge.
 *
 * @param metrics The metrics instance.
 * @param time The limit update time.
 */
void tglog_metrics_set_limit_update_time(tglog_metrics *metrics, uint64_t time);

/**
 * Set the max event dispatch time gauge.
 *
 * @param metrics The metrics instance.
 * @param time The max event dispatch time.
 */
void tglog_metrics_set_max_event_dispatch_time(tglog_metrics *metrics, uint64_t time);

/**
 * Set the max message dispatch time gauge.
 *
 * @param metrics The metrics instance.
 * @param time The max message dispatch time.
 */
void tglog_metrics_set_max_msg_dispatch_time(tglog_metrics *metrics, uint64_t time);

/**
 * Set the max message dispatch count gauge.
 *
 * @param metrics The metrics instance.
 * @param count The max message dispatch count.
 */
void tglog_metrics_set_max_msg_dispatch_count(tglog_metrics *metrics, uint64_t count);

/**
 * Print the metrics in plain text format.
 *
 * @param metrics The metrics instance.
 * @param buffer The buffer to print to, at least 4096 bytes.
 * @param size The size of the buffer.
 * @return The number of bytes written to the buffer or -1 on error.
 */
int tglog_metrics_print(tglog_metrics *metrics, char *buffer, size_t size);

/**
 * Print the metrics in JSON format.
 *
 * @param metrics The metrics instance.
 * @param buffer The buffer to print to.
 * @param size The size of the buffer.
 * @return The number of bytes written to the buffer or -1 on error.
 */
int tglog_metrics_print_json(tglog_metrics *metrics, char *buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* TGLOG_METRICS_H */