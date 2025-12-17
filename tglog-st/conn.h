#ifndef TGLOG_CONN_H
#define TGLOG_CONN_H

/* Standard library headers */
#include <stdbool.h>

/* Third-party library headers */
#include "event2/event.h"
#include "event2/bufferevent.h"

/* Project headers */
#include "conf.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum tglog_conn_create_type {
    CREATE_BY_NONE = 0,         /* conn is created and reset its create_type*/
    CREATE_BY_INIT = 1,         /* conn is created by init */
    CREATE_BY_ENDPOINT = 2,     /* conn is created by new endpoint found */
    CREATE_BY_SUPPLEMENT = 3,   /* conn is created by event that an old connection is closed by peer and needs to supplement a new one */
    CREATE_BY_REPLACE = 4,      /* conn is created by timer to replace the connection which has reached its lifetime */
    CREATE_BY_RECOVER = 5       /* conn is created by timer to recover unavailable endpoint */
} tglog_conn_create_type;

typedef struct tglog_worker tglog_worker;
typedef struct tglog_conn {
    tglog_worker *worker;               /* worker who hold this conn */
    tglog_conn_create_type create_type; /* conn create type */
    tglog_network network;              /* network type */
    char *endpoint;                     /* endpoint address */
    evutil_socket_t fd;                 /* socket fd */
    struct bufferevent *bev;            /* buffered event */
    struct event *close_event;          /* delay close timer event */
    void *user_data;                    /* user data */
    struct evbuffer *pending_data;      /* pending data to rescue */
    time_t expire_time;                 /* expire time */
    int write_high_watermark;           /* write high watermark */
} tglog_conn;

typedef struct tglog_conn_list {
    tglog_conn **items;                 /* items */
    int capacity;                       /* capacity */
    int count;                          /* count */
} tglog_conn_list;

/**
 * Create a conn.
 *
 * @param worker The worker.
 * @param create_type The create type.
 * @param network The network type.
 * @param endpoint The endpoint address.
 * @param fd The socket fd.
 * @param bev The buffered event.
 * @param user_data The user data.
 * @param write_high_watermark The write high watermark.
 * @param max_lifetime The max lifetime.
 * @return The conn.
 */
tglog_conn *tglog_conn_create(tglog_worker *worker, tglog_conn_create_type create_type, tglog_network network, const char *endpoint,
                              evutil_socket_t fd, struct bufferevent *bev, void *user_data,
                              int write_high_watermark, int max_lifetime);

/**
 * Send data to a conn.
 *
 * @param conn The connection.
 * @param data The data to send.
 * @param len The length of the data.
 * @return The number sent or -1 and errno set to indicate the error.
 */
ssize_t tglog_conn_send(tglog_conn *conn, const void *data, size_t len);

/**
 * Send multiple data buffers to a conn.
 *
 * @param conn The connection.
 * @param iov The data to send.
 * @param iovcnt The number of the data.
 * @return The number sent or -1 and errno set to indicate the error.
 */
ssize_t tglog_conn_sendv(tglog_conn *conn, const struct iovec *iov, int iovcnt);

/**
 * Send data to a conn using bufferevent.
 * If bufferevent's out buffer is not empty, use bufferevent_write, otherwise use send,
 * If partially sent, the remaining data will be buffered in the out buffer.
 * This function is only used for TCP.
 *
 * @param conn The connection.
 * @param data The data to send.
 * @param len The length of the data.
 * @return The number sent or -1.
 */
ssize_t tglog_conn_send_with_bufferevent(tglog_conn *conn, const void *data, size_t len);

/**
 * Send multiple data buffers to a conn using bufferevent.
 * If bufferevent's out buffer is not empty, use bufferevent_write, otherwise use writev,
 * If partially sent, the remaining data will be buffered in the out buffer.
 * This function is only used for TCP.
 *
 * @param conn The connection.
 * @param iov The data to send.
 * @param iovcnt The number of the data.
 * @return The number sent on success, 0 on bufferevent exceeds the high watermark or -1 on error.
 */
ssize_t tglog_conn_sendv_with_bufferevent(tglog_conn *conn, const struct iovec *iov, int iovcnt);

/**
 * Send data to a conn using evbuffer.
 * This function is only used for TCP.
 *
 * @param conn The connection.
 * @param buf The buffer to send.
 * @param len The length of the buffer.
 * @return The number sent or -1.
 */
ssize_t tglog_conn_send_with_buffer(tglog_conn *conn, struct evbuffer *buf, size_t len);

/**
 * Get the length of the out buffer.
 * This function is only used for TCP.
 *
 * @param conn The connection.
 * @return The length of the out buffer.
 */
size_t tglog_conn_get_out_buffer_len(tglog_conn *conn);

/**
 * Flush the out buffer.
 * This function is only used for TCP.
 *
 * @param conn The connection.
 */
void tglog_conn_flush(tglog_conn *conn);

/**
 * Write buffer to a conn.
 * This function is only used for TCP.
 *
 * @param conn The connection.
 * @param buf The buffer to write.
 * @return 0 on success, -1 on error.
 */
int tglog_conn_write_buffer(tglog_conn *conn, struct evbuffer *buf);

/**
 * Get the pending data.
 * This function is only used for TCP.
 *
 * @param conn The connection.
 * @return The pending data.
 */
struct evbuffer *tglog_conn_get_pending_data(tglog_conn *conn);

/**
 * Set the pending data.
 * This function is only used for TCP.
 *
 * @param conn The connection.
 * @param pending_data The pending data.
 */
void tglog_conn_set_pending_data(tglog_conn *conn, struct evbuffer *pending_data);

/**
 * Remove the pending data.
 * This function is only used for TCP.
 *
 * @param conn The connection.
 * @return The pending data.
 */
struct evbuffer *tglog_conn_remove_pending_data(tglog_conn *conn);

/**
 * Receive data with a conn.
 *
 * @param conn The connection.
 * @param buf The buffer to receive data.
 * @param buf_size The size of the buffer.
 * @param recv_len The number of bytes received.
 * @return The number of bytes received on success, or a negative value on errors.
 */
ssize_t tglog_conn_recv(tglog_conn *conn, void *buf, size_t buf_size, size_t *recv_len);

/**
 * Close a connection.
 * 
 * @param conn The connection.
 */
void tglog_conn_close(tglog_conn *conn);

/**
 * Create a conn list.
 *
 * @param capacity The capacity of the list.
 * @return A conn list or NULL on error.
 */
tglog_conn_list *tglog_conn_list_create(int capacity);

/**
 * Add a conn to the list.
 *
 * @param list The list.
 * @param conn The connection.
 * @return true on success, false on error.
 */
bool tglog_conn_list_add(tglog_conn_list *list, tglog_conn *conn);

/**
 * Remove a conn from the list.
 *
 * @param list The list.
 * @param conn The connection.
 * @return true on success, false on error.
 */
bool tglog_conn_list_remove(tglog_conn_list *list, tglog_conn *conn);

/**
 * Remove conns from the list by address.
 *
 * @param list The list.
 * @param addr The address.
 * @return true on success, false on error.
 */
bool tglog_conn_list_remove_by_addr(tglog_conn_list *list, char *addr);

/**
 * Get the number of conns in the list by address.
 *
 * @param list The list.
 * @param addr The address.
 * @return The number of conns.
 */
int tglog_conn_list_get_count_by_addr(tglog_conn_list *list, char *addr);

/**
 * Destroy a conn list.
 *
 * @param list The list.
 */
void tglog_conn_list_destroy(tglog_conn_list *list);

#ifdef __cplusplus
}
#endif

#endif /* TGLOG_CONN_H */