/* Standard library headers */
#include <math.h>
#include <stdlib.h>
#include <inttypes.h>

/* System headers */
#include "tglog_platform.h"

/* Project headers */
#include "pool.h"
#include "request.h"
#include "conf.h"


tglog_payload_pool *tglog_payload_pool_create(size_t block_size, size_t capacity) {
    tglog_payload_pool *pool = calloc(1, sizeof(tglog_payload_pool));
    if (!pool) {
        return NULL;
    }

    pool->block_size = block_size;
    pool->capacity = capacity;
    pool->allocated = 0;

    /* Calculate the size of each payload block: header + data area */
    size_t payload_block_size = sizeof(tglog_payload) + block_size;

    /* Allocate a continuous memory block for all payloads */
    pool->payload_array = malloc(payload_block_size * capacity);
    if (!pool->payload_array) {
        free(pool);
        return NULL;
    }

    /* Initialize the payload stack */
    tglog_stack_init(&pool->payload_stack);

    /* Initialize each payload block in the continuous memory */
    for (size_t i = 0; i < capacity; i++) {
        /* Calculate the address of current payload block */
        tglog_payload *payload = (tglog_payload *) ((char *) pool->payload_array + i * payload_block_size);

        /* Set payload data pointer to the area right after the header */
        payload->data = (char *) payload + sizeof(tglog_payload);
        payload->size = block_size;

        /* Push to stack */
        tglog_stack_push_spnc(&pool->payload_stack, &payload->stack_entry);
        pool->allocated++;
    }

    return pool;
}

void tglog_payload_pool_destroy(tglog_payload_pool *pool) {
    if (!pool) {
        return;
    }

    /* Free the continuous memory block */
    if (pool->payload_array) {
        free(pool->payload_array);
    }

    free(pool);
}

TGLOG_STACK_CONTAINER(tglog_payload, stack_entry, entry_to_payload);

tglog_payload *tglog_payload_pool_get(tglog_payload_pool *pool) {
    if (!pool) {
        return NULL;
    }

    tglog_stack_entry_t *entry = tglog_stack_pop_npsc(&pool->payload_stack);
    if (entry) {
        pool->hit_count++;
        return entry_to_payload(entry);
    }

    pool->miss_count++;
    return NULL;
}

void tglog_payload_pool_put(tglog_payload_pool *pool, tglog_payload *payload) {
    if (!pool || !payload) {
        return;
    }

    tglog_stack_push_spnc(&pool->payload_stack, &payload->stack_entry);
    pool->return_count++;
}

void *tglog_payload_pool_get_buf(tglog_payload_pool *pool) {
    tglog_payload *payload = tglog_payload_pool_get(pool);
    if (!payload) {
        return NULL;
    }

    return payload->data;
}

void tglog_payload_pool_put_buf(tglog_payload_pool *pool, void *buf) {
    if (!pool || !buf) {
        return;
    }

    tglog_payload *payload = (tglog_payload *) ((char *) buf - sizeof(tglog_payload));
    tglog_payload_pool_put(pool, payload);
}

#define PAYLOAD_SIZE_256B   256
#define PAYLOAD_SIZE_512B   512
#define PAYLOAD_SIZE_1KB    1024
#define PAYLOAD_SIZE_2KB    2048
#define PAYLOAD_SIZE_4KB    4096
#define PAYLOAD_SIZE_8KB    8192
#define PAYLOAD_SIZE_16KB   16384
#define PAYLOAD_SIZE_32KB   32768
#define PAYLOAD_SIZE_48KB   49152
#define PAYLOAD_SIZE_64KB   65536

tglog_payload_pools *tglog_payload_pools_create(const tglog_conf *conf) {
    tglog_payload_pools *pools = calloc(1, sizeof(tglog_payload_pools));
    if (!pools) {
        return NULL;
    }

    /*Set pool block size*/
    pools->block_size[0] = PAYLOAD_SIZE_256B;
    pools->block_size[1] = PAYLOAD_SIZE_512B;
    pools->block_size[2] = PAYLOAD_SIZE_1KB;
    pools->block_size[3] = PAYLOAD_SIZE_2KB;
    pools->block_size[4] = PAYLOAD_SIZE_4KB;
    pools->block_size[5] = PAYLOAD_SIZE_8KB;
    pools->block_size[6] = PAYLOAD_SIZE_16KB;
    pools->block_size[7] = PAYLOAD_SIZE_32KB;
    pools->block_size[8] = PAYLOAD_SIZE_48KB;
    pools->block_size[9] = PAYLOAD_SIZE_64KB;

    /*Set pool capacity*/
    size_t capacities[10] = {
            conf->payload_pool_256b_cap,
            conf->payload_pool_512b_cap,
            conf->payload_pool_1kb_cap,
            conf->payload_pool_2kb_cap,
            conf->payload_pool_4kb_cap,
            conf->payload_pool_8kb_cap,
            conf->payload_pool_16kb_cap,
            conf->payload_pool_32kb_cap,
            conf->payload_pool_48kb_cap,
            conf->payload_pool_64kb_cap
    };

    /* Create pools for each size category */
    for (int i = 0; i < 10; i++) {
        if (capacities[i] > 0) {
            pools->pools[i] = tglog_payload_pool_create(pools->block_size[i], capacities[i]);
            if (!pools->pools[i]) {
                tglog_payload_pools_destroy(pools);
                return NULL;
            }
        }
    }

    return pools;
}

void tglog_payload_pools_destroy(tglog_payload_pools *pools) {
    if (!pools) {
        return;
    }

    for (int i = 0; i < 10; i++) {
        if (pools->pools[i]) {
            tglog_payload_pool_destroy(pools->pools[i]);
            pools->pools[i] = NULL;
        }
    }

    free(pools);
}

#define PAYLOAD_POOL_256B   0
#define PAYLOAD_POOL_512B   1
#define PAYLOAD_POOL_1KB    2
#define PAYLOAD_POOL_2KB    3
#define PAYLOAD_POOL_4KB    4
#define PAYLOAD_POOL_8KB    5
#define PAYLOAD_POOL_16KB   6
#define PAYLOAD_POOL_32KB   7
#define PAYLOAD_POOL_48KB   8
#define PAYLOAD_POOL_64KB   9

static inline int tglog_get_payload_pool_index(size_t size) {
    if (size <= 256) return PAYLOAD_POOL_256B;
    if (size <= 512) return PAYLOAD_POOL_512B;
    if (size <= 1024) return PAYLOAD_POOL_1KB;
    if (size <= 2048) return PAYLOAD_POOL_2KB;
    if (size <= 4096) return PAYLOAD_POOL_4KB;
    if (size <= 8192) return PAYLOAD_POOL_8KB;
    if (size <= 16384) return PAYLOAD_POOL_16KB;
    if (size <= 32768) return PAYLOAD_POOL_32KB;
    if (size <= 49152) return PAYLOAD_POOL_48KB;
    if (size <= 65536) return PAYLOAD_POOL_64KB;
    return -1;
}

tglog_payload_pool *tglog_payload_pools_get_pool(tglog_payload_pools *pools, size_t size) {
    if (!pools) {
        return NULL;
    }

    int index = tglog_get_payload_pool_index(size);
    if (index < 0) {
        return NULL;
    }

    return pools->pools[index];
}

tglog_message_pool *tglog_message_pool_create(size_t capacity) {
    tglog_message_pool *pool = calloc(1, sizeof(tglog_message_pool));
    if (!pool) return NULL;

    /* Allocate memory for the message array */
    pool->msg_array = calloc(capacity, sizeof(tglog_message));
    if (!pool->msg_array) {
        free(pool);
        return NULL;
    }

    pool->capacity = capacity;
    pool->allocated = 0;

    /* Initialize the message stack */
    tglog_stack_init(&pool->msg_stack);

    /* Pre-fill the message stack with all message objects */
    for (size_t i = 0; i < capacity; i++) {
        tglog_message *msg = &pool->msg_array[i];
        msg->pool = pool;
        tglog_stack_push_spnc(&pool->msg_stack, &msg->stack_entry);
        pool->allocated++;
    }

    return pool;
}

void tglog_message_pool_destroy(tglog_message_pool *pool) {
    if (!pool) {
        return;
    }

    /* Free the message array */
    if (pool->msg_array) {
        free(pool->msg_array);
    }

    /* Free the pool */
    free(pool);
}

TGLOG_STACK_CONTAINER(tglog_message, stack_entry, entry_to_message);

tglog_message *tglog_message_pool_get(tglog_message_pool *pool) {
    tglog_stack_entry_t *entry = tglog_stack_pop_npsc(&pool->msg_stack);
    if (entry) {
        pool->hit_count++;
        tglog_message *msg = entry_to_message(entry);
        msg->pool = pool;
        return msg;
    }

    pool->miss_count++;
    return NULL;
}

void tglog_message_pool_put(tglog_message_pool *pool, tglog_message *msg) {
    tglog_stack_push_spnc(&pool->msg_stack, &msg->stack_entry);
    pool->return_count++;
}

int tglog_mem_pool_stats_print(tglog_mem_pool_stats *stats, char *buffer, size_t size) {
    if (!stats || !buffer || size == 0) {
        return -1;
    }

    size_t offset = 0;
    int n;

    /* Print message pool statistics */
    n = snprintf(buffer + offset, size - offset,
                 "\n=== TGLog Memory Pool Statistics ===\n"
                 "\nMessage Pool:\n"
                 "  Capacity:     %" PRIu64 "\n"
                 "  Hits:         %" PRIu64 "\n"
                 "  Misses:       %" PRIu64 "\n"
                 "  Returns:      %" PRIu64 "\n"
                 "  Hit Rate:     %.2f%%\n",
                 stats->msg_pool_capacity,
                 stats->msg_pool_hit_count,
                 stats->msg_pool_miss_count,
                 stats->msg_pool_return_count,
                 stats->msg_pool_hit_rate);

    if (n < 0 || offset + n >= size) {
        return -1;
    }
    offset += n;

    /* Print payload pools header */
    n = snprintf(buffer + offset, size - offset,
                 "\nPayload Pools:\n"
                 "  %-10s %-10s %-10s %-10s %-10s %-10s %-10s %-10s\n"
                 "  %s\n",
                 "Size", "Capacity", "Free", "Hits", "Misses", "Returns", "Hit Rate", "Usage",
                 "--------------------------------------------------------------------------------");

    if (n < 0 || offset + n >= size) {
        return -1;
    }
    offset += n;

    /* Print each payload pool statistics */
    for (int i = 0; i < 10; i++) {
        if (stats->payload_stats[i].capacity > 0) {
            uint64_t used = stats->payload_stats[i].capacity - stats->payload_stats[i].current_free;
            double usage = (double) used / (double) stats->payload_stats[i].capacity * 100.0;

            n = snprintf(buffer + offset, size - offset,
                         "  %-10zu %-10" PRIu64 " %-10" PRIu64 " %-10" PRIu64 " %-10" PRIu64 " %-10" PRIu64 " %-9.2f%% %-9.2f%%\n",
                         stats->payload_stats[i].block_size,
                         stats->payload_stats[i].capacity,
                         stats->payload_stats[i].current_free,
                         stats->payload_stats[i].hit_count,
                         stats->payload_stats[i].miss_count,
                         stats->payload_stats[i].return_count,
                         stats->payload_stats[i].hit_rate,
                         usage);

            if (n < 0 || offset + n >= size) {
                return -1;
            }
            offset += n;
        }
    }

    /* Print payload pools summary */
    n = snprintf(buffer + offset, size - offset,
                 "\nPayload Pools Summary:\n"
                 "  Total Hits:    %" PRIu64 "\n"
                 "  Total Misses:  %" PRIu64 "\n"
                 "  Total Returns: %" PRIu64 "\n"
                 "  Hit Rate:      %.2f%%\n\n",
                 stats->total_payload_hits,
                 stats->total_payload_misses,
                 stats->total_payload_returns,
                 stats->total_payload_hit_rate);

    if (n < 0 || offset + n >= size) {
        return -1;
    }
    offset += n;

    return (int) offset;
}

int tglog_mem_pool_stats_print_json(tglog_mem_pool_stats *stats, char *buffer, size_t size) {
    if (!stats || !buffer || size == 0) {
        return -1;
    }

    size_t offset = 0;
    int n;

    n = snprintf(buffer + offset, size - offset, "{\n");
    if (n < 0 || offset + n >= size) {
        return -1;
    }
    offset += n;

    /* Message pool statistics */
    n = snprintf(buffer + offset, size - offset,
                 "  \"message_pool\": {\n"
                 "    \"capacity\": %" PRIu64 ",\n"
                 "    \"hits\": %" PRIu64 ",\n"
                 "    \"misses\": %" PRIu64 ",\n"
                 "    \"returns\": %" PRIu64 ",\n"
                 "    \"hit_rate\": %.2f\n"
                 "  },\n",
                 stats->msg_pool_capacity,
                 stats->msg_pool_hit_count,
                 stats->msg_pool_miss_count,
                 stats->msg_pool_return_count,
                 stats->msg_pool_hit_rate);

    if (n < 0 || offset + n >= size) {
        return -1;
    }
    offset += n;

    /* Payload pools statistics */
    n = snprintf(buffer + offset, size - offset, "  \"payload_pools\": [\n");
    if (n < 0 || offset + n >= size) {
        return -1;
    }
    offset += n;

    int first = 1;
    for (int i = 0; i < 10; i++) {
        if (stats->payload_stats[i].capacity > 0) {
            if (!first) {
                n = snprintf(buffer + offset, size - offset, ",\n");
                if (n < 0 || offset + n >= size) {
                    return -1;
                }
                offset += n;
            }
            first = 0;

            uint64_t used = stats->payload_stats[i].capacity - stats->payload_stats[i].current_free;

            n = snprintf(buffer + offset, size - offset,
                         "    {\n"
                         "      \"size\": %zu,\n"
                         "      \"capacity\": %" PRIu64 ",\n"
                         "      \"free\": %" PRIu64 ",\n"
                         "      \"used\": %" PRIu64 ",\n"
                         "      \"hits\": %" PRIu64 ",\n"
                         "      \"misses\": %" PRIu64 ",\n"
                         "      \"returns\": %" PRIu64 ",\n"
                         "      \"hit_rate\": %.2f\n"
                         "    }",
                         stats->payload_stats[i].block_size,
                         stats->payload_stats[i].capacity,
                         stats->payload_stats[i].current_free,
                         used,
                         stats->payload_stats[i].hit_count,
                         stats->payload_stats[i].miss_count,
                         stats->payload_stats[i].return_count,
                         stats->payload_stats[i].hit_rate);

            if (n < 0 || offset + n >= size) {
                return -1;
            }
            offset += n;
        }
    }

    n = snprintf(buffer + offset, size - offset, "\n  ],\n");
    if (n < 0 || offset + n >= size) {
        return -1;
    }
    offset += n;

    /* Payload statistics summary */
    n = snprintf(buffer + offset, size - offset,
                 "  \"summary\": {\n"
                 "    \"total_payload_hits\": %" PRIu64 ",\n"
                 "    \"total_payload_misses\": %" PRIu64 ",\n"
                 "    \"total_payload_returns\": %" PRIu64 ",\n"
                 "    \"total_payload_hit_rate\": %.2f\n"
                 "  }\n"
                 "}\n",
                 stats->total_payload_hits,
                 stats->total_payload_misses,
                 stats->total_payload_returns,
                 stats->total_payload_hit_rate);

    if (n < 0 || offset + n >= size) {
        return -1;
    }
    offset += n;

    return (int) offset;
}

tglog_mem_pool *tglog_mem_pool_create(const tglog_conf *conf) {
    tglog_mem_pool *pool = calloc(1, sizeof(tglog_mem_pool));
    if (!pool) {
        return NULL;
    }

    pool->msg_pool = tglog_message_pool_create(conf->message_pool_cap);
    if (!pool->msg_pool) {
        tglog_mem_pool_destroy(pool);
        return NULL;
    }

    pool->payload_pools = tglog_payload_pools_create(conf);
    if (!pool->payload_pools) {
        tglog_mem_pool_destroy(pool);
        return NULL;
    }

    return pool;
}

void tglog_mem_pool_destroy(tglog_mem_pool *pool) {
    if (!pool) {
        return;
    }

    tglog_payload_pools_destroy(pool->payload_pools);
    tglog_message_pool_destroy(pool->msg_pool);
    free(pool);
}

/* Single-threaded version: no concurrent push/pop, so direct read of
 * stack->head via TGLOG_STACK_FIRST() is safe — no need for atomic pair load. */
static size_t tglog_stack_count(tglog_stack_t *stack) {
    size_t count = 0;
    tglog_stack_entry_t *entry = TGLOG_STACK_FIRST(stack);
    while (entry != NULL) {
        count++;
        entry = TGLOG_STACK_NEXT(entry);
    }
    return count;
}

bool tglog_mem_pool_get_stats(tglog_mem_pool *pool, tglog_mem_pool_stats *stats) {
    if (!pool || !stats) {
        return false;
    }

    /* Clear statistics structure */
    memset(stats, 0, sizeof(tglog_mem_pool_stats));

    /* 1. Collect message pool statistics */
    if (pool->msg_pool) {
        tglog_message_pool *msg_pool = pool->msg_pool;
        stats->msg_pool_capacity = msg_pool->capacity;
        stats->msg_pool_hit_count = msg_pool->hit_count;
        stats->msg_pool_miss_count = msg_pool->miss_count;
        stats->msg_pool_return_count = msg_pool->return_count;

        /* Calculate hit rate */
        uint64_t total_requests = stats->msg_pool_hit_count + stats->msg_pool_miss_count;
        if (total_requests > 0) {
            stats->msg_pool_hit_rate = (double) stats->msg_pool_hit_count / (double) total_requests * 100.0;
        }
    }

    /* 2. Collect payload pool statistics */
    if (pool->payload_pools) {
        uint64_t total_hits = 0;
        uint64_t total_misses = 0;
        uint64_t total_returns = 0;

        for (int i = 0; i < 10; i++) {
            tglog_payload_pool *p_pool = pool->payload_pools->pools[i];

            if (p_pool) {
                stats->payload_stats[i].block_size = p_pool->block_size;
                stats->payload_stats[i].capacity = p_pool->capacity;
                stats->payload_stats[i].hit_count = p_pool->hit_count;
                stats->payload_stats[i].miss_count = p_pool->miss_count;
                stats->payload_stats[i].return_count = p_pool->return_count;

                /* Calculate current free count */
                stats->payload_stats[i].current_free = tglog_stack_count(&p_pool->payload_stack);

                /* Calculate hit rate */
                uint64_t requests = stats->payload_stats[i].hit_count + stats->payload_stats[i].miss_count;
                if (requests > 0) {
                    stats->payload_stats[i].hit_rate = (double) stats->payload_stats[i].hit_count / (double) requests * 100.0;
                }

                /* Accumulate to totals */
                total_hits += stats->payload_stats[i].hit_count;
                total_misses += stats->payload_stats[i].miss_count;
                total_returns += stats->payload_stats[i].return_count;
            } else {
                /* Pool does not exist, set block_size to corresponding size */
                stats->payload_stats[i].block_size = pool->payload_pools->block_size[i];
            }
        }

        /* 3. Calculate summary statistics */
        stats->total_payload_hits = total_hits;
        stats->total_payload_misses = total_misses;
        stats->total_payload_returns = total_returns;

        uint64_t total_payload_requests = total_hits + total_misses;
        if (total_payload_requests > 0) {
            stats->total_payload_hit_rate = (double) total_hits / (double) total_payload_requests * 100.0;
        }
    }

    return true;
}


