#ifndef TGLOG_CONF_H
#define TGLOG_CONF_H

/* Standard library headers */

/* System headers */

/* Third-party library headers */

/* Project headers */
#include "debug_logger.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum tglog_network {
    TGLOG_NETWORK_UDP = 0,
    TGLOG_NETWORK_TCP = 1,
} tglog_network;

typedef struct tglog_conf {
    tglog_network network;                  /* network: TGLOG_NETWORK_UDP/TGLOG_NETWORK_TCP, default: TGLOG_NETWORK_UDP */
    char host[256];                         /* server IP/domain name, default: none */
    int port;                               /* server port, default: 0 */
    int update_interval;                    /* time interval in seconds to refresh the server endpoints, default: 60s */
    int conn_timeout;                       /* connection timeout in ms, TCP only, default: 3000ms */
    /**
     * we use `writev` to write socket fd first, if the socket buffer is full, we do not block, we write the left data to bufferevent,
     * bufferevent will write the data to socket fd later when the socket can write more data, write_high_watermark is the threshold of
     * the buffered data size in a bufferevent.
     * 我们先用`writev`写socket fd，如果socket buffer满了，我们不会阻塞，我们会把剩余的数据写到bufferevent，bufferevent会在socket可以写更多数据时把数据写
     * 到socket fd，write_high_watermark是bufferevent中缓冲数据的阈值。
     */
    int write_high_watermark;               /* write high watermark in bytes, when the buffered data size in a connection higher than this value, stop writing to the connection, default: 32*1024*1024 */
    int socket_send_buffer_size;            /* socket send buffer size in bytes, default: 64*1024*1024 */
    int socket_recv_buffer_size;            /* socket receive buffer size in bytes, tglog has no response, default: 16*1024 */
    /**
     * max connection lifetime in seconds, default: 300s.
     * A 0 or negative value will use long lifetime connection, do NOT do that unless your RSs(real servers) are bound to the DNS directly, or if your RSs are bound to a CLB,
     * it will lead to unbalance traffics on CLB, and if you want to restart your RSs, you have to replace the CLBs bound to the DNS, or you will encounter data loss.
     * 连接生命周期，默认：300s。设置为0或负数时将使用长连接，服务器直接挂到DNS上才建议使用长连接，否则（如挂在CLB上）会导致不均衡，且服务器升级时要替换DNS下的CLB才能保证不丢数据。
     */
    int max_conn_lifetime;
    int batching_max_publish_delay;         /* the max delay time in ms of a batch, if times out, the message will be sent immediately, default: 10ms */
    int batching_max_messages;              /* max messages in a batch, default: 50 */
    int batching_max_size;                  /* max batch size in bytes, default: 32*1024 */
    int max_pending_messages;               /* how many messages can be pending every worker, single-threaded version has only 1 worker, default: 1000000, the real value will be the next power of 2 of this config */
    int send_timeout;                       /* send timeout in ms, how long can a message stay in the client, default: 30000ms */
    int max_retries;                        /* max retry counts, default: 3 */
    int stop_timeout;                       /* stop timeout in ms, default: 3000ms */
    /* debug logger */
    can_log_func can_log;                   /* debug log filter func, default: tglog_can_log_info*/
    write_log_func write_log;               /* debug log write func, default: tglog_std_write_log*/
    void *user_data;                        /* user data for debug log callbacks, default: NULL */
    size_t message_pool_cap;                /* message pool capacity, default: 100000 */
    size_t payload_pool_256b_cap;           /* payload pool 256b capacity, default: 2000 */
    size_t payload_pool_512b_cap;           /* payload pool 512b capacity, default: 2000 */
    size_t payload_pool_1kb_cap;            /* payload pool 1kb capacity, default: 2000 */
    size_t payload_pool_2kb_cap;            /* payload pool 2kb capacity, default: 1000 */
    size_t payload_pool_4kb_cap;            /* payload pool 4kb capacity, default: 1000 */
    size_t payload_pool_8kb_cap;            /* payload pool 8kb capacity, default: 1000 */
    size_t payload_pool_16kb_cap;           /* payload pool 16kb capacity, default: 500 */
    size_t payload_pool_32kb_cap;           /* payload pool 32kb capacity, default: 500 */
    size_t payload_pool_48kb_cap;           /* payload pool 48kb capacity, default: 100 */
    size_t payload_pool_64kb_cap;           /* payload pool 64kb capacity, default: 100 */
    /* single-threaded version specific configs */
    int max_update_us;                      /* max time in us for a single update call, default: 200us, the real time may exceed this limit */
    int max_messages_per_update;            /* max messages to process per update, default: 1000 */
} tglog_conf;

/**
 * Create a tglog configuration and set to default values
 *
 * @param network The network type, TGLOG_NETWORK_UDP or TGLOG_NETWORK_TCP.
 * @param host The server IP/domain name.
 * @param port The server port number.
 * @param can_log The debug log filter function, if `can_log` is NULL, `tglog_can_log_debug` will be used.
 * @param write_log The debug log write function, if `write_log` is NULL, `tglog_std_write_log` will be used.
 * @param user_data The user data for callbacks.
 * @return A conf or NULL on errors.
 */
tglog_conf *tglog_conf_create(tglog_network network, const char *host, int port, can_log_func can_log, write_log_func write_log, void *user_data);

/**
 * Set default values for a tglog configuration
 *
 * @param conf The conf to set.
 */
void tglog_conf_set_default(tglog_conf *conf);

/**
 * Set server for a tglog configuration
 *
 * @param conf The conf to set.
 * @param network The network type, TGLOG_NETWORK_UDP or TGLOG_NETWORK_TCP.
 * @param host The server IP/domain name.
 * @param port The server port number.
 * @return 0 on success, -1 on errors.
 */
int tglog_conf_set_server(tglog_conf *conf, tglog_network network, const char *host, int port);

/**
 * Set debug logger for a tglog configuration
 *
 * @param can_log The debug log filter function.
 * @param write_log The debug log write function.
 * @param user_data The user data for callbacks.
 * @param conf The conf to set.
 */
void tglog_conf_set_debug_logger(tglog_conf *conf, can_log_func can_log, write_log_func write_log, void *user_data);

/**
 * Destroy a tglog configuration
 *
 * @param conf The conf to destroy.
 */
void tglog_conf_destroy(tglog_conf *conf);

#ifdef __cplusplus
}
#endif

#endif /* TGLOG_CONF_H */