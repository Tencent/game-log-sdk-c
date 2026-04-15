/* Standard library headers */
#include <stdlib.h>

/* System headers */
#include "tglog_platform.h"

/* Third-party library headers */
#include "event2/buffer.h"

/* Project headers */
#include "conn.h"
#include "worker.h"
#include "debug_logger.h"

#define TGLOG_DEFAULT_WRITE_HIGH_WATERMARK (32 * 1024 * 1024)

tglog_conn *tglog_conn_create(tglog_worker *worker, tglog_conn_create_type create_type, tglog_network network, const char *endpoint,
                              evutil_socket_t fd, struct bufferevent *bev, void *user_data,
                              int write_high_watermark, int max_lifetime) {
    tglog_conn *conn = (tglog_conn *) calloc(1, sizeof(tglog_conn));
    if (!conn) {
        return NULL;
    }

    int max_jitter = max_lifetime / 10;
    max_jitter = max_jitter > 20 ? (max_jitter > 30 ? 30 : max_jitter) : 20; // 20 ~ 30
    /* Initialize connection */
    conn->worker = worker;
    conn->create_type = create_type;
    conn->network = network;
    conn->endpoint = strdup(endpoint);
    if (!conn->endpoint) {
        free(conn);
        return NULL;
    }
    conn->fd = fd;
    conn->bev = bev;
    conn->user_data = user_data;
    int jitter = rand_r(&worker->rand_seed) % max_jitter; // 0 ~ max_jitter-1
    conn->expire_time = time(NULL) + max_lifetime + jitter;
    conn->write_high_watermark = write_high_watermark > 0 ? write_high_watermark : TGLOG_DEFAULT_WRITE_HIGH_WATERMARK;
    return conn;
}

void tglog_conn_close(tglog_conn *conn) {
    if (!conn) {
        return;
    }

    if (conn->endpoint) {
        free(conn->endpoint);
    }

    if (conn->bev) {
        bufferevent_free(conn->bev);
    } else if (conn->fd >= 0) {
        evutil_closesocket(conn->fd);
    }

    if (conn->close_event) {
        event_free(conn->close_event);
    }

    if (conn->pending_data) {
        evbuffer_free(conn->pending_data);
        conn->pending_data = NULL;
    }

    /* Call worker function to update metrics */
    tglog_worker_on_conn_closed(conn->worker);
    free(conn);
}

ssize_t tglog_conn_send(tglog_conn *conn, const void *data, size_t len) {
    if (!conn || conn->fd < 0 || !conn->bev || !data || len == 0) {
        return -1;
    }

    /**
     * We do not use bufferevent_write here because it requires an additional memory copy to the bufferevent's buffer.
     * Furthermore, we cannot determine when the data is actually sent to the socket. The tglog protocol v1 only involves
     * requests without responses, so the only way to consider the data as sent is when it is successfully written to the socket.
     * Therefore, we write the data directly to the socket file descriptor here. This approach reduces one memory copy and
     * allows us to immediately know if the data was sent successfully and invoke the callbacks.
     * 这里不使用bufferevent_write是因为它可能需要多一次内存拷贝，而且写进去后何时真正写到socket上是无法知道的，tglog v1协议是只有请求没有响应
     * 的，只能通过写socket成功表示发送成功，所以我们这里直接把数据写到socket上，这样可以减少一次数据拷贝，也可以及时的知道数据是否发送成功并触发回调。
    */
    /*return bufferevent_write(conn->bev, data, len);*/
    return send(conn->fd, data, (int)len, MSG_NOSIGNAL);
}

ssize_t tglog_conn_sendv(tglog_conn *conn, const struct iovec *iov, int iovcnt) {
    if (!conn || conn->fd < 0 || !conn->bev || !iov || iovcnt <= 0) {
        return -1;
    }
    return writev(conn->fd, iov, iovcnt);
}

ssize_t tglog_conn_send_with_bufferevent(tglog_conn *conn, const void *data, size_t len) {
    if (!conn || conn->fd < 0 || !conn->bev || conn->network != TGLOG_NETWORK_TCP || !data || len == 0) {
        return -1;
    }

    size_t out_buffer_len = tglog_conn_get_out_buffer_len(conn);

    /* If out_buf is not empty, use bufferevent_write */
    if (out_buffer_len > 0) {
        size_t high_watermark = conn->write_high_watermark > 0 ? conn->write_high_watermark : TGLOG_DEFAULT_WRITE_HIGH_WATERMARK;
        if (out_buffer_len > high_watermark) {
            errno = ENOBUFS;
            return -1;
        }

        int ret = bufferevent_write(conn->bev, data, len);
        if (ret == 0) {
            bufferevent_flush(conn->bev, EV_WRITE, BEV_FLUSH);
            return (ssize_t) len;
        }
        return -1;
    }

    /* If out_buf is empty, try to send directly */
    ssize_t sent = send(conn->fd, data, (int)len, MSG_NOSIGNAL);

    /* Send success */
    if (sent == (ssize_t) len) {
        return sent;
    }

    int sock_err = evutil_socket_geterror(conn->fd);
    bool eagain = TGLOG_ERR_IS_EAGAIN(sock_err);
    if (eagain) {
        tglog_worker_on_conn_eagain(conn->worker);
    }

    /* Partial send success or EAGAIN/EWOULDBLOCK, write remaining data to bufferevent */
    if (sent > 0 || (sent == -1 && eagain)) {
        size_t remaining_len = sent > 0 ? len - sent : len;
        const char *remaining_data = (const char *) data + (sent > 0 ? sent : 0);

        /* Here we don't check high watermark because it's rescue data */
        int ret = bufferevent_write(conn->bev, remaining_data, remaining_len);
        if (ret == 0) {
            bufferevent_flush(conn->bev, EV_WRITE, BEV_FLUSH);
            return (ssize_t) len;
        }
        return -1;
    }

    /* Send failure */
    return -1;
}

ssize_t tglog_conn_sendv_with_bufferevent(tglog_conn *conn, const struct iovec *iov, int iovcnt) {
    if (!conn || conn->fd < 0 || !conn->bev || conn->network != TGLOG_NETWORK_TCP || !iov || iovcnt <= 0) {
        return -1;
    }

    /* Calculate total length */
    size_t total_len = 0;
    for (int i = 0; i < iovcnt; i++) {
        total_len += iov[i].iov_len;
    }

    if (total_len == 0) {
        return 0;
    }

    size_t out_buffer_len = tglog_conn_get_out_buffer_len(conn);

    /* If out_buf is not empty, use bufferevent_write */
    if (out_buffer_len > 0) {
        size_t high_watermark = conn->write_high_watermark > 0 ? conn->write_high_watermark : TGLOG_DEFAULT_WRITE_HIGH_WATERMARK;
        if (out_buffer_len > high_watermark) {
            tglog_error(conn->worker->logger, "out_buffer_len > high_watermark: %zu > %zu", out_buffer_len, high_watermark);
            return 0;
        }

        /* Write all iovec data to bufferevent */
        for (int i = 0; i < iovcnt; i++) {
            int ret = bufferevent_write(conn->bev, iov[i].iov_base, iov[i].iov_len);
            if (ret != 0) {
                tglog_error(conn->worker->logger, "bufferevent_write failed: %d", ret);
                return -1;
            }
        }
        bufferevent_flush(conn->bev, EV_WRITE, BEV_FLUSH);
        return (ssize_t) total_len;
    }

    /* If out_buf is empty, try to send directly with writev */
    bool eagain = false;
    ssize_t sent = writev(conn->fd, iov, iovcnt);
    if (sent < 0) {
        int sock_err = evutil_socket_geterror(conn->fd);
        eagain = sock_err == EAGAIN || sock_err == EWOULDBLOCK;
        tglog_error(conn->worker->logger, "writev failed: %s", tglog_socket_error_str(0));
    }

    /* Send success */
    if (sent == (ssize_t) total_len) {
        return sent;
    }

    if (eagain) {
        tglog_worker_on_conn_eagain(conn->worker);
    }

    /* Partial send success or EAGAIN/EWOULDBLOCK, write remaining data to bufferevent */
    if (sent > 0 || (sent == -1 && eagain)) {
        // size_t remaining_len = sent > 0 ? total_len - sent : total_len;
        // size_t written = sent > 0 ? sent : 0;

        /* Find which iovec to start from and offset within it */
        int start_iov = 0;
        size_t offset_in_iov = 0;

        if (sent > 0) {
            size_t accumulated = 0;
            for (int i = 0; i < iovcnt; i++) {
                if (accumulated + iov[i].iov_len > (size_t) sent) {
                    start_iov = i;
                    offset_in_iov = sent - accumulated;
                    break;
                }
                accumulated += iov[i].iov_len;
            }
        }

        /* Write remaining data to bufferevent */
        /* Here we don't check high watermark because it's rescue data */
        for (int i = start_iov; i < iovcnt; i++) {
            const char *data = (const char *) iov[i].iov_base + (i == start_iov ? offset_in_iov : 0);
            size_t len = iov[i].iov_len - (i == start_iov ? offset_in_iov : 0);

            if (len > 0) {
                int ret = bufferevent_write(conn->bev, data, len);
                if (ret != 0) {
                    tglog_error(conn->worker->logger, "bufferevent_write failed: %d", ret);
                    return -1;
                }
            }
        }
        bufferevent_flush(conn->bev, EV_WRITE, BEV_FLUSH);
        return (ssize_t) total_len;
    }

    /* Send failure */
    return -1;
}

ssize_t tglog_conn_send_with_buffer(tglog_conn *conn, struct evbuffer *buf, size_t len) {
    if (!conn || conn->fd < 0 || !conn->bev || conn->network != TGLOG_NETWORK_TCP || !buf) {
        return -1;
    }

    size_t out_buffer_len = tglog_conn_get_out_buffer_len(conn);
    size_t high_watermark = conn->write_high_watermark > 0 ? conn->write_high_watermark : TGLOG_DEFAULT_WRITE_HIGH_WATERMARK;
    if (out_buffer_len > high_watermark) {
        errno = ENOBUFS;
        return -1;
    }

    int ret = bufferevent_write_buffer(conn->bev, buf);
    if (ret == 0) {
        return (ssize_t) len;
    }

    return -1;
}

size_t tglog_conn_get_out_buffer_len(tglog_conn *conn) {
    if (!conn || !conn->bev || conn->network != TGLOG_NETWORK_TCP) {
        return 0;
    }

    return evbuffer_get_length(bufferevent_get_output(conn->bev));
}

void tglog_conn_flush(tglog_conn *conn) {
    if (!conn || !conn->bev || conn->network != TGLOG_NETWORK_TCP) {
        return;
    }
    bufferevent_flush(conn->bev, EV_WRITE, BEV_FLUSH);
}

int tglog_conn_write_buffer(tglog_conn *conn, struct evbuffer *buf) {
    if (!conn || !conn->bev || conn->network != TGLOG_NETWORK_TCP) {
        return -1;
    }

    /* tglog_conn_write_buffer is used to rescue data, we do not check high watermark here */
    return bufferevent_write_buffer(conn->bev, buf);
}

struct evbuffer *tglog_conn_get_pending_data(tglog_conn *conn) {
    if (!conn || !conn->bev || conn->network != TGLOG_NETWORK_TCP) {
        return NULL;
    }

    if (conn->pending_data) {
        return conn->pending_data;
    }

    size_t len = tglog_conn_get_out_buffer_len(conn);
    if (len == 0) {
        return NULL;
    }

    struct evbuffer *buf = evbuffer_new();
    if (!buf) {
        return NULL;
    }

    evbuffer_remove_buffer(bufferevent_get_output(conn->bev), buf, len);
    conn->pending_data = buf;
    return buf;
}

void tglog_conn_set_pending_data(tglog_conn *conn, struct evbuffer *pending_data) {
    if (!conn) {
        return;
    }

    conn->pending_data = pending_data;
}

struct evbuffer *tglog_conn_remove_pending_data(tglog_conn *conn) {
    struct evbuffer *buf = tglog_conn_get_pending_data(conn);
    if (!buf) {
        return NULL;
    }

    tglog_conn_set_pending_data(conn, NULL);
    return buf;
}

ssize_t tglog_conn_recv(tglog_conn *conn, void *buf, size_t buf_size, size_t *recv_len) {
    if (!conn || conn->fd < 0 || !conn->bev || !buf || buf_size == 0 || !recv_len) {
        return -1;
    }

    struct evbuffer *input = bufferevent_get_input(conn->bev);
    size_t avail = evbuffer_get_length(input);
    if (avail == 0) {
        *recv_len = 0;
        return 0;
    }

    size_t to_read = avail > buf_size ? buf_size : avail;
    *recv_len = evbuffer_remove(input, buf, to_read);
    return (ssize_t) (*recv_len);
}

#define TGLOG_DEFAULT_CONN_CAPACITY       16

tglog_conn_list *tglog_conn_list_create(int capacity) {
    capacity = capacity > TGLOG_DEFAULT_CONN_CAPACITY ? capacity : TGLOG_DEFAULT_CONN_CAPACITY;
    tglog_conn_list *list = (tglog_conn_list *) calloc(1, sizeof(tglog_conn_list));
    if (!list) {
        return NULL;
    }

    list->capacity = capacity;
    list->items = (tglog_conn **) calloc(capacity, sizeof(tglog_conn *));
    if (!list->items) {
        free(list);
        return NULL;
    }

    list->count = 0;
    return list;
}

bool tglog_conn_list_add(tglog_conn_list *list, tglog_conn *conn) {
    if (!list || !conn) {
        return false;
    }

    if (list->count + 1 > list->capacity) {
        tglog_conn **new_conns = realloc(list->items, sizeof(tglog_conn *) * (list->capacity * 2));
        if (!new_conns) {
            return false;
        }
        list->items = new_conns;
        list->capacity *= 2;
    }

    list->items[list->count++] = conn;
    return true;
}

bool tglog_conn_list_remove(tglog_conn_list *list, tglog_conn *conn) {
    bool found = false;
    int i = 0;
    for (i = list->count - 1; i >= 0; i--) {
        if (list->items[i] == conn) {
            if (i == list->count - 1) {
                list->items[i] = NULL;
            } else {
                list->items[i] = list->items[list->count - 1];
                list->items[list->count - 1] = NULL;
            }
            --list->count;
            found = true;
        }
    }

    return found;
}

bool tglog_conn_list_remove_by_addr(tglog_conn_list *list, char *addr) {
    bool found = false;
    int i = 0;
    for (i = list->count - 1; i >= 0; i--) {
        if (list->items[i]->endpoint && strcmp(list->items[i]->endpoint, addr) == 0) {
            if (i == list->count - 1) {
                list->items[i] = NULL;
            } else {
                list->items[i] = list->items[list->count - 1];
                list->items[list->count - 1] = NULL;
            }
            --list->count;
            found = true;
        }
    }

    return found;
}

int tglog_conn_list_get_count_by_addr(tglog_conn_list *list, char *addr) {
    int count = 0;
    int i = 0;
    for (i = 0; i < list->count; i++) {
        if (list->items[i]->endpoint && strcmp(list->items[i]->endpoint, addr) == 0) {
            count++;
        }
    }
    return count;
}

void tglog_conn_list_destroy(tglog_conn_list *list) {
    if (!list) {
        return;
    }

    int i = 0;
    for (i = 0; i < list->count; i++) {
        tglog_conn_close(list->items[i]);
    }

    free(list->items);
    free(list);
}