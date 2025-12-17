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
 * The top level metrics are usually the sum of the worker metrics.
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
    struct tglog_metrics **workers;                 /* worker metrics */
    int worker_count;                               /* worker count */
} tglog_metrics;

/**
 * Create a metrics.
 *
 * @param worker_count The worker count.
 * @return A metrics instance or NULL on errors.
 */
tglog_metrics *tglog_metrics_create(int worker_count);

/**
 * Destroy a metrics.
 *
 * @param metrics The metrics to destroy.
 */
void tglog_metrics_destroy(tglog_metrics *metrics);

/**
 * Increment the request message count.
 * This function will increase the total request message count and the worker request message count.
 *
 * @param metrics The metrics instance.
 * @param worker_id The worker ID.
 * @param count The count to increment.
 */
void tglog_metrics_inc_request_msg_count(tglog_metrics *metrics, int worker_id, int count);

/**
 * Increment the enqueued message count.
 * This function will increase the total enqueued message count and the worker enqueued message count.
 *
 * @param metrics The metrics instance.
 * @param worker_id The worker ID.
 * @param count The count to increment.
 */
void tglog_metrics_inc_enqueue_msg_count(tglog_metrics *metrics, int worker_id, int count);

/**
 * Increment the dequeued message count.
 * This function will increase the total dequeued message count and the worker dequeued message count.
 *
 * @param metrics The metrics instance.
 * @param worker_id The worker ID.
 * @param count The count to increment.
 */
void tglog_metrics_inc_dequeue_msg_count(tglog_metrics *metrics, int worker_id, int count);

/**
 * Increment the succeeded message count.
 * This function will increase the total succeeded message count and the worker succeeded message count.
 *
 * @param metrics The metrics instance.
 * @param worker_id The worker ID.
 * @param count The count to increment.
 */
void tglog_metrics_inc_succeeded_msg_count(tglog_metrics *metrics, int worker_id, int count);

/**
 * Increment the failed message count.
 * This function will increase the total failed message count and the worker failed message count.
 *
 * @param metrics The metrics instance.
 * @param worker_id The worker ID.
 * @param count The count to increment.
 */
void tglog_metrics_inc_failed_msg_count(tglog_metrics *metrics, int worker_id, int count);

/**
 * Increment the timeout message count.
 * This function will increase the total timeout message count and the worker timeout message count.
 *
 * @param metrics The metrics instance.
 * @param worker_id The worker ID.
 * @param count The count to increment.
 */
void tglog_metrics_inc_timeout_msg_count(tglog_metrics *metrics, int worker_id, int count);

/**
 * Increment the retry message count.
 * This function will increase the total retry message count and the worker retry message count.
 *
 * @param metrics The metrics instance.
 * @param worker_id The worker ID.
 * @param count The count to increment.
 */
void tglog_metrics_inc_retry_msg_count(tglog_metrics *metrics, int worker_id, int count);

/**
 * Increment the dropped message count.
 * This function will increase the total dropped message count and the worker dropped message count.
 *
 * @param metrics The metrics instance.
 * @param worker_id The worker ID.
 * @param count The count to increment.
 */
void tglog_metrics_inc_dropped_msg_count(tglog_metrics *metrics, int worker_id, int count);

/**
 * Increment the EAGAIN error count.
 * This function will increase the total EAGAIN error count and the worker EAGAIN error count.
 *
 * @param metrics The metrics instance.
 * @param worker_id The worker ID.
 * @param count The count to increment.
 */
void tglog_metrics_inc_eagain_err_count(tglog_metrics *metrics, int worker_id, int count);

/**
 * Increment the rescued message bytes.
 *
 * @param metrics The metrics instance.
 * @param worker_id The worker ID.
 * @param bytes The bytes to increment.
 */
void tglog_metrics_inc_rescued_msg_bytes(tglog_metrics *metrics, int worker_id, size_t bytes);

/**
 * Increment the opened connection count.
 * This function will increase the total opened connection count and the worker opened connection count.
 *
 * @param metrics The metrics instance.
 * @param worker_id The worker ID.
 * @param count The count to increment.
 */
void tglog_metrics_inc_opened_conn_count(tglog_metrics *metrics, int worker_id, int count);

/**
 * Increment the closed connection count.
 * This function will increase the total closed connection count and the worker closed connection count.
 *
 * @param metrics The metrics instance.
 * @param worker_id The worker ID.
 * @param count The count to increment.
 */
void tglog_metrics_inc_closed_conn_count(tglog_metrics *metrics, int worker_id, int count);

/**
 * Increment the error connection count.
 * This function will increase the total error connection count and the worker error connection count.
 *
 * @param metrics The metrics instance.
 * @param worker_id The worker ID.
 * @param count The count to increment.
 */
void tglog_metrics_inc_error_conn_count(tglog_metrics *metrics, int worker_id, int count);

/**
 * Set the endpoint count.
 * This function just set the top level endpoint count.
 *
 * @param metrics The metrics instance.
 * @param count The endpoint count.
 */
void tglog_metrics_set_endpoint_count(tglog_metrics *metrics, uint64_t count);

/**
 * Set the worker endpoint count.
 * This function just set the worker endpoint count.
 *
 * @param metrics The metrics instance.
 * @param worker_id The worker ID.
 * @param count The endpoint count.
 */
void tglog_metrics_set_worker_endpoint_count(tglog_metrics *metrics, int worker_id, uint64_t count);

/**
 * Increment the add endpoint count.
 * This function will increase the total add endpoint count and the worker add endpoint count.
 *
 * @param metrics The metrics instance.
 * @param worker_id The worker ID.
 * @param count The count to increment.
 */
void tglog_metrics_inc_add_endpoint_count(tglog_metrics *metrics, int worker_id, int count);

/**
 * Increment the remove endpoint count.
 * This function will increase the total remove endpoint count and the worker remove endpoint count.
 *
 * @param metrics The metrics instance.
 * @param worker_id The worker ID.
 * @param count The count to increment.
 */
void tglog_metrics_inc_remove_endpoint_count(tglog_metrics *metrics, int worker_id, int count);

/**
 * Set the message queue size gauge.
 * This function just set the top level message queue size gauge.
 *
 * @param metrics The metrics instance.
 * @param size The message queue size.
 */
void tglog_metrics_set_msg_queue_size(tglog_metrics *metrics, uint64_t size);

/**
 * Set the worker message queue size gauge.
 * This function just set the worker message queue size gauge.
 *
 * @param metrics The metrics instance.
 * @param worker_id The worker ID.
 * @param size The message queue size.
 */
void tglog_metrics_set_worker_msg_queue_size(tglog_metrics *metrics, int worker_id, uint64_t size);

/**
 * Set the message queue capacity gauge.
 * This function just set the top level message queue capacity gauge.
 *
 * @param metrics The metrics instance.
 * @param capacity The message queue capacity.
 */
void tglog_metrics_set_msg_queue_capacity(tglog_metrics *metrics, uint64_t capacity);

/**
 * Set the worker message queue capacity gauge.
 * This function just set the worker message queue capacity gauge.
 *
 * @param metrics The metrics instance.
 * @param worker_id The worker ID.
 * @param capacity The message queue capacity.
 */
void tglog_metrics_set_worker_msg_queue_capacity(tglog_metrics *metrics, int worker_id, uint64_t capacity);

/**
 * Print the metrics in JSON format.
 *
 * @param metrics The metrics instance.
 * @param buffer The buffer to store the JSON string, at least 4096 bytes.
 * @param size The size of the buffer.
 * @return The length of the JSON string, or -1 on error.
 */
int tglog_metrics_print(tglog_metrics *metrics, char *buffer, size_t size);

/**
 * Print the metrics in JSON format.
 *
 * @param metrics The metrics instance.
 * @param buffer The buffer to store the JSON string, at least 8192 bytes.
 * @param size The size of the buffer.
 * @return The length of the JSON string, or -1 on error.
 */
int tglog_metrics_print_json(tglog_metrics *metrics, char *buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* TGLOG_METRICS_H */
