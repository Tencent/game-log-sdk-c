#ifndef TGLOG_REQUEST_H
#define TGLOG_REQUEST_H

/* Standard library headers */
#include <time.h>

/* System headers */
#include <sys/uio.h>

/* Third-party library headers */
#include "ck_ring.h"
#include "ck_stack.h"

/* Project headers */
#include "def.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct tglog_metrics tglog_metrics;
typedef struct tglog_payload_pool tglog_payload_pool;
typedef struct tglog_message_pool tglog_message_pool;
typedef struct tglog_mem_pool tglog_mem_pool;

/* Callback function type for message completion, called when message is processed, equals to `tglog_send_callback` */
typedef void (*tglog_msg_callback)(const char *payload, int err_code, const char *err_msg, void *user_data);

typedef struct tglog_message {
    char *payload;                      /* log payload */
    size_t payload_len;                 /* length of the payload */
    tglog_msg_callback callback;        /* callback function for message completion */
    void *user_data;                    /* user data to pass to the callback */
    tglog_metrics *metrics;             /* metrics for the message */
    int worker_id;                      /* id of the worker that created the message */
    tglog_msg_flag flag;                /* message flags */
    tglog_payload_pool *payload_pool;   /* payload pool */
    tglog_message_pool *pool;           /* message pool */
    ck_stack_entry_t stack_entry;       /* stack entry */
} tglog_message;

CK_RING_PROTOTYPE(msg_queue, tglog_message*);

/**
 * Create a tglog message.
 * @param mem_pool The memory pool.
 * @param payload The log payload.
 * @param len The length of the payload.
 * @param callback The callback function for message completion.
 * @param user_data User data to pass to the callback.
 * @param flag The message flag.
 * @param metrics Metrics for the message.
 * @param worker_id The id of the worker that created the message.
 * @return A message or NULL on errors.
 */
tglog_message *tglog_message_create(tglog_mem_pool *mem_pool, const char *payload, size_t len,
                                    tglog_msg_callback callback, void *user_data,
                                    tglog_msg_flag flag, tglog_metrics *metrics, int worker_id);

/**
 * Get the length of a message.
 *
 * If the message's payload ends with tglog delimiter('\n'), the length is the length of the payload,
 * else the length is the length of the payload plus 1, which indicates that we need to append a delimiter after the payload.
 *
 * @param msg The message.
 * @return The length of the message.
 */
size_t tglog_message_len(tglog_message *msg);

/**
 * Destroy a tglog message.
 *
 * @param msg The message to destroy.
 */
void tglog_message_destroy(tglog_message *msg);

/**
 * Call the callback function of a message.
 *
 * @param msg The message.
 * @param err_code The error code.
 */
void tglog_message_done(tglog_message *msg, int err_code);

typedef struct tglog_batch {
    tglog_message **msgs;                           /* message array */
    int capacity;                                   /* capacity of the message array */
    int msg_count;                                  /* number of messages in the batch */
    size_t len;                                     /* length of the batch */
    struct timeval time;                            /* time when the batch is created or reset */
    int sent_index;                                 /* index of the message has been sent */
    int retries;                                    /* number of retries */
    struct timeval retry_start_time;                /* start time of batch retry */
    struct timeval retry_deadline;                  /* retry deadline */
    char *buffer;                                   /* buffer for batch data (to avoid re-assembling on retry) */
    size_t buffer_len;                              /* length of data in buffer */
    struct iovec *iov;                              /* iovec for batch data (to avoid re-assembling on retry) */
    int iov_count;                                  /* number of iovec entries */
} tglog_batch;

/**
 * Create a tglog batch.
 * @param capacity The capacity of the batch.
 * @return A batch or NULL on errors.
 */
tglog_batch *tglog_batch_create(int capacity);

/**
 * Destroy a tglog batch.
 *
 * @param batch The batch to destroy.
 */
void tglog_batch_destroy(tglog_batch *batch);

/**
 * Reset a batch.
 *
 * @param batch The batch to reset.
 */
void tglog_batch_reset(tglog_batch *batch);

/**
 * Add a message to a batch.
 *
 * @param batch The batch.
 * @param msg The message to add.
 * @return true if the message was added, false if the batch is full.
 */
bool tglog_batch_add(tglog_batch *batch, tglog_message *msg);

/**
 * Call the callback function of all messages in a batch.
 *
 * @param batch The batch.
 * @param err_code The error code.
 */
void tglog_batch_done(tglog_batch *batch, int err_code);

/**
 * Check if a batch can be sent, ignoring the timeout.
 *
 * @param batch The batch.
 * @param max_msg The maximum number of messages in a batch.
 * @param max_size The maximum size of a batch.
 * @return true if the batch can be sent, false otherwise.
 */
bool tglog_batch_can_send_ignore_timeout(tglog_batch *batch, int max_msg, int max_size);

/**
 * Check if a batch can be sent.
 *
 * @param batch The batch.
 * @param max_msg The maximum number of messages in a batch.
 * @param max_size The maximum size of a batch.
 * @param max_delay The maximum delay in milliseconds of a batch.
 * @return true if the batch can be sent, false otherwise.
 */
bool tglog_batch_can_send(tglog_batch *batch, int max_msg, int max_size, int max_delay);

/**
 * Get the length of a batch.
 *
 * @param batch The batch.
 * @return The length of the batch.
 */
size_t tglog_batch_len(tglog_batch *batch);

typedef struct tglog_backoff {
    unsigned int initial_interval;
    unsigned int max_interval;
    double multiplier;
    double randomization;
} tglog_backoff;

/**
 * Get the next interval for a backoff.
 *
 * @param b The backoff.
 * @param retry_count The retry count.
 * @param bound_max Whether to bound the interval by the max interval.
 * @param rand_seed The random seed for rand_r.
 * @return The next interval.
 */
unsigned int tglog_backoff_next_interval(tglog_backoff *b, int retry_count, bool bound_max, unsigned int *rand_seed);

/**
 * Get the total backoff time by retry count.
 *
 * @param b The backoff.
 * @param retry_count The retry count.
 * @param rand_seed The random seed for rand_r.
 * @return The total backoff time.
 */
unsigned int tglog_backoff_get_total_backoff_time(tglog_backoff *b, int retry_count, unsigned int *rand_seed);

/* Forward declaration */
typedef struct tglog_endpoint_list tglog_endpoint_list;

/* Endpoint notify structure */
typedef struct tglog_endpoint_notify {
    tglog_endpoint_list *all;
    tglog_endpoint_list *added;
    tglog_endpoint_list *removed;
} tglog_endpoint_notify;

CK_RING_PROTOTYPE(epn_queue, tglog_endpoint_notify*);

/**
 * Create a tglog endpoint notify.
 * @param all The list of all endpoints.
 * @param added The list of added endpoints.
 * @param removed The list of removed endpoints.
 * @return An endpoint notify or NULL on errors.
 */
tglog_endpoint_notify *tglog_endpoint_notify_create(const tglog_endpoint_list *all, const tglog_endpoint_list *added, const tglog_endpoint_list *removed);

/**
 * Destroy a tglog endpoint notify.
 *
 * @param epn The endpoint notify to destroy.
 */
void tglog_endpoint_notify_destroy(tglog_endpoint_notify *epn);

/**
 * Get current time in milliseconds.
 *
 * @return Current time in milliseconds.
 */
uint64_t tglog_get_time_ms(void);

/**
 * Get current time in microseconds.
 *
 * @return Current time in microseconds.
 */
uint64_t tglog_get_time_us(void);

/**
 * Get time since a timeval in milliseconds.
 *
 * @param tv The timeval.
 * @return Time since the timeval in milliseconds.
 */
long tglog_time_since_ms(struct timeval *tv);

#ifdef __cplusplus
}
#endif

#endif /* TGLOG_REQUEST_H */