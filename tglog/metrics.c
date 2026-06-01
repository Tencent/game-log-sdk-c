/* Standard library headers */
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

/* System headers */

/* Third-party library headers */
#include "tglog_atomic.h"

/* Project headers */
#include "metrics.h"


tglog_metrics *tglog_metrics_create(int worker_count) {
    if (worker_count <= 0) {
        return NULL;
    }

    /*alloc metrics structure and initialize to zero values*/
    tglog_metrics *metrics = (tglog_metrics *) calloc(1, sizeof(tglog_metrics));
    if (!metrics) {
        return NULL;
    }

    /*alloc worker metrics*/
    metrics->workers = (tglog_metrics **) calloc(worker_count, sizeof(tglog_metrics *));
    if (!metrics->workers) {
        free(metrics);
        return NULL;
    }

    /*alloc and initialize worker metrics*/
    int i;
    for (i = 0; i < worker_count; i++) {
        /*alloc worker metrics and initialize to zero values*/
        metrics->workers[i] = (tglog_metrics *) calloc(1, sizeof(tglog_metrics));
        if (!metrics->workers[i]) {
            /*free already allocated worker metrics*/
            int j;
            for (j = 0; j < i; j++) {
                free(metrics->workers[j]);
            }
            free(metrics->workers);
            free(metrics);
            return NULL;
        }

        metrics->workers[i]->workers = NULL;
        metrics->workers[i]->worker_count = 0;
    }

    metrics->worker_count = worker_count;
    return metrics;
}

void tglog_metrics_destroy(tglog_metrics *metrics) {
    if (!metrics) {
        return;
    }

    /*free all worker metrics*/
    if (metrics->workers) {
        int i;
        for (i = 0; i < metrics->worker_count; i++) {
            if (metrics->workers[i]) {
                free(metrics->workers[i]);
            }
        }
        free(metrics->workers);
    }

    /*free metrics*/
    free(metrics);
}

static void atomic_increment(uint64_t *counter, int count) {
    tglog_atomic_add_64(counter, count);
}

/*
 * Sharded counter pattern: hot-path increments only the per-worker counter.
 * The global counter is computed by summing all worker counters at print time.
 * This eliminates one atomic write per call on the producer/worker hot paths,
 * which were causing cache-line ping-pong on the global metrics struct.
 */
void tglog_metrics_inc_request_msg_count(tglog_metrics *metrics, int worker_id, int count) {
    if (metrics && worker_id >= 0 && worker_id < metrics->worker_count) {
        atomic_increment(&metrics->workers[worker_id]->request_msg_count, count);
    }
}

void tglog_metrics_inc_enqueue_msg_count(tglog_metrics *metrics, int worker_id, int count) {
    if (metrics && worker_id >= 0 && worker_id < metrics->worker_count) {
        atomic_increment(&metrics->workers[worker_id]->enqueued_msg_count, count);
    }
}

void tglog_metrics_inc_dequeue_msg_count(tglog_metrics *metrics, int worker_id, int count) {
    if (metrics && worker_id >= 0 && worker_id < metrics->worker_count) {
        atomic_increment(&metrics->workers[worker_id]->dequeued_msg_count, count);
    }
}

void tglog_metrics_inc_succeeded_msg_count(tglog_metrics *metrics, int worker_id, int count) {
    if (metrics && worker_id >= 0 && worker_id < metrics->worker_count) {
        atomic_increment(&metrics->workers[worker_id]->succeeded_msg_count, count);
    }
}

void tglog_metrics_inc_failed_msg_count(tglog_metrics *metrics, int worker_id, int count) {
    if (metrics && worker_id >= 0 && worker_id < metrics->worker_count) {
        atomic_increment(&metrics->workers[worker_id]->failed_msg_count, count);
    }
}

void tglog_metrics_inc_timeout_msg_count(tglog_metrics *metrics, int worker_id, int count) {
    if (metrics && worker_id >= 0 && worker_id < metrics->worker_count) {
        atomic_increment(&metrics->workers[worker_id]->timeout_msg_count, count);
    }
}

void tglog_metrics_inc_retry_msg_count(tglog_metrics *metrics, int worker_id, int count) {
    if (metrics && worker_id >= 0 && worker_id < metrics->worker_count) {
        atomic_increment(&metrics->workers[worker_id]->retry_msg_count, count);
    }
}

void tglog_metrics_inc_dropped_msg_count(tglog_metrics *metrics, int worker_id, int count) {
    if (metrics && worker_id >= 0 && worker_id < metrics->worker_count) {
        atomic_increment(&metrics->workers[worker_id]->dropped_msg_count, count);
    }
}

void tglog_metrics_inc_eagain_err_count(tglog_metrics *metrics, int worker_id, int count) {
    if (metrics && worker_id >= 0 && worker_id < metrics->worker_count) {
        atomic_increment(&metrics->workers[worker_id]->eagain_err_count, count);
    }
}

void tglog_metrics_inc_rescued_msg_bytes(tglog_metrics *metrics, int worker_id, size_t bytes) {
    if (metrics && worker_id >= 0 && worker_id < metrics->worker_count) {
        atomic_increment(&metrics->workers[worker_id]->rescued_msg_bytes, (int) bytes);
    }
}

void tglog_metrics_inc_opened_conn_count(tglog_metrics *metrics, int worker_id, int count) {
    if (metrics && worker_id >= 0 && worker_id < metrics->worker_count) {
        atomic_increment(&metrics->workers[worker_id]->opened_conn_count, count);
    }
}

void tglog_metrics_inc_closed_conn_count(tglog_metrics *metrics, int worker_id, int count) {
    if (metrics && worker_id >= 0 && worker_id < metrics->worker_count) {
        atomic_increment(&metrics->workers[worker_id]->closed_conn_count, count);
    }
}

void tglog_metrics_inc_error_conn_count(tglog_metrics *metrics, int worker_id, int count) {
    if (metrics && worker_id >= 0 && worker_id < metrics->worker_count) {
        atomic_increment(&metrics->workers[worker_id]->error_conn_count, count);
    }
}

void tglog_metrics_set_endpoint_count(tglog_metrics *metrics, uint64_t count) {
    if (metrics) {
        tglog_atomic_store_64(&metrics->endpoint_count, count);
    }
}

void tglog_metrics_set_worker_endpoint_count(tglog_metrics *metrics, int worker_id, uint64_t count) {
    if (metrics && worker_id >= 0 && worker_id < metrics->worker_count) {
        tglog_atomic_store_64(&metrics->workers[worker_id]->endpoint_count, count);
    }
}

void tglog_metrics_inc_add_endpoint_count(tglog_metrics *metrics, int worker_id, int count) {
    if (metrics && worker_id >= 0 && worker_id < metrics->worker_count) {
        atomic_increment(&metrics->workers[worker_id]->add_endpoint_count, count);
    }
}

void tglog_metrics_inc_remove_endpoint_count(tglog_metrics *metrics, int worker_id, int count) {
    if (metrics && worker_id >= 0 && worker_id < metrics->worker_count) {
        atomic_increment(&metrics->workers[worker_id]->remove_endpoint_count, count);
    }
}

void tglog_metrics_set_msg_queue_size(tglog_metrics *metrics, uint64_t size) {
    if (metrics) {
        tglog_atomic_store_64(&metrics->msg_queue_size, size);
    }
}

void tglog_metrics_set_worker_msg_queue_size(tglog_metrics *metrics, int worker_id, uint64_t size) {
    if (metrics && worker_id >= 0 && worker_id < metrics->worker_count) {
        tglog_atomic_store_64(&metrics->workers[worker_id]->msg_queue_size, size);
    }
}

void tglog_metrics_set_msg_queue_capacity(tglog_metrics *metrics, uint64_t capacity) {
    if (metrics) {
        tglog_atomic_store_64(&metrics->msg_queue_capacity, capacity);
    }
}

void tglog_metrics_set_worker_msg_queue_capacity(tglog_metrics *metrics, int worker_id, uint64_t capacity) {
    if (metrics && worker_id >= 0 && worker_id < metrics->worker_count) {
        tglog_atomic_store_64(&metrics->workers[worker_id]->msg_queue_capacity, capacity);
    }
}

/*
 * Aggregate per-worker counters into a global snapshot.
 * Counters (xxx_count, xxx_bytes) are summed; gauges (endpoint_count,
 * msg_queue_size, msg_queue_capacity) remain in the global struct and
 * are not aggregated here. Out-param `out` must be zero-initialized; only
 * the counter fields are written.
 */
static void tglog_metrics_aggregate_counters(tglog_metrics *metrics, tglog_metrics *out) {
    int i;
    for (i = 0; i < metrics->worker_count; i++) {
        tglog_metrics *w = metrics->workers[i];
        if (!w) continue;
        out->request_msg_count   += tglog_atomic_load_64(&w->request_msg_count);
        out->enqueued_msg_count  += tglog_atomic_load_64(&w->enqueued_msg_count);
        out->dequeued_msg_count  += tglog_atomic_load_64(&w->dequeued_msg_count);
        out->succeeded_msg_count += tglog_atomic_load_64(&w->succeeded_msg_count);
        out->failed_msg_count    += tglog_atomic_load_64(&w->failed_msg_count);
        out->timeout_msg_count   += tglog_atomic_load_64(&w->timeout_msg_count);
        out->retry_msg_count     += tglog_atomic_load_64(&w->retry_msg_count);
        out->dropped_msg_count   += tglog_atomic_load_64(&w->dropped_msg_count);
        out->eagain_err_count    += tglog_atomic_load_64(&w->eagain_err_count);
        out->rescued_msg_bytes   += tglog_atomic_load_64(&w->rescued_msg_bytes);
        out->opened_conn_count   += tglog_atomic_load_64(&w->opened_conn_count);
        out->closed_conn_count   += tglog_atomic_load_64(&w->closed_conn_count);
        out->error_conn_count    += tglog_atomic_load_64(&w->error_conn_count);
        out->add_endpoint_count    += tglog_atomic_load_64(&w->add_endpoint_count);
        out->remove_endpoint_count += tglog_atomic_load_64(&w->remove_endpoint_count);
    }
}

int tglog_metrics_print(tglog_metrics *metrics, char *buffer, size_t size) {
    if (!metrics || !buffer || size == 0) {
        return -1;
    }

    size_t offset = 0;
    int n;

    /* Aggregate per-worker counters; load gauges directly. */
    tglog_metrics agg;
    memset(&agg, 0, sizeof(agg));
    tglog_metrics_aggregate_counters(metrics, &agg);

    uint64_t request_msg_count = agg.request_msg_count;
    uint64_t enqueued_msg_count = agg.enqueued_msg_count;
    uint64_t dequeued_msg_count = agg.dequeued_msg_count;
    uint64_t succeeded_msg_count = agg.succeeded_msg_count;
    uint64_t failed_msg_count = agg.failed_msg_count;
    uint64_t timeout_msg_count = agg.timeout_msg_count;
    uint64_t retry_msg_count = agg.retry_msg_count;
    uint64_t dropped_msg_count = agg.dropped_msg_count;
    uint64_t eagain_err_count = agg.eagain_err_count;
    uint64_t rescued_msg_bytes = agg.rescued_msg_bytes;
    uint64_t opened_conn_count = agg.opened_conn_count;
    uint64_t closed_conn_count = agg.closed_conn_count;
    uint64_t error_conn_count = agg.error_conn_count;
    uint64_t add_endpoint_count = agg.add_endpoint_count;
    uint64_t remove_endpoint_count = agg.remove_endpoint_count;
    uint64_t endpoint_count = tglog_atomic_load_64(&metrics->endpoint_count);
    uint64_t msg_queue_size = tglog_atomic_load_64(&metrics->msg_queue_size);
    uint64_t msg_queue_capacity = tglog_atomic_load_64(&metrics->msg_queue_capacity);

    /* Print header */
    n = snprintf(buffer + offset, size - offset,
                 "\n=== TGLog Metrics Statistics (Total) ===\n");
    if (n < 0 || offset + n >= size) return -1;
    offset += n;

    /* Print message statistics */
    n = snprintf(buffer + offset, size - offset,
                 "\nMessage Statistics:\n"
                 "  Request Count:    %"PRIu64"\n"
                 "  Enqueued Count:   %"PRIu64"\n"
                 "  Dequeued Count:   %"PRIu64"\n"
                 "  Succeeded Count:  %"PRIu64"\n"
                 "  Failed Count:     %"PRIu64"\n"
                 "  Timeout Count:    %"PRIu64"\n"
                 "  Retry Count:      %"PRIu64"\n"
                 "  Dropped Count:    %"PRIu64"\n"
                 "  EAGAIN Errors:    %"PRIu64"\n"
                 "  Rescued Bytes:     %"PRIu64"\n",
                 request_msg_count,
                 enqueued_msg_count,
                 dequeued_msg_count,
                 succeeded_msg_count,
                 failed_msg_count,
                 timeout_msg_count,
                 retry_msg_count,
                 dropped_msg_count,
                 eagain_err_count,
                 rescued_msg_bytes);
    if (n < 0 || offset + n >= size) return -1;
    offset += n;

    /* Print connection statistics */
    n = snprintf(buffer + offset, size - offset,
                 "\nConnection Statistics:\n"
                 "  Opened Count:     %"PRIu64"\n"
                 "  Closed Count:     %"PRIu64"\n"
                 "  Error Count:      %"PRIu64"\n",
                 opened_conn_count,
                 closed_conn_count,
                 error_conn_count);
    if (n < 0 || offset + n >= size) return -1;
    offset += n;

    /* Print endpoint statistics */
    n = snprintf(buffer + offset, size - offset,
                 "\nEndpoint Statistics:\n"
                 "  Current Count:    %"PRIu64"\n"
                 "  Added Count:      %"PRIu64"\n"
                 "  Removed Count:    %"PRIu64"\n",
                 endpoint_count,
                 add_endpoint_count,
                 remove_endpoint_count);
    if (n < 0 || offset + n >= size) return -1;
    offset += n;

    /* Print message queue statistics */
    n = snprintf(buffer + offset, size - offset,
                 "\nMessage Queue Statistics:\n"
                 "  Current Size:     %"PRIu64"\n"
                 "  Capacity:         %"PRIu64"\n",
                 msg_queue_size,
                 msg_queue_capacity);
    if (n < 0 || offset + n >= size) return -1;
    offset += n;

    /* Calculate queue usage percentage */
    double queue_usage = 0.0;
    if (msg_queue_capacity > 0) {
        queue_usage = (double) msg_queue_size / (double) msg_queue_capacity * 100.0;
    }
    n = snprintf(buffer + offset, size - offset,
                 "  Usage:            %.2f%%\n",
                 queue_usage);
    if (n < 0 || offset + n >= size) return -1;
    offset += n;

    /* Print worker statistics */
    int i;
    for (i = 0; i < metrics->worker_count; i++) {
        tglog_metrics *worker = metrics->workers[i];
        if (!worker) continue;

        // 读取worker metrics的值（原子操作）
        uint64_t w_request_msg_count = tglog_atomic_load_64(&worker->request_msg_count);
        uint64_t w_enqueued_msg_count = tglog_atomic_load_64(&worker->enqueued_msg_count);
        uint64_t w_dequeued_msg_count = tglog_atomic_load_64(&worker->dequeued_msg_count);
        uint64_t w_succeeded_msg_count = tglog_atomic_load_64(&worker->succeeded_msg_count);
        uint64_t w_failed_msg_count = tglog_atomic_load_64(&worker->failed_msg_count);
        uint64_t w_timeout_msg_count = tglog_atomic_load_64(&worker->timeout_msg_count);
        uint64_t w_retry_msg_count = tglog_atomic_load_64(&worker->retry_msg_count);
        uint64_t w_dropped_msg_count = tglog_atomic_load_64(&worker->dropped_msg_count);
        uint64_t w_eagain_err_count = tglog_atomic_load_64(&worker->eagain_err_count);
        uint64_t w_rescued_msg_bytes = tglog_atomic_load_64(&worker->rescued_msg_bytes);
        uint64_t w_opened_conn_count = tglog_atomic_load_64(&worker->opened_conn_count);
        uint64_t w_closed_conn_count = tglog_atomic_load_64(&worker->closed_conn_count);
        uint64_t w_error_conn_count = tglog_atomic_load_64(&worker->error_conn_count);
        uint64_t w_endpoint_count = tglog_atomic_load_64(&worker->endpoint_count);
        uint64_t w_add_endpoint_count = tglog_atomic_load_64(&worker->add_endpoint_count);
        uint64_t w_remove_endpoint_count = tglog_atomic_load_64(&worker->remove_endpoint_count);
        uint64_t w_msg_queue_size = tglog_atomic_load_64(&worker->msg_queue_size);
        uint64_t w_msg_queue_capacity = tglog_atomic_load_64(&worker->msg_queue_capacity);

        n = snprintf(buffer + offset, size - offset,
                     "\n=== Worker %d Statistics ===\n", i);
        if (n < 0 || offset + n >= size) return -1;
        offset += n;

        n = snprintf(buffer + offset, size - offset,
                     "\nMessage Statistics:\n"
                     "  Request Count:    %"PRIu64"\n"
                     "  Enqueued Count:   %"PRIu64"\n"
                     "  Dequeued Count:   %"PRIu64"\n"
                     "  Succeeded Count:  %"PRIu64"\n"
                     "  Failed Count:     %"PRIu64"\n"
                     "  Timeout Count:    %"PRIu64"\n"
                     "  Retry Count:      %"PRIu64"\n"
                     "  Dropped Count:    %"PRIu64"\n"
                     "  EAGAIN Errors:    %"PRIu64"\n"
                     "  Rescued Bytes:    %"PRIu64"\n",
                     w_request_msg_count,
                     w_enqueued_msg_count,
                     w_dequeued_msg_count,
                     w_succeeded_msg_count,
                     w_failed_msg_count,
                     w_timeout_msg_count,
                     w_retry_msg_count,
                     w_dropped_msg_count,
                     w_eagain_err_count,
                     w_rescued_msg_bytes);
        if (n < 0 || offset + n >= size) return -1;
        offset += n;

        n = snprintf(buffer + offset, size - offset,
                     "\nConnection Statistics:\n"
                     "  Opened Count:     %"PRIu64"\n"
                     "  Closed Count:     %"PRIu64"\n"
                     "  Error Count:      %"PRIu64"\n",
                     w_opened_conn_count,
                     w_closed_conn_count,
                     w_error_conn_count);
        if (n < 0 || offset + n >= size) return -1;
        offset += n;

        n = snprintf(buffer + offset, size - offset,
                     "\nEndpoint Statistics:\n"
                     "  Current Count:    %"PRIu64"\n"
                     "  Added Count:      %"PRIu64"\n"
                     "  Removed Count:    %"PRIu64"\n",
                     w_endpoint_count,
                     w_add_endpoint_count,
                     w_remove_endpoint_count);
        if (n < 0 || offset + n >= size) return -1;
        offset += n;

        n = snprintf(buffer + offset, size - offset,
                     "\nMessage Queue Statistics:\n"
                     "  Current Size:     %"PRIu64"\n"
                     "  Capacity:         %"PRIu64"\n",
                     w_msg_queue_size,
                     w_msg_queue_capacity);
        if (n < 0 || offset + n >= size) return -1;
        offset += n;

        double w_queue_usage = 0.0;
        if (w_msg_queue_capacity > 0) {
            w_queue_usage = (double) w_msg_queue_size / (double) w_msg_queue_capacity * 100.0;
        }
        n = snprintf(buffer + offset, size - offset,
                     "  Usage:            %.2f%%\n",
                     w_queue_usage);
        if (n < 0 || offset + n >= size) return -1;
        offset += n;
    }

    return (int) offset;
}

int tglog_metrics_print_json(tglog_metrics *metrics, char *buffer, size_t size) {
    if (!metrics || !buffer || size == 0) {
        return -1;
    }

    size_t offset = 0;
    int n;

    /* Aggregate per-worker counters; load gauges directly. */
    tglog_metrics agg;
    memset(&agg, 0, sizeof(agg));
    tglog_metrics_aggregate_counters(metrics, &agg);

    uint64_t request_msg_count = agg.request_msg_count;
    uint64_t enqueued_msg_count = agg.enqueued_msg_count;
    uint64_t dequeued_msg_count = agg.dequeued_msg_count;
    uint64_t succeeded_msg_count = agg.succeeded_msg_count;
    uint64_t failed_msg_count = agg.failed_msg_count;
    uint64_t timeout_msg_count = agg.timeout_msg_count;
    uint64_t retry_msg_count = agg.retry_msg_count;
    uint64_t dropped_msg_count = agg.dropped_msg_count;
    uint64_t eagain_err_count = agg.eagain_err_count;
    uint64_t rescued_msg_bytes = agg.rescued_msg_bytes;
    uint64_t opened_conn_count = agg.opened_conn_count;
    uint64_t closed_conn_count = agg.closed_conn_count;
    uint64_t error_conn_count = agg.error_conn_count;
    uint64_t add_endpoint_count = agg.add_endpoint_count;
    uint64_t remove_endpoint_count = agg.remove_endpoint_count;
    uint64_t endpoint_count = tglog_atomic_load_64(&metrics->endpoint_count);
    uint64_t msg_queue_size = tglog_atomic_load_64(&metrics->msg_queue_size);
    uint64_t msg_queue_capacity = tglog_atomic_load_64(&metrics->msg_queue_capacity);

    // 开始打印JSON
    n = snprintf(buffer + offset, size - offset, "{\n");
    if (n < 0 || offset + n >= size) return -1;
    offset += n;

    // 打印主metrics字段
    n = snprintf(buffer + offset, size - offset, "  \"request_msg_count\": %"PRIu64",\n", request_msg_count);
    if (n < 0 || offset + n >= size) return -1;
    offset += n;

    n = snprintf(buffer + offset, size - offset, "  \"enqueued_msg_count\": %"PRIu64",\n", enqueued_msg_count);
    if (n < 0 || offset + n >= size) return -1;
    offset += n;

    n = snprintf(buffer + offset, size - offset, "  \"dequeued_msg_count\": %"PRIu64",\n", dequeued_msg_count);
    if (n < 0 || offset + n >= size) return -1;
    offset += n;

    n = snprintf(buffer + offset, size - offset, "  \"succeeded_msg_count\": %"PRIu64",\n", succeeded_msg_count);
    if (n < 0 || offset + n >= size) return -1;
    offset += n;

    n = snprintf(buffer + offset, size - offset, "  \"failed_msg_count\": %"PRIu64",\n", failed_msg_count);
    if (n < 0 || offset + n >= size) return -1;
    offset += n;

    n = snprintf(buffer + offset, size - offset, "  \"timeout_msg_count\": %"PRIu64",\n", timeout_msg_count);
    if (n < 0 || offset + n >= size) return -1;
    offset += n;

    n = snprintf(buffer + offset, size - offset, "  \"retry_msg_count\": %"PRIu64",\n", retry_msg_count);
    if (n < 0 || offset + n >= size) return -1;
    offset += n;

    n = snprintf(buffer + offset, size - offset, "  \"dropped_msg_count\": %"PRIu64",\n", dropped_msg_count);
    if (n < 0 || offset + n >= size) return -1;
    offset += n;

    n = snprintf(buffer + offset, size - offset, "  \"eagain_err_count\": %"PRIu64",\n", eagain_err_count);
    if (n < 0 || offset + n >= size) return -1;
    offset += n;

    n = snprintf(buffer + offset, size - offset, "  \"rescued_msg_bytes\": %"PRIu64",\n", rescued_msg_bytes);
    if (n < 0 || offset + n >= size) return -1;
    offset += n;

    n = snprintf(buffer + offset, size - offset, "  \"opened_conn_count\": %"PRIu64",\n", opened_conn_count);
    if (n < 0 || offset + n >= size) return -1;
    offset += n;

    n = snprintf(buffer + offset, size - offset, "  \"closed_conn_count\": %"PRIu64",\n", closed_conn_count);
    if (n < 0 || offset + n >= size) return -1;
    offset += n;

    n = snprintf(buffer + offset, size - offset, "  \"error_conn_count\": %"PRIu64",\n", error_conn_count);
    if (n < 0 || offset + n >= size) return -1;
    offset += n;

    n = snprintf(buffer + offset, size - offset, "  \"endpoint_count\": %"PRIu64",\n", endpoint_count);
    if (n < 0 || offset + n >= size) return -1;
    offset += n;

    n = snprintf(buffer + offset, size - offset, "  \"add_endpoint_count\": %"PRIu64",\n", add_endpoint_count);
    if (n < 0 || offset + n >= size) return -1;
    offset += n;

    n = snprintf(buffer + offset, size - offset, "  \"remove_endpoint_count\": %"PRIu64",\n", remove_endpoint_count);
    if (n < 0 || offset + n >= size) return -1;
    offset += n;

    n = snprintf(buffer + offset, size - offset, "  \"msg_queue_size\": %"PRIu64",\n", msg_queue_size);
    if (n < 0 || offset + n >= size) return -1;
    offset += n;

    n = snprintf(buffer + offset, size - offset, "  \"msg_queue_capacity\": %"PRIu64",\n", msg_queue_capacity);
    if (n < 0 || offset + n >= size) return -1;
    offset += n;

    // 打印workers对象
    n = snprintf(buffer + offset, size - offset, "  \"workers\": {\n");
    if (n < 0 || offset + n >= size) return -1;
    offset += n;

    int i = 0;
    for (i = 0; i < metrics->worker_count; i++) {
        tglog_metrics *worker = metrics->workers[i];
        if (!worker) continue;

        // 读取worker metrics的值（原子操作）
        uint64_t w_request_msg_count = tglog_atomic_load_64(&worker->request_msg_count);
        uint64_t w_enqueued_msg_count = tglog_atomic_load_64(&worker->enqueued_msg_count);
        uint64_t w_dequeued_msg_count = tglog_atomic_load_64(&worker->dequeued_msg_count);
        uint64_t w_succeeded_msg_count = tglog_atomic_load_64(&worker->succeeded_msg_count);
        uint64_t w_failed_msg_count = tglog_atomic_load_64(&worker->failed_msg_count);
        uint64_t w_timeout_msg_count = tglog_atomic_load_64(&worker->timeout_msg_count);
        uint64_t w_retry_msg_count = tglog_atomic_load_64(&worker->retry_msg_count);
        uint64_t w_dropped_msg_count = tglog_atomic_load_64(&worker->dropped_msg_count);
        uint64_t w_eagain_err_count = tglog_atomic_load_64(&worker->eagain_err_count);
        uint64_t w_rescued_msg_bytes = tglog_atomic_load_64(&worker->rescued_msg_bytes);
        uint64_t w_opened_conn_count = tglog_atomic_load_64(&worker->opened_conn_count);
        uint64_t w_closed_conn_count = tglog_atomic_load_64(&worker->closed_conn_count);
        uint64_t w_error_conn_count = tglog_atomic_load_64(&worker->error_conn_count);
        uint64_t w_endpoint_count = tglog_atomic_load_64(&worker->endpoint_count);
        uint64_t w_add_endpoint_count = tglog_atomic_load_64(&worker->add_endpoint_count);
        uint64_t w_remove_endpoint_count = tglog_atomic_load_64(&worker->remove_endpoint_count);
        uint64_t w_msg_queue_size = tglog_atomic_load_64(&worker->msg_queue_size);
        uint64_t w_msg_queue_capacity = tglog_atomic_load_64(&worker->msg_queue_capacity);

        // 打印worker对象（使用下标作为key）
        n = snprintf(buffer + offset, size - offset, "    \"%d\": {\n", i);
        if (n < 0 || offset + n >= size) return -1;
        offset += n;

        n = snprintf(buffer + offset, size - offset, "      \"request_msg_count\": %"PRIu64",\n", w_request_msg_count);
        if (n < 0 || offset + n >= size) return -1;
        offset += n;

        n = snprintf(buffer + offset, size - offset, "      \"enqueued_msg_count\": %"PRIu64",\n", w_enqueued_msg_count);
        if (n < 0 || offset + n >= size) return -1;
        offset += n;

        n = snprintf(buffer + offset, size - offset, "      \"dequeued_msg_count\": %"PRIu64",\n", w_dequeued_msg_count);
        if (n < 0 || offset + n >= size) return -1;
        offset += n;

        n = snprintf(buffer + offset, size - offset, "      \"succeeded_msg_count\": %"PRIu64",\n", w_succeeded_msg_count);
        if (n < 0 || offset + n >= size) return -1;
        offset += n;

        n = snprintf(buffer + offset, size - offset, "      \"failed_msg_count\": %"PRIu64",\n", w_failed_msg_count);
        if (n < 0 || offset + n >= size) return -1;
        offset += n;

        n = snprintf(buffer + offset, size - offset, "      \"timeout_msg_count\": %"PRIu64",\n", w_timeout_msg_count);
        if (n < 0 || offset + n >= size) return -1;
        offset += n;

        n = snprintf(buffer + offset, size - offset, "      \"retry_msg_count\": %"PRIu64",\n", w_retry_msg_count);
        if (n < 0 || offset + n >= size) return -1;
        offset += n;

        n = snprintf(buffer + offset, size - offset, "      \"dropped_msg_count\": %"PRIu64",\n", w_dropped_msg_count);
        if (n < 0 || offset + n >= size) return -1;
        offset += n;

        n = snprintf(buffer + offset, size - offset, "      \"eagain_err_count\": %"PRIu64",\n", w_eagain_err_count);
        if (n < 0 || offset + n >= size) return -1;
        offset += n;

        n = snprintf(buffer + offset, size - offset, "      \"rescued_msg_bytes\": %"PRIu64",\n", w_rescued_msg_bytes);
        if (n < 0 || offset + n >= size) return -1;
        offset += n;

        n = snprintf(buffer + offset, size - offset, "      \"opened_conn_count\": %"PRIu64",\n", w_opened_conn_count);
        if (n < 0 || offset + n >= size) return -1;
        offset += n;

        n = snprintf(buffer + offset, size - offset, "      \"closed_conn_count\": %"PRIu64",\n", w_closed_conn_count);
        if (n < 0 || offset + n >= size) return -1;
        offset += n;

        n = snprintf(buffer + offset, size - offset, "      \"error_conn_count\": %"PRIu64",\n", w_error_conn_count);
        if (n < 0 || offset + n >= size) return -1;
        offset += n;

        n = snprintf(buffer + offset, size - offset, "      \"endpoint_count\": %"PRIu64",\n", w_endpoint_count);
        if (n < 0 || offset + n >= size) return -1;
        offset += n;

        n = snprintf(buffer + offset, size - offset, "      \"add_endpoint_count\": %"PRIu64",\n", w_add_endpoint_count);
        if (n < 0 || offset + n >= size) return -1;
        offset += n;

        n = snprintf(buffer + offset, size - offset, "      \"remove_endpoint_count\": %"PRIu64",\n", w_remove_endpoint_count);
        if (n < 0 || offset + n >= size) return -1;
        offset += n;

        n = snprintf(buffer + offset, size - offset, "      \"msg_queue_size\": %"PRIu64",\n", w_msg_queue_size);
        if (n < 0 || offset + n >= size) return -1;
        offset += n;

        n = snprintf(buffer + offset, size - offset, "      \"msg_queue_capacity\": %"PRIu64"\n", w_msg_queue_capacity);
        if (n < 0 || offset + n >= size) return -1;
        offset += n;

        // 处理逗号（最后一个worker不加逗号）
        if (i < metrics->worker_count - 1) {
            n = snprintf(buffer + offset, size - offset, "    },\n");
        } else {
            n = snprintf(buffer + offset, size - offset, "    }\n");
        }
        if (n < 0 || offset + n >= size) return -1;
        offset += n;
    }

    n = snprintf(buffer + offset, size - offset, "  }\n");
    if (n < 0 || offset + n >= size) return -1;
    offset += n;

    n = snprintf(buffer + offset, size - offset, "}\n");
    if (n < 0 || offset + n >= size) return -1;
    offset += n;

    return (int) offset;
}