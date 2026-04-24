/* Standard library headers */
#include <math.h>
#include <stdlib.h>

/* System headers */
#include "tglog_platform.h"

/* Project headers */
#include "request.h"
#include "endpoint.h"
#include "err.h"
#include "metrics.h"
#include "pool.h"


tglog_message *tglog_message_create(tglog_mem_pool *mem_pool, const char *payload, size_t len,
                                    tglog_msg_callback callback, void *user_data,
                                    tglog_msg_flag flag, tglog_metrics *metrics, int worker_id) {
    if (!payload || len == 0) {
        return NULL;
    }

    tglog_message *msg = NULL;
    if (mem_pool) {
        msg = tglog_message_pool_get(mem_pool->msg_pool);
    }
    if (!msg) {
        msg = (tglog_message *) calloc(1, sizeof(tglog_message));
    }
    if (!msg) {
        return NULL;
    }

    /* Copy log payload if required */
    if (flag == TGLOG_MSG_F_COPY) {
        char *payload_buf = NULL;
        size_t payload_buf_len = len + 1; /* +1 for \0 */
        if (mem_pool) {
            tglog_payload_pool *payload_pool = tglog_payload_pools_get_pool(mem_pool->payload_pools, len + 1);
            if (payload_pool) {
                tglog_payload *payload_entry = tglog_payload_pool_get(payload_pool);
                if (payload_entry) {
                    payload_buf = payload_entry->data;
                    msg->payload_pool = payload_pool;
                }
            }
        }

        if (!payload_buf) {
            payload_buf = (char *) malloc(payload_buf_len);
        }

        if (!payload_buf) {
            tglog_message_destroy(msg);
            return NULL;
        }

        msg->payload = payload_buf;
        memcpy(msg->payload, payload, len);
        msg->payload[len] = '\0';
    } else {
        msg->payload = (char *) payload;
    }

    msg->payload_len = len;
    msg->flag = flag;
    msg->callback = callback;
    msg->user_data = user_data;
    msg->metrics = metrics;
    msg->worker_id = worker_id;

    return msg;
}

size_t tglog_message_len(tglog_message *msg) {
    if (!msg) {
        return 0;
    }

    return msg->payload_len;
}

static void tglog_message_reset(tglog_message *msg) {
    msg->payload = NULL;
    msg->payload_len = 0;
    msg->callback = NULL;
    msg->user_data = NULL;
    msg->metrics = NULL;
    msg->worker_id = 0;
    msg->flag = 0;
    msg->payload_pool = NULL;
    msg->pool = NULL;
}

void tglog_message_destroy(tglog_message *msg) {
    if (!msg) {
        return;
    }

    if ((msg->flag == TGLOG_MSG_F_FREE) || (msg->flag == TGLOG_MSG_F_COPY)) {
        tglog_payload_pool *payload_pool = msg->payload_pool;
        if (payload_pool) {
            tglog_payload_pool_put_buf(payload_pool, msg->payload);
        } else {
            free(msg->payload);
        }
    }

    tglog_message_pool *pool = msg->pool;
    if (pool) {
        tglog_message_reset(msg);
        tglog_message_pool_put(pool, msg);
    } else {
        free(msg);
    }
}

void tglog_message_done(tglog_message *msg, int err_code) {
    if (!msg) {
        return;
    }

    if (msg->callback) {
        msg->callback(msg->payload, err_code, tglog_get_err_msg(err_code), msg->user_data);
    }

    if (err_code == TGLOG_ERR_OK) {
        tglog_metrics_inc_succeeded_msg_count(msg->metrics, 1);
    } else {
        tglog_metrics_inc_failed_msg_count(msg->metrics, 1);
    }

    if (err_code == TGLOG_ERR_SEND_TIMEOUT) {
        tglog_metrics_inc_timeout_msg_count(msg->metrics, 1);
    }

    tglog_message_destroy(msg);
}

tglog_batch *tglog_batch_create(int capacity) {
    if (capacity <= 0) {
        return NULL;
    }

    tglog_batch *batch = (tglog_batch *) calloc(1, sizeof(tglog_batch));
    if (!batch) {
        return NULL;
    }

    batch->capacity = capacity;
    batch->msg_count = 0;
    batch->len = 0;
    batch->buffer = NULL;
    batch->buffer_len = 0;
    batch->iov = NULL;
    batch->iov_count = 0;
    batch->msgs = (tglog_message **) calloc(capacity, sizeof(tglog_message *));
    if (!batch->msgs) {
        free(batch);
        return NULL;
    }

    gettimeofday(&batch->time, NULL);
    return batch;
}

void tglog_batch_destroy(tglog_batch *batch) {
    if (!batch) {
        return;
    }

    int i = 0;
    for (i = 0; i < batch->msg_count; i++) {
        tglog_message_destroy(batch->msgs[i]);
    }

    free(batch->msgs);
    free(batch);
}

void tglog_batch_reset(tglog_batch *batch) {
    if (!batch) {
        return;
    }

    batch->msg_count = 0;
    batch->len = 0;
    batch->buffer = NULL;
    batch->buffer_len = 0;
    batch->iov = NULL;
    batch->iov_count = 0;
    gettimeofday(&batch->time, NULL);
    batch->sent_index = 0;
    batch->retries = 0;
    batch->retry_start_time.tv_sec = 0;
    batch->retry_start_time.tv_usec = 0;
    batch->retry_deadline.tv_sec = 0;
    batch->retry_deadline.tv_usec = 0;
}

bool tglog_batch_add(tglog_batch *batch, tglog_message *msg) {
    if (!batch || !msg) {
        return false;
    }

    if (batch->msg_count >= batch->capacity) {
        return false;
    }

    batch->msgs[batch->msg_count++] = msg;
    batch->len += tglog_message_len(msg);
    if (batch->msg_count == 1) {
        gettimeofday(&batch->time, NULL);
    }
    return true;
}

void tglog_batch_done(tglog_batch *batch, int err_code) {
    if (!batch) {
        return;
    }

    int i = 0;
    for (i = 0; i < batch->msg_count; i++) {
        tglog_message *msg = batch->msgs[i];
        if (batch->sent_index > 0 && i < batch->sent_index) {
            tglog_message_done(msg, TGLOG_ERR_OK);
            continue;
        }

        tglog_message_done(msg, err_code);
    }

    tglog_batch_reset(batch);
}

bool tglog_batch_can_send_ignore_timeout(tglog_batch *batch, int max_msg, int max_size) {
    if (!batch) {
        return false;
    }

    return batch->msg_count >= max_msg || (int) batch->len >= max_size || batch->msg_count >= batch->capacity;
}

bool tglog_batch_can_send(tglog_batch *batch, int max_msg, int max_size, int max_delay) {
    if (!batch) {
        return false;
    }

    if (tglog_batch_can_send_ignore_timeout(batch, max_msg, max_size)) {
        return true;
    }

    struct timeval now;
    gettimeofday(&now, NULL);
    int64_t elapsed = (int64_t)(now.tv_sec - batch->time.tv_sec) * 1000 + (now.tv_usec - batch->time.tv_usec) / 1000;

    return (batch->msg_count > 0 && elapsed >= max_delay);
}

size_t tglog_batch_len(tglog_batch *batch) {
    if (!batch) {
        return 0;
    }

    return batch->len;
}

unsigned int tglog_backoff_next_interval(tglog_backoff *b, int retry_count, bool bound_max, unsigned int *rand_seed) {
    if (retry_count <= 0) {
        return b->initial_interval;
    }

    double interval = b->initial_interval * pow(b->multiplier, retry_count - 1);
    if (b->randomization > 0) {
        double random_factor = (rand_r(rand_seed) / (double) RAND_MAX) * 2 - 1;
        interval = interval * (1 + b->randomization * random_factor);
    }

    if (bound_max && interval > b->max_interval) {
        interval = b->max_interval;
    }

    return (unsigned int) interval;
}

unsigned int tglog_backoff_get_total_backoff_time(tglog_backoff *b, int retry_count, unsigned int *rand_seed){
    if (retry_count <= 0) {
        return 0;
    }

    unsigned int total = 0;
    for (int i = 1; i <= retry_count; i++) {
        total += tglog_backoff_next_interval(b, i, true, rand_seed);
    }
    return total;
}

tglog_endpoint_notify *tglog_endpoint_notify_create(const tglog_endpoint_list *all, const tglog_endpoint_list *added, const tglog_endpoint_list *removed) {
    /* Create an endpoint notify */
    tglog_endpoint_notify *epn = (tglog_endpoint_notify *) malloc(sizeof(tglog_endpoint_notify));
    if (!epn) {
        return NULL;
    }

    /* Make copies of all the endpoint lists */
    epn->all = tglog_endpoint_list_copy(all);
    epn->added = tglog_endpoint_list_copy(added);
    epn->removed = tglog_endpoint_list_copy(removed);
    return epn;
}

void tglog_endpoint_notify_destroy(tglog_endpoint_notify *epn) {
    if (!epn) {
        return;
    }

    tglog_endpoint_list_destroy(epn->all);
    tglog_endpoint_list_destroy(epn->added);
    tglog_endpoint_list_destroy(epn->removed);
    free(epn);
}

uint64_t tglog_get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t) tv.tv_sec * 1000 + (uint64_t) tv.tv_usec / 1000;
}

uint64_t tglog_get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t) tv.tv_sec * 1000000 + (uint64_t) tv.tv_usec;
}

int64_t tglog_time_since_ms(struct timeval *tv) {
    struct timeval now;
    gettimeofday(&now, NULL);
    return (int64_t)(now.tv_sec - tv->tv_sec) * 1000 + (now.tv_usec - tv->tv_usec) / 1000;
}
