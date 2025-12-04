#ifndef TGLOG_CLIENT_H
#define TGLOG_CLIENT_H

/* Standard library headers */
#include <stdint.h>

/* System headers */

/* Third-party library headers */

/* Project headers */
#include "conf.h"
#include "def.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct tglog_debug_logger tglog_debug_logger;
typedef struct tglog_mem_pool tglog_mem_pool;
typedef struct tglog_discoverer tglog_discoverer;
typedef struct tglog_metrics tglog_metrics;
typedef struct tglog_worker tglog_worker;
typedef struct tglog_mem_pool_stats tglog_mem_pool_stats;

typedef struct tglog_client {
    tglog_conf conf;                                  /* configuration */
    tglog_debug_logger *logger;                       /* debug logger */
    tglog_mem_pool *mem_pool;                         /* memory pool */
    struct event_base *base;                          /* internal event_base (self-contained) */
    tglog_discoverer *discoverer;                     /* discoverer, DNS lookup */
    tglog_metrics *metrics;                           /* metrics */
    tglog_worker *worker;                             /* worker for message processing */
} tglog_client;

/**
 * Create a new client.
 *
 * The client will make a copy of the configuration. If the input configuration is allocated on the heap, the caller is responsible for freeing it.
 * In single-threaded version, the client will create and maintain its own event_base internally.
 * The initial connection will be created synchronously with a timeout.
 *
 * @param conf The configuration.
 * @return The client, or NULL on error.
 */
tglog_client *tglog_client_create(const tglog_conf *conf);

/**
 * Destroy a client.
 *
 * This function will automatically flush all pending logs before destroying.
 *
 * @param client The client.
 * @return void
 */
void tglog_client_destroy(tglog_client *client);

/**
 * The callback function of tglog_client_send.
 *
 * @param log_payload The payload of the log.
 * @param err_code The error code.
 * @param err_msg The error message.
 * @param user_data The user data.
 */
typedef void (*tglog_send_callback)(const char *log_payload, int err_code, const char *err_msg, void *user_data);

/**
 * Send a log to the server.
 * 
 * TGLog is of this format: "<log_name>|v1|v2|v3|...|vn\n",
 * for example: "login|2025-06-06 12:00:00|username|password\n",
 * where log_name is: "login",
 * and log_payload is the whole log line: "login|2025-06-06 12:00:00|username|password\n"
 *
 * @param client The client.
 * @param log_payload The payload of the log, should end with '\n'.
 * @param callback The callback function.
 * @param user_data The user data used to call the callback.
 * @param msg_flag The flag of the message, can be TGLOG_MSG_F_NONE, TGLOG_MSG_F_FREE or TGLOG_MSG_F_COPY, see def.h to reference the flags.
 *  TGLOG_MSG_F_NONE: The client will not free or copy the log_payload, you need to free it yourself.
 *  TGLOG_MSG_F_FREE: The client will call free() to free the log_payload after sending, make sure your log_payload is allocated with malloc()/calloc().
 *  TGLOG_MSG_F_COPY: The client will copy the log_payload before sending, you can free it yourself after the call.
 * @return 0 on success, others on error, see def.h to reference the error codes.
 */
int tglog_client_send(tglog_client *client, const char *log_payload, tglog_send_callback callback, void *user_data, tglog_msg_flag msg_flag);

/**
 * Get the size of the message queue.
 *
 * @param client The client.
 * @return The size of the message queue.
 */
size_t tglog_client_get_msg_queue_size(tglog_client *client);

/**
 * Get the capacity of the message queue.
 *
 * @param client The client.
 * @return The capacity of the message queue.
 */
size_t tglog_client_get_msg_queue_capacity(tglog_client *client);

/**
 * Print the metrics of the client.
 *
 * @param client The client.
 * @param buffer The buffer to print the metrics, at least 4096 (8192 for JSON) bytes.
 * @param size The size of the buffer.
 * @param json Whether to print the metrics in JSON format.
 * @return The number of bytes written to the buffer or -1 on error.
 */
int tglog_client_print_metrics(tglog_client *client, char *buffer, size_t size, bool json);

/**
 * Print the memory pool statistics of the client.
 *
 * @param client The client.
 * @param buffer The buffer to print the metrics, at least 4096 (8192 for JSON) bytes.
 * @param size The size of the buffer.
 * @param json Whether to print the metrics in JSON format.
 * @return The number of bytes written to the buffer or -1 on error.
 */
int tglog_client_print_mem_pool_stats(tglog_client *client, char *buffer, size_t size, bool json);

/**
 * Update the client.
 *
 * This function must be called periodically in single-threaded mode.
 * It processes network events, handles batch timeouts, DNS updates, etc.
 * The function is non-blocking and will return quickly.
 *
 * @param client The client.
 * @return message count processed on success, negative on error.
 */
int tglog_client_update(tglog_client *client);

#ifdef __cplusplus
}
#endif

#endif /* TGLOG_CLIENT_H */