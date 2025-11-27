#ifndef TGLOG_POOL_H
#define TGLOG_POOL_H

/* Standard library headers */
#include <stdio.h>

/* System headers */

/* Third-party library headers */
#include "ck_stack.h"

/* Project headers */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tglog_payload {
    void *data;                      /* payload data */
    size_t size;                     /* payload size */
    ck_stack_entry_t stack_entry;    /* stack entry */
} tglog_payload;

typedef struct tglog_payload_pool {
    ck_stack_t payload_stack;       /* payload stack */
    void *payload_array;            /* payload array */
    size_t block_size;              /* block size */
    size_t capacity;                /* pool capacity */
    size_t allocated;               /* allocated payloads */
    uint64_t hit_count;             /* hit count (atomic) */
    uint64_t miss_count;            /* miss count (atomic) */
    uint64_t return_count;          /* return count (atomic) */
} tglog_payload_pool;

/**
 * Create a payload pool.
 *
 * @param block_size The block size.
 * @param capacity The pool capacity.
 * @return The payload pool.
 */
tglog_payload_pool *tglog_payload_pool_create(size_t block_size, size_t capacity);

/**
 * Destroy a payload pool.
 *
 * @param pool The payload pool.
 */
void tglog_payload_pool_destroy(tglog_payload_pool *pool);

/**
 * Get a payload from the payload pool.
 *
 * @param pool The payload pool.
 * @return The payload.
 */
tglog_payload *tglog_payload_pool_get(tglog_payload_pool *pool);

/**
 * Return a payload to the payload pool.
 *
 * @param pool The payload pool.
 * @param payload The payload.
 */
void tglog_payload_pool_put(tglog_payload_pool *pool, tglog_payload *payload);

/**
 * Get a buffer from the payload pool.
 *
 * @param pool The payload pool.
 * @return The buffer. You should call tglog_payload_pool_put_buf() to return the buffer.
 */
void *tglog_payload_pool_get_buf(tglog_payload_pool *pool);

/**
 * Return a buffer to the payload pool.
 *
 * @param pool The payload pool.
 * @param buf The buffer. The buffer should be allocated by tglog_payload_pool_get_buf().
 */
void tglog_payload_pool_put_buf(tglog_payload_pool *pool, void *buf);

typedef struct tglog_conf tglog_conf;
typedef struct tglog_payload_pools {
    tglog_payload_pool *pools[10];  /* 256B, 512B, 1KB, 2KB, 4KB, 8KB, 16KB, 32KB, 48KB, 64KB */
    size_t block_size[10];          /* block size for each payload pool */
} tglog_payload_pools;

/**
 * Create a payload pools.
 *
 * @param conf The configuration.
 * @return The payload pools.
 */
tglog_payload_pools *tglog_payload_pools_create(const tglog_conf *conf);

/**
 * Destroy a payload pools.
 *
 * @param pools The payload pools.
 */
void tglog_payload_pools_destroy(tglog_payload_pools *pools);

/**
 * Get a payload pool from the payload pools.
 *
 * @param pools The payload pools.
 * @param size The payload size.
 * @return The payload pool.
 */
tglog_payload_pool *tglog_payload_pools_get_pool(tglog_payload_pools *pools, size_t size);

typedef struct tglog_message tglog_message;
typedef struct tglog_message_pool {
    ck_stack_t msg_stack;           /* message stack */
    tglog_message *msg_array;       /* message array */
    size_t capacity;                /* pool capacity */
    size_t allocated;               /* allocated messages */
    uint64_t hit_count;             /* hit count (atomic) */
    uint64_t miss_count;            /* miss count (atomic) */
    uint64_t return_count;          /* return count (atomic) */
} tglog_message_pool;

/**
 * Create a message pool.
 *
 * @param capacity The pool capacity.
 * @return The message pool.
 */
tglog_message_pool *tglog_message_pool_create(size_t capacity);

/**
 * Destroy a message pool.
 *
 * @param pool The message pool.
 */
void tglog_message_pool_destroy(tglog_message_pool *pool);

/**
 * Get a message from the message pool.
 *
 * @param pool The message pool.
 * @return The message.
 */
tglog_message *tglog_message_pool_get(tglog_message_pool *pool);

/**
 * Return a message to the message pool.
 *
 * @param pool The message pool.
 * @param msg The message.
 */
void tglog_message_pool_put(tglog_message_pool *pool, tglog_message *msg);

typedef struct tglog_mem_pool_stats {
    /* message pool statistics */
    uint64_t msg_pool_capacity;
    uint64_t msg_pool_hit_count;
    uint64_t msg_pool_miss_count;
    uint64_t msg_pool_return_count;
    double msg_pool_hit_rate;

    /* payload pool statistics */
    struct {
        size_t block_size;
        uint64_t capacity;
        uint64_t hit_count;
        uint64_t miss_count;
        uint64_t return_count;
        double hit_rate;
        uint64_t current_free;
    } payload_stats[10];

    /* total statistics */
    uint64_t total_payload_hits;
    uint64_t total_payload_misses;
    uint64_t total_payload_returns;
    double total_payload_hit_rate;
} tglog_mem_pool_stats;

/**
 * Print memory pool statistics.
 *
 * @param stats The memory pool statistics.
 * @param buffer The buffer to print the statistics to, at least 2048 bytes.
 * @param size The size of the buffer.
 * @return The number of bytes written to the buffer or -1 on error.
 */
int tglog_mem_pool_stats_print(tglog_mem_pool_stats *stats, char *buffer, size_t size);

/**
 * Print memory pool statistics in JSON format.
 *
 * @param stats The memory pool statistics.
 * @param buffer The buffer to print the statistics to, at least 4096 bytes.
 * @param size The size of the buffer.
 * @return The number of bytes written to the buffer or -1 on error.
 */
int tglog_mem_pool_stats_print_json(tglog_mem_pool_stats *stats, char *buffer, size_t size);

typedef struct tglog_mem_pool {
    tglog_message_pool *msg_pool;           /* message pool */
    tglog_payload_pools *payload_pools;     /* payload pools */
} tglog_mem_pool;

/**
 * Create a memory pool.
 *
 * @param conf The configuration.
 * @return The memory pool.
 */
tglog_mem_pool *tglog_mem_pool_create(const tglog_conf *conf);

/**
 * Destroy a memory pool.
 *
 * @param pool The memory pool.
 */
void tglog_mem_pool_destroy(tglog_mem_pool *pool);

/**
   * Get memory pool statistics.
   *
   * @param pool The memory pool.
   * @param stats The statistics.
   * @return 0 on success, -1 on error.
   */
int tglog_mem_pool_get_stats(tglog_mem_pool *pool, tglog_mem_pool_stats *stats);

#ifdef __cplusplus
}
#endif

#endif //TGLOG_POOL_H
