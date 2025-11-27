/* Standard library headers */
#include <stdlib.h>
#include <string.h>

/* Project headers */
#include "metrics.h"

tglog_metrics *tglog_metrics_create(void) {
    tglog_metrics *metrics = (tglog_metrics *) calloc(1, sizeof(tglog_metrics));
    if (!metrics) {
        return NULL;
    }

    /* All fields are initialized to 0 by calloc */
    return metrics;
}

void tglog_metrics_destroy(tglog_metrics *metrics) {
    if (metrics) {
        free(metrics);
    }
}

/* Single-threaded version: no atomic operations needed */

void tglog_metrics_inc_request_msg_count(tglog_metrics *metrics, int count) {
    if (metrics) {
        metrics->request_msg_count += count;
    }
}

void tglog_metrics_inc_enqueue_msg_count(tglog_metrics *metrics, int count) {
    if (metrics) {
        metrics->enqueued_msg_count += count;
    }
}

void tglog_metrics_inc_dequeue_msg_count(tglog_metrics *metrics, int count) {
    if (metrics) {
        metrics->dequeued_msg_count += count;
    }
}

void tglog_metrics_inc_succeeded_msg_count(tglog_metrics *metrics, int count) {
    if (metrics) {
        metrics->succeeded_msg_count += count;
    }
}

void tglog_metrics_inc_failed_msg_count(tglog_metrics *metrics, int count) {
    if (metrics) {
        metrics->failed_msg_count += count;
    }
}

void tglog_metrics_inc_timeout_msg_count(tglog_metrics *metrics, int count) {
    if (metrics) {
        metrics->timeout_msg_count += count;
    }
}

void tglog_metrics_inc_retry_msg_count(tglog_metrics *metrics, int count) {
    if (metrics) {
        metrics->retry_msg_count += count;
    }
}

void tglog_metrics_inc_dropped_msg_count(tglog_metrics *metrics, int count) {
    if (metrics) {
        metrics->dropped_msg_count += count;
    }
}

void tglog_metrics_inc_eagain_err_count(tglog_metrics *metrics, int count) {
    if (metrics) {
        metrics->eagain_err_count += count;
    }
}

void tglog_metrics_inc_opened_conn_count(tglog_metrics *metrics, int count) {
    if (metrics) {
        metrics->opened_conn_count += count;
    }
}

void tglog_metrics_inc_closed_conn_count(tglog_metrics *metrics, int count) {
    if (metrics) {
        metrics->closed_conn_count += count;
    }
}

void tglog_metrics_inc_error_conn_count(tglog_metrics *metrics, int count) {
    if (metrics) {
        metrics->error_conn_count += count;
    }
}

void tglog_metrics_set_endpoint_count(tglog_metrics *metrics, uint64_t count) {
    if (metrics) {
        metrics->endpoint_count = count;
    }
}

void tglog_metrics_inc_add_endpoint_count(tglog_metrics *metrics, int count) {
    if (metrics) {
        metrics->add_endpoint_count += count;
    }
}

void tglog_metrics_inc_remove_endpoint_count(tglog_metrics *metrics, int count) {
    if (metrics) {
        metrics->remove_endpoint_count += count;
    }
}

void tglog_metrics_set_msg_queue_size(tglog_metrics *metrics, uint64_t size) {
    if (metrics) {
        metrics->msg_queue_size = size;
    }
}

void tglog_metrics_set_msg_queue_capacity(tglog_metrics *metrics, uint64_t capacity) {
    if (metrics) {
        metrics->msg_queue_capacity = capacity;
    }
}

void tglog_metrics_inc_total_update_count(tglog_metrics *metrics, int count) {
    if (metrics) {
        metrics->total_update_count += count;
    }
}

void tglog_metrics_inc_timeout_update_count(tglog_metrics *metrics, int count) {
    if (metrics) {
        metrics->timeout_update_count += count;
    }
}

void tglog_metrics_inc_idle_update_count(tglog_metrics *metrics, int count) {
    if (metrics) {
        metrics->idle_update_count += count;
    }
}

void tglog_metrics_set_max_update_time(tglog_metrics *metrics, uint64_t time) {
    if (metrics) {
        if (time > metrics->max_update_time_us) {
            metrics->max_update_time_us = time;
        }
    }
}

void tglog_metrics_set_limit_update_time(tglog_metrics *metrics, uint64_t time){
    if (metrics) {
        metrics->limit_update_time_us = time;
    }
}

void tglog_metrics_set_max_event_dispatch_time(tglog_metrics *metrics, uint64_t time) {
    if (metrics) {
        if (time > metrics->max_event_dispatch_time_us) {
            metrics->max_event_dispatch_time_us = time;
        }
    }
}

void tglog_metrics_set_max_msg_dispatch_time(tglog_metrics *metrics, uint64_t time) {
    if (metrics) {
        if (time > metrics->max_msg_dispatch_time_us) {
            metrics->max_msg_dispatch_time_us = time;
        }
    }
}

void tglog_metrics_set_max_msg_dispatch_count(tglog_metrics *metrics, uint64_t count) {
    if (metrics) {
        if (count > metrics->max_msg_dispatch_count) {
            metrics->max_msg_dispatch_count = count;
        }
    }
}

int tglog_metrics_print(tglog_metrics *metrics, char *buffer, size_t size) {
    if (!metrics || !buffer || size == 0) {
        return -1;
    }

    size_t offset = 0;
    int n;

    /* Print header */
    n = snprintf(buffer + offset, size - offset,
                 "\n=== TGLog Metrics Statistics ===\n");

    if (n < 0 || offset + n >= size) {
        return -1;
    }
    offset += n;

    /* Print message statistics */
    n = snprintf(buffer + offset, size - offset,
                 "\nMessage Statistics:\n"
                 "  Request Count:    %llu\n"
                 "  Enqueued Count:   %llu\n"
                 "  Dequeued Count:   %llu\n"
                 "  Succeeded Count:  %llu\n"
                 "  Failed Count:     %llu\n"
                 "  Timeout Count:    %llu\n"
                 "  Retry Count:      %llu\n"
                 "  Dropped Count:    %llu\n"
                 "  EAGAIN Errors:    %llu\n",
                 (unsigned long long) metrics->request_msg_count,
                 (unsigned long long) metrics->enqueued_msg_count,
                 (unsigned long long) metrics->dequeued_msg_count,
                 (unsigned long long) metrics->succeeded_msg_count,
                 (unsigned long long) metrics->failed_msg_count,
                 (unsigned long long) metrics->timeout_msg_count,
                 (unsigned long long) metrics->retry_msg_count,
                 (unsigned long long) metrics->dropped_msg_count,
                 (unsigned long long) metrics->eagain_err_count);

    if (n < 0 || offset + n >= size) {
        return -1;
    }
    offset += n;

    /* Print connection statistics */
    n = snprintf(buffer + offset, size - offset,
                 "\nConnection Statistics:\n"
                 "  Opened Count:     %llu\n"
                 "  Closed Count:     %llu\n"
                 "  Error Count:      %llu\n",
                 (unsigned long long) metrics->opened_conn_count,
                 (unsigned long long) metrics->closed_conn_count,
                 (unsigned long long) metrics->error_conn_count);

    if (n < 0 || offset + n >= size) {
        return -1;
    }
    offset += n;

    /* Print endpoint statistics */
    n = snprintf(buffer + offset, size - offset,
                 "\nEndpoint Statistics:\n"
                 "  Current Count:    %llu\n"
                 "  Added Count:      %llu\n"
                 "  Removed Count:    %llu\n",
                 (unsigned long long) metrics->endpoint_count,
                 (unsigned long long) metrics->add_endpoint_count,
                 (unsigned long long) metrics->remove_endpoint_count);

    if (n < 0 || offset + n >= size) {
        return -1;
    }
    offset += n;

    /* Print message queue statistics */
    n = snprintf(buffer + offset, size - offset,
                 "\nMessage Queue Statistics:\n"
                 "  Current Size:     %llu\n"
                 "  Capacity:         %llu\n",
                 (unsigned long long) metrics->msg_queue_size,
                 (unsigned long long) metrics->msg_queue_capacity);

    if (n < 0 || offset + n >= size) {
        return -1;
    }
    offset += n;

    /* Calculate queue usage percentage */
    double queue_usage = 0.0;
    if (metrics->msg_queue_capacity > 0) {
        queue_usage = (double) metrics->msg_queue_size / metrics->msg_queue_capacity * 100.0;
    }

    n = snprintf(buffer + offset, size - offset,
                 "  Usage:            %.2f%%\n",
                 queue_usage);

    if (n < 0 || offset + n >= size) {
        return -1;
    }
    offset += n;

    /* Print update statistics */
    n = snprintf(buffer + offset, size - offset,
                 "\nUpdate Statistics:\n"
                 "  Total Updates:              %llu\n"
                 "  Timeout Updates:            %llu\n"
                 "  Idle Updates:               %llu\n"
                 "  Max Update Time:            %llu us\n"
                 "  Limit Update Time:          %llu us\n"
                 "  Max Event Dispatch Time:    %llu us\n"
                 "  Max Msg Dispatch Time:      %llu us\n"
                 "  Max Msg Dispatch:           %llu\n\n",
                 (unsigned long long) metrics->total_update_count,
                 (unsigned long long) metrics->timeout_update_count,
                 (unsigned long long) metrics->idle_update_count,
                 (unsigned long long) metrics->max_update_time_us,
                 (unsigned long long) metrics->limit_update_time_us,
                 (unsigned long long) metrics->max_event_dispatch_time_us,
                 (unsigned long long) metrics->max_msg_dispatch_time_us,
                 (unsigned long long) metrics->max_msg_dispatch_count);

    if (n < 0 || offset + n >= size) {
        return -1;
    }
    offset += n;

    return (int)offset;
}

int tglog_metrics_print_json(tglog_metrics *metrics, char* buffer, size_t size) {
    if (!metrics || !buffer || size == 0) {
        return -1;
    }

    int n = snprintf(buffer, size,
                     "{\n"
                     "  \"request_msg_count\": %llu,\n"
                     "  \"enqueued_msg_count\": %llu,\n"
                     "  \"dequeued_msg_count\": %llu,\n"
                     "  \"succeeded_msg_count\": %llu,\n"
                     "  \"failed_msg_count\": %llu,\n"
                     "  \"timeout_msg_count\": %llu,\n"
                     "  \"retry_msg_count\": %llu,\n"
                     "  \"dropped_msg_count\": %llu,\n"
                     "  \"eagain_err_count\": %llu,\n"
                     "  \"opened_conn_count\": %llu,\n"
                     "  \"closed_conn_count\": %llu,\n"
                     "  \"error_conn_count\": %llu,\n"
                     "  \"endpoint_count\": %llu,\n"
                     "  \"add_endpoint_count\": %llu,\n"
                     "  \"remove_endpoint_count\": %llu,\n"
                     "  \"msg_queue_size\": %llu,\n"
                     "  \"msg_queue_capacity\": %llu,\n"
                     "  \"total_update_count\": %llu,\n"
                     "  \"timeout_update_count\": %llu,\n"
                     "  \"idle_update_count\": %llu,\n"
                     "  \"max_update_time_us\": %llu,\n"
                     "  \"limit_update_time_us\": %llu,\n"
                     "  \"max_event_dispatch_time_us\": %llu,\n"
                     "  \"max_msg_dispatch_time_us\": %llu,\n"
                     "  \"max_msg_dispatch_count\": %llu\n"
                     "}",
                     (unsigned long long) metrics->request_msg_count,
                     (unsigned long long) metrics->enqueued_msg_count,
                     (unsigned long long) metrics->dequeued_msg_count,
                     (unsigned long long) metrics->succeeded_msg_count,
                     (unsigned long long) metrics->failed_msg_count,
                     (unsigned long long) metrics->timeout_msg_count,
                     (unsigned long long) metrics->retry_msg_count,
                     (unsigned long long) metrics->dropped_msg_count,
                     (unsigned long long) metrics->eagain_err_count,
                     (unsigned long long) metrics->opened_conn_count,
                     (unsigned long long) metrics->closed_conn_count,
                     (unsigned long long) metrics->error_conn_count,
                     (unsigned long long) metrics->endpoint_count,
                     (unsigned long long) metrics->add_endpoint_count,
                     (unsigned long long) metrics->remove_endpoint_count,
                     (unsigned long long) metrics->msg_queue_size,
                     (unsigned long long) metrics->msg_queue_capacity,
                     (unsigned long long) metrics->total_update_count,
                     (unsigned long long) metrics->timeout_update_count,
                     (unsigned long long) metrics->idle_update_count,
                     (unsigned long long) metrics->max_update_time_us,
                     (unsigned long long) metrics->limit_update_time_us,
                     (unsigned long long) metrics->max_event_dispatch_time_us,
                     (unsigned long long) metrics->max_msg_dispatch_time_us,
                     (unsigned long long) metrics->max_msg_dispatch_count);

    if (n < 0 || (size_t) n >= size) {
        return -1;
    }

    return n;
}
