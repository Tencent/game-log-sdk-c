/* Standard library headers */

/* System headers */
#ifdef _WIN32
/* Windows specific headers */
#else

#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <unistd.h>

#endif

/* Third-party library headers */
#include "event2/event.h"
#include "event2/buffer.h"

/* Project headers */
#include "worker.h"
#include "conn.h"
#include "def.h"
#include "endpoint.h"
#include "err.h"
#include "metrics.h"

/* TGLog v1 has no response, 30s is enough */
#define TGLOG_DEFAULT_CONN_CLOSE_DELAY      30
#define TGLOG_MSG_PER_RUN                   3000

#define TGLOG_WORKER_NOTIFY_ENDPOINT EV_TIMEOUT     /* Endpoint updated notify */
#define TGLOG_WORKER_NOTIFY_CLOSE    EV_READ        /* Close worker notify */

enum tglog_worker_state {
    TGLOG_WORKER_STATE_INIT,            /* Worker initialized state */
    TGLOG_WORKER_STATE_READY,           /* Worker ready state */
    TGLOG_WORKER_STATE_CLOSING,         /* Worker closing state */
    TGLOG_WORKER_STATE_CLOSED           /* Worker closed state */
};

/* Backoff config in seconds to recover unavailable endpoints */
static tglog_backoff tglog_endpoint_recover_backoff = {
        10,     /* min */
        60,     /* max */
        2.0,    /* factor */
        0.5     /* jitter */
};

/* Backoff config for batch retry */
static tglog_backoff tglog_batch_retry_backoff = {
        100,    /* min (milliseconds) */
        10000,  /* max (milliseconds) */
        2.0,    /* factor */
        0.2     /* jitter */
};

static void *tglog_worker_thread(void *user_data) {
    tglog_worker *worker = (tglog_worker *) user_data;
    struct event_base *base = worker->base;

    tglog_debug(worker->logger, "Worker: %d thread started", worker->id);
    event_base_dispatch(base);
    tglog_debug(worker->logger, "Worker: %d thread stopped", worker->id);
    return NULL;
}

static void tglog_worker_set_endpoint_retry_count(tglog_worker *worker, char *addr, int value) {
    ck_ht_hash_t hash;
    ck_ht_hash(&hash, &worker->endpoint_retry_counts, addr, strlen(addr));

    ck_ht_entry_t entry;
    ck_ht_entry_key_set(&entry, addr, strlen(addr));
    bool ok = ck_ht_get_spmc(&worker->endpoint_retry_counts, hash, &entry);
    if (ok) {
        /* Use the old key pointer here */
        ck_ht_entry_set(&entry, hash, ck_ht_entry_key(&entry), strlen(addr), (void *) (intptr_t) value);
        if (ck_ht_set_spmc(&worker->endpoint_retry_counts, hash, &entry)) {
            tglog_info(worker->logger, "Set endpoint retry count, worker: %d , addr: %s, retry_count: %d", worker->id, addr, value);
        } else {
            tglog_error(worker->logger, "Set endpoint retry count failed, worker: %d , addr: %s, retry_count: %d", worker->id, addr, value);
        }
        return;
    }

    /* Duplicate the addr here */
    ck_ht_entry_set(&entry, hash, strdup(addr), strlen(addr), (void *) (intptr_t) 1);
    if (!ck_ht_set_spmc(&worker->endpoint_retry_counts, hash, &entry)) {
        tglog_error(worker->logger, "Set endpoint retry count failed, worker: %d , addr: %s, retry_count: %d", worker->id, addr, value);
        free(ck_ht_entry_key(&entry));
    }

    tglog_info(worker->logger, "Set endpoint retry count, worker: %d , addr: %s, retry_count: %d", worker->id, addr, value);
}

static int tglog_worker_get_endpoint_retry_count(tglog_worker *worker, char *addr) {
    ck_ht_hash_t hash;
    ck_ht_hash(&hash, &worker->endpoint_retry_counts, addr, strlen(addr));

    ck_ht_entry_t entry;
    ck_ht_entry_key_set(&entry, addr, strlen(addr));
    bool ok = ck_ht_get_spmc(&worker->endpoint_retry_counts, hash, &entry);
    if (!ok) {
        tglog_error(worker->logger, "Get endpoint retry count failed, worker: %d , addr: %s", worker->id, addr);
        return 0;
    }

    int value = (int) (intptr_t) ck_ht_entry_value(&entry);
    return value;
}

static void tglog_worker_del_endpoint_retry_count(tglog_worker *worker, char *addr) {
    ck_ht_hash_t hash;
    ck_ht_hash(&hash, &worker->endpoint_retry_counts, addr, strlen(addr));

    ck_ht_entry_t entry;
    ck_ht_entry_key_set(&entry, addr, strlen(addr));
    if (ck_ht_remove_spmc(&worker->endpoint_retry_counts, hash, &entry)) {
        /* Free the key */
        free(ck_ht_entry_key(&entry));
        return;
    }

    tglog_info(worker->logger, "Delete endpoint retry count but not found, worker: %d , addr: %s", worker->id, addr);
}

static int tglog_worker_get_conn_count_by_addr(tglog_worker *worker, char *addr) {
    return tglog_conn_list_get_count_by_addr(worker->conns, addr);
}

static void tglog_worker_mark_endpoint_unavailable(tglog_worker *worker, char *addr) {
    int ep_count = (int) worker->endpoints->count;
    /* When we dial a new conn, we will ignore the unavailable endpoint, if there is only 1 endpoint,
     * we don't mark it unavailable anymore, or we will have no endpoint to dial.
     */
    if (ep_count <= 1) {
        tglog_warn(worker->logger, "Skip to mark endpoint unavailable as there is only one endpoint, worker: %d, addr: %s, endpoint count: %d", worker->id, addr, ep_count);
        return;
    }

    if (tglog_worker_get_conn_count_by_addr(worker, addr) > 0) {
        tglog_warn(worker->logger, "Skip to mark endpoint unavailable as there are still connections, worker: %d, addr: %s", worker->id, addr);
        return;
    }

    ck_ht_hash_t hash;
    ck_ht_hash(&hash, &worker->unavailable_endpoints, addr, strlen(addr));

    ck_ht_entry_t entry;
    ck_ht_entry_key_set(&entry, addr, strlen(addr));
    bool ok = ck_ht_get_spmc(&worker->unavailable_endpoints, hash, &entry);
    if (ok) {
        tglog_warn(worker->logger, "Endpoint already marked unavailable, worker: %d, addr: %s", worker->id, addr);
        return;
    }

    time_t value = time(NULL);
    /* Duplicate the addr, we need to free the key when we delete it or when we destroy the worker */
    ck_ht_entry_set(&entry, hash, strdup(addr), strlen(addr), (void *) (intptr_t) value);
    if (!ck_ht_put_spmc(&worker->unavailable_endpoints, hash, &entry)) {
        tglog_error(worker->logger, "Mark endpoint unavailable failed, worker: %d, addr: %s", worker->id, addr);
        free(ck_ht_entry_key(&entry));
    }

    tglog_warn(worker->logger, "Mark endpoint unavailable, worker: %d, addr: %s", worker->id, addr);
    tglog_worker_set_endpoint_retry_count(worker, addr, 0);
}

static time_t tglog_worker_is_endpoint_unavailable(tglog_worker *worker, char *addr) {
    ck_ht_hash_t hash;
    ck_ht_hash(&hash, &worker->unavailable_endpoints, addr, strlen(addr));

    ck_ht_entry_t entry;
    ck_ht_entry_key_set(&entry, addr, strlen(addr));
    if (!ck_ht_get_spmc(&worker->unavailable_endpoints, hash, &entry)) {
        return 0;
    }

    return (time_t) (intptr_t) ck_ht_entry_value(&entry);
}

static void tglog_worker_del_unavailable_endpoint(tglog_worker *worker, char *addr) {
    ck_ht_hash_t hash;
    ck_ht_hash(&hash, &worker->unavailable_endpoints, addr, strlen(addr));

    ck_ht_entry_t entry;
    ck_ht_entry_key_set(&entry, addr, strlen(addr));
    if (ck_ht_remove_spmc(&worker->unavailable_endpoints, hash, &entry)) {
        free(ck_ht_entry_key(&entry));
        return;
    }

    tglog_info(worker->logger, "Delete unavailable endpoint but not found, worker: %d, addr: %s", worker->id, addr);
}

static bool tglog_worker_set_delay_close_conn(tglog_worker *worker, tglog_conn *conn) {
    ck_ht_hash_t hash;
    ck_ht_hash(&hash, &worker->delay_close_conns, conn, sizeof(conn));

    ck_ht_entry_t entry;
    ck_ht_entry_set(&entry, hash, conn, sizeof(conn), (void *) (intptr_t) 1);
    if (!ck_ht_set_spmc(&worker->delay_close_conns, hash, &entry)) {
        tglog_error(worker->logger, "Track delay close conn failed, worker: %d, addr: %s", worker->id, conn->endpoint);
        return false;
    }

    return true;
}

static void tglog_worker_del_delay_close_conn(tglog_worker *worker, tglog_conn *conn) {
    ck_ht_hash_t hash;
    ck_ht_hash(&hash, &worker->delay_close_conns, conn, sizeof(conn));

    ck_ht_entry_t entry;
    ck_ht_entry_set(&entry, hash, conn, sizeof(conn), (void *) (intptr_t) 1);
    if (ck_ht_remove_spmc(&worker->delay_close_conns, hash, &entry)) {
        tglog_info(worker->logger, "Delete delay close conn, worker: %d, addr: %s", worker->id, conn->endpoint);
        return;
    }

    tglog_error(worker->logger, "Delete delay close conn failed, worker: %d, addr: %s", worker->id, conn->endpoint);
}

static tglog_conn *tglog_worker_get_conn(tglog_worker *worker);

static bool tglog_worker_rescue_pending_data(tglog_worker *worker, tglog_conn *conn) {
    /* Remove pending data from connection */
    /* Because TCP is a stream-oriented transmission protocol, the pending data may contain incomplete logs */
    struct evbuffer *buf = tglog_conn_remove_pending_data(conn);
    if (!buf) {
        return true;
    }

    size_t len = evbuffer_get_length(buf);
    tglog_conn *alt_conn = tglog_worker_get_conn(worker);
    if (!alt_conn) {
        tglog_warn(worker->logger, "Get alternative conn to rescue pending data failed, worker: %d", worker->id);
        /* Restore pending data back to connection */
        tglog_conn_set_pending_data(conn, buf);
        return false;
    }

    if (tglog_conn_write_buffer(alt_conn, buf) != 0) {
        tglog_error(worker->logger, "Rescue pending data failed, worker: %d, addr: %s", worker->id, alt_conn->endpoint);
        /* Restore pending data back to connection */
        tglog_conn_set_pending_data(conn, buf);
        return false;
    }

    tglog_info(worker->logger, "Rescue pending data, worker: %d, len: %zu", worker->id, len);
    tglog_metrics_inc_rescued_msg_bytes(worker->metrics, worker->id, len);
    return true;
}


static void tglog_worker_close_conn_by_timer(evutil_socket_t fd, short what, void *user_data) {
    (void) fd;
    (void) what;
    tglog_conn *conn = user_data;
    tglog_worker *worker = conn->worker;
    tglog_debug(worker->logger, "Worker: %d, addr: %s", worker->id, conn->endpoint);

    tglog_worker_del_delay_close_conn(worker, conn);
    tglog_worker_rescue_pending_data(worker, conn);
    tglog_conn_close(conn);
}

static void tglog_worker_close_conn_delay(tglog_worker *worker, tglog_conn *conn, int delay_seconds) {
    if (!conn) {
        return;
    }

    tglog_debug(worker->logger, "Worker: %d, addr:%s, delay: %d", worker->id, conn->endpoint, delay_seconds);

    if (delay_seconds <= 0) {
        if (tglog_worker_rescue_pending_data(worker, conn)) {
            tglog_info(worker->logger, "Close conn immediately, worker: %d, addr: %s", worker->id, conn->endpoint);
            /* Rescue pending data succeeded, close conn immediately */
            tglog_conn_close(conn);
            return;
        }

        /* Rescue pending data failed, use default delay */
        delay_seconds = TGLOG_DEFAULT_CONN_CLOSE_DELAY;
    }

    /* Add to delay close conn map and bind a timer to it */
    struct timeval interval = {delay_seconds, 0};
    struct event *close_event = event_new(worker->base, -1, EV_TIMEOUT, tglog_worker_close_conn_by_timer, conn);
    if (close_event == NULL) {
        tglog_error(worker->logger, "Create timer to close connection failed, should not be here, worker: %d, addr: %s", worker->id, conn->endpoint);
        /* Create timer failed, rescue pending data and close conn immediately */
        tglog_worker_rescue_pending_data(worker, conn);
        tglog_conn_close(conn);
        return;
    }

    conn->close_event = close_event;
    if (!tglog_worker_set_delay_close_conn(worker, conn)) {
        tglog_error(worker->logger, "Track delay close conn failed, should not be here, worker: %d, addr: %s", worker->id, conn->endpoint);
        /* Track delay close conn failed, rescue pending data and close conn immediately */
        tglog_worker_rescue_pending_data(worker, conn);
        tglog_conn_close(conn); /* tglog_conn_close() will free the close_event */
        return;
    }

    event_add(close_event, &interval);
}

static bool tglog_worker_add_connecting_conn(tglog_worker *worker, tglog_conn *conn) {
    if (!tglog_conn_list_add(worker->connecting_conns, conn)) {
        tglog_fatal(worker->logger, "Realloc worker connecting conns array failed, conn memory may leak, worker: %d", worker->id);
        return false;
    }

    tglog_debug(worker->logger, "Add a connecting conn, worker: %d, addr: %s, conn: %p, current connecting conn count: %d",
                worker->id, conn->endpoint, conn, worker->connecting_conns->count);
    return true;
}

static void tglog_worker_remove_connecting_conn(tglog_worker *worker, tglog_conn *conn) {
    if (tglog_conn_list_remove(worker->connecting_conns, conn)) {
        tglog_debug(worker->logger, "Remove a connecting conn, worker: %d, addr: %s, conn: %p, current connecting conn count: %d",
                    worker->id, conn->endpoint, conn, worker->connecting_conns->count);
    }
}

static bool tglog_worker_add_conn(tglog_worker *worker, tglog_conn *conn) {
    if (!tglog_conn_list_add(worker->conns, conn)) {
        tglog_fatal(worker->logger, "Realloc worker conns array failed, conn memory may leak, worker: %d", worker->id);
        return false;
    }

    tglog_info(worker->logger, "Add a new conn, worker: %d, addr: %s, conn: %p, current conn count: %d",
               worker->id, conn->endpoint, conn, worker->conns->count);
    return true;
}

static bool tglog_worker_remove_conn(tglog_worker *worker, tglog_conn *conn) {
    if (tglog_conn_list_remove(worker->conns, conn)) {
        tglog_info(worker->logger, "Remove an old conn, worker: %d, addr: %s, conn: %p, current conn count: %d",
                   worker->id, conn->endpoint, conn, worker->conns->count);
        return true;
    }
    return false;
}

static tglog_conn *tglog_worker_dial_new_conn(tglog_worker *worker, char *addr, tglog_conn_create_type create_type, void *user_data);

static bool tglog_worker_supplement_conn(tglog_worker *worker) {
    tglog_info(worker->logger, "A connection was closed, try to supplement a new one, worker: %d", worker->id);

    /* Try to maintain multiple connections per endpoint */
    int i;

    for (i = 0; i < worker->endpoints->count; i++) {
        tglog_endpoint *ep = worker->endpoints->items[i];
        if (tglog_worker_is_endpoint_unavailable(worker, ep->addr)) {
            continue;
        }

        /* Count existing connections for this endpoint */
        int existing_conns = 0;
        int j;
        for (j = 0; j < worker->conns->count; j++) {
            if (strcmp(worker->conns->items[j]->endpoint, ep->addr) == 0) {
                existing_conns++;
            }
        }

        /* Check connecting connections too */
        for (j = 0; j < worker->connecting_conns->count; j++) {
            if (strcmp(worker->connecting_conns->items[j]->endpoint, ep->addr) == 0) {
                existing_conns++;
            }
        }

        /* Create connections if below target */
        if (existing_conns < worker->conns_per_endpoint) {
            tglog_conn *new_conn = tglog_worker_dial_new_conn(worker, ep->addr, CREATE_BY_SUPPLEMENT, NULL);
            if (!new_conn) {
                tglog_worker_mark_endpoint_unavailable(worker, ep->addr);
                continue;
            }

            if (!tglog_worker_add_connecting_conn(worker, new_conn)) {
                tglog_error(worker->logger, "Add connecting conn failed, close the conn to avoid memory leak, worker: %d, addr: %s", worker->id, ep->addr);
                tglog_conn_close(new_conn);
                return false;
            }
            return true;
        } else {
            tglog_warn(worker->logger, "Existing connections for endpoint %s is %d, which is greater than target %d",
                       ep->addr, existing_conns, worker->conns_per_endpoint);
            continue;
        }
    }

    if (worker->endpoints->count == 0) {
        tglog_warn(worker->logger, "No endpoints available");
        return false;
    }

    /* Fallback: Randomly select an endpoint and force create connection
     *
     * This fallback logic handles critical scenarios where the first loop cannot create connections:
     *
     * 1. Partial endpoint failure with load rebalancing:
     *    - Example: Endpoints A and B each have 3 connections (conns_per_endpoint=3)
     *    - A's server fails, all 3 connections close and trigger supplement_conn
     *    - A is marked unavailable, first loop skips it
     *    - B already has 3 connections (>= conns_per_endpoint), first loop skips it too
     *    - Without fallback: No new connections created, system loses capacity
     *    - With fallback: Randomly select B and create 4th/5th/6th connection
     *    - Result: B temporarily exceeds conns_per_endpoint to maintain total capacity
     *    - Trade-off: Sacrifice load balancing for high availability
     *
     * 2. Give unavailable endpoints a chance to recover:
     *    - Random selection may pick an unavailable endpoint (e.g., A)
     *    - If A has recovered from transient failure, connection succeeds immediately
     *    - This provides faster recovery than waiting for recover_endpoint_event (30s timer)
     *
     * 3. Connection counting race conditions:
     *    - Connections in connecting_conns may ultimately fail
     *    - Connections being closed may still be counted in existing_conns
     *    - Fallback ensures at least one connection attempt is made
     *
     * Note: This logic ignores both unavailable status and conns_per_endpoint limit,
     * prioritizing system availability over strict connection distribution.
     */
    int random_index = rand() % worker->endpoints->count;
    tglog_endpoint *ep = worker->endpoints->items[random_index];
    tglog_conn *new_conn = tglog_worker_dial_new_conn(worker, ep->addr, CREATE_BY_SUPPLEMENT, NULL);
    if (!new_conn) {
        return false;
    }

    if (!tglog_worker_add_connecting_conn(worker, new_conn)) {
        tglog_error(worker->logger, "Add connecting conn failed, close the conn to avoid memory leak, worker: %d, addr: %s", worker->id, ep->addr);
        tglog_conn_close(new_conn);
        return false;
    }
    return true;
}

static bool tglog_worker_replace_conn(tglog_worker *worker, tglog_conn *old_conn) {
    tglog_debug(worker->logger, "Worker: %d", worker->id);

    /* Use round-robin to select endpoint for better load balancing */
    int i;
    for (i = 0; i < worker->endpoints->count; i++) {
        tglog_endpoint *ep = worker->endpoints->items[worker->endpoint_index % worker->endpoints->count];
        worker->endpoint_index++;

        if (tglog_worker_is_endpoint_unavailable(worker, ep->addr)) {
            continue;
        }

        tglog_conn *new_conn = tglog_worker_dial_new_conn(worker, ep->addr, CREATE_BY_REPLACE, old_conn);
        if (!new_conn) {
            tglog_worker_mark_endpoint_unavailable(worker, ep->addr);
            continue;
        }

        if (!tglog_worker_add_connecting_conn(worker, new_conn)) {
            tglog_error(worker->logger, "Add connecting conn failed, close the conn to avoid memory leak, worker: %d, addr: %s", worker->id, ep->addr);
            tglog_conn_close(new_conn);
            return false;
        }
        return true;
    }

    return false;
}

static void tglog_worker_on_read(struct bufferevent *bev, void *user_data) {
    tglog_conn *conn = user_data;
    tglog_worker *worker = conn->worker;
    tglog_debug(worker->logger, "Worker: %d", worker->id);

    bufferevent_disable(bev, EV_READ);
    /* TGLog v1 has no response, we do nothing here */
}

static void tglog_worker_on_conn_connected(tglog_worker *worker, tglog_conn *conn) {
    tglog_debug(worker->logger, "Worker: %d", worker->id);

    /* Update metrics */
    tglog_metrics_inc_opened_conn_count(worker->client->metrics, worker->id, 1);

    /* Remove from connecting list and add to connected list */
    tglog_worker_remove_connecting_conn(worker, conn);
    tglog_worker_add_conn(worker, conn);

    /* Handle different connection types */
    switch (conn->create_type) {
        case CREATE_BY_INIT:
            tglog_info(worker->logger, "Create conn to init worker, worker: %d, addr: %s", worker->id, conn->endpoint);
#ifdef _WIN32
            EnterCriticalSection(&worker->ready_lock);
            worker->ready_conn++;
            if (worker->ready_conn + worker->failed_conn >= worker->init_conn_count) {
                if (worker->ready_conn > 0) {
                    ck_pr_store_32(&worker->state, TGLOG_WORKER_STATE_READY);
                }
                WakeConditionVariable(&worker->ready_cond);
            }
            LeaveCriticalSection(&worker->ready_lock);
#else
            pthread_mutex_lock(&worker->ready_lock);
            worker->ready_conn++;
            if (worker->ready_conn + worker->failed_conn >= worker->init_conn_count) {
                if (worker->ready_conn > 0) {
                    ck_pr_store_32(&worker->state, TGLOG_WORKER_STATE_READY);
                }
                pthread_cond_signal(&worker->ready_cond);
            }
            pthread_mutex_unlock(&worker->ready_lock);
#endif
            break;

        case CREATE_BY_ENDPOINT:
            tglog_info(worker->logger, "Create conn for new endpoint, worker: %d, addr: %s", worker->id, conn->endpoint);
            break;

        case CREATE_BY_SUPPLEMENT:
            tglog_info(worker->logger, "Create conn to supplement, worker: %d, addr: %s", worker->id, conn->endpoint);
            break;

        case CREATE_BY_REPLACE:
            tglog_info(worker->logger, "Create conn to replace an expired one, worker: %d, addr: %s", worker->id, conn->endpoint);
            if (conn->user_data) {
                tglog_worker_remove_conn(worker, conn->user_data);
                tglog_worker_close_conn_delay(worker, conn->user_data, TGLOG_DEFAULT_CONN_CLOSE_DELAY);
                conn->user_data = NULL;
            }
            break;

        case CREATE_BY_RECOVER:
            tglog_info(worker->logger, "Create conn to recover endpoint, worker: %d, addr: %s", worker->id, conn->endpoint);
            if (conn->user_data) {
                tglog_worker_del_unavailable_endpoint(worker, conn->user_data);
                tglog_worker_del_endpoint_retry_count(worker, conn->user_data);
                free(conn->user_data);
                conn->user_data = NULL;
            }
            break;

        default:
            tglog_info(worker->logger, "Create conn for unknown type, worker: %d, addr: %s, type: %d",
                       worker->id, conn->endpoint, conn->create_type);
            break;
    }

    /* Reset create type */
    conn->create_type = CREATE_BY_NONE;
}

static void tglog_worker_on_conn_error(tglog_worker *worker, tglog_conn *conn, short what) {
    tglog_error(worker->logger, "Worker: %d, addr: %s, event: %d", worker->id, conn->endpoint, what);

    /* Update metrics */
    tglog_metrics_inc_error_conn_count(worker->client->metrics, worker->id, 1);

    /* Remove conn from the available conn list */
    tglog_worker_remove_conn(worker, conn);

    /* Remove conn from the connecting conn list */
    tglog_worker_remove_connecting_conn(worker, conn);

    /* Mark this endpoint as unavailable */
    tglog_worker_mark_endpoint_unavailable(worker, conn->endpoint);

    /* Handle different connection types */
    switch (conn->create_type) {
        case CREATE_BY_INIT:
            tglog_info(worker->logger, "Create conn to init failed, worker: %d, addr: %s", worker->id, conn->endpoint);
#ifdef _WIN32
            EnterCriticalSection(&worker->ready_lock);
            worker->failed_conn++;
            if (worker->ready_conn + worker->failed_conn >= worker->init_conn_count) {
                if (worker->ready_conn > 0) {
                    ck_pr_store_32(&worker->state, TGLOG_WORKER_STATE_READY);
                }
                WakeConditionVariable(&worker->ready_cond);
            }
            LeaveCriticalSection(&worker->ready_lock);
#else
            pthread_mutex_lock(&worker->ready_lock);
            worker->failed_conn++;
            if (worker->ready_conn + worker->failed_conn >= worker->init_conn_count) {
                if (worker->ready_conn > 0) {
                    ck_pr_store_32(&worker->state, TGLOG_WORKER_STATE_READY);
                }
                pthread_cond_signal(&worker->ready_cond);
            }
            pthread_mutex_unlock(&worker->ready_lock);
#endif
            break;

        case CREATE_BY_ENDPOINT:
            tglog_info(worker->logger, "Create conn for new endpoint failed, worker: %d, addr: %s", worker->id, conn->endpoint);
            break;

        case CREATE_BY_SUPPLEMENT:
            tglog_info(worker->logger, "Create conn to supplement failed, worker: %d, addr: %s", worker->id, conn->endpoint);
            break;

        case CREATE_BY_REPLACE:
            tglog_info(worker->logger, "Create conn to replace an expired one failed, worker: %d, addr: %s", worker->id, conn->endpoint);
            break;

        case CREATE_BY_RECOVER:
            tglog_info(worker->logger, "Create conn to recover endpoint failed, worker: %d, addr: %s", worker->id, conn->endpoint);
            if (conn->user_data) {
                free(conn->user_data);
                conn->user_data = NULL;
            }
            break;

        case CREATE_BY_NONE:
            if (!tglog_worker_supplement_conn(worker)) {
                tglog_warn(worker->logger, "A connection was closed, try to supplement one but no available endpoints, worker: %d", worker->id);
            }
            break;

        default:
            break;
    }

    /* If there is no pending data, close the connection 5s later */
    size_t out_buffer_len = tglog_conn_get_out_buffer_len(conn);
    tglog_worker_close_conn_delay(worker, conn, out_buffer_len > 0 ? TGLOG_DEFAULT_CONN_CLOSE_DELAY : 5);
}

static void tglog_worker_on_event(struct bufferevent *bev, short what, void *user_data) {
    (void) bev;  /* Unused */
    tglog_conn *conn = (tglog_conn *) user_data;
    tglog_worker *worker = conn->worker;

    tglog_debug(worker->logger, "Worker: %d", worker->id);

    if (what & BEV_EVENT_CONNECTED) {
        tglog_worker_on_conn_connected(worker, conn);
    } else {
        tglog_worker_on_conn_error(worker, conn, what);
    }
}

static bool tglog_worker_parse_host_port(tglog_worker *worker, const char *addr, char *host, size_t host_size, int *port) {
    if (!addr || !host || !port) {
        return false;
    }

    /* Find colon separator */
    const char *colon = strrchr(addr, ':');
    if (!colon) {
        tglog_error(worker->logger, "Invalid address format, should be host:port, got: %s", addr);
        return false;
    }

    /* Copy host part */
    size_t host_length = colon - addr;
    if (host_length >= host_size) {
        tglog_error(worker->logger, "Host buffer too small for: %s", addr);
        return false;
    }

    strncpy(host, addr, host_length);
    host[host_length] = '\0';

    /* Parse port */
    *port = atoi(colon + 1);
    if (*port <= 0 || *port > 65535) {
        tglog_error(worker->logger, "Invalid port in address: %s", addr);
        return false;
    }

    return true;
}

static int tglog_worker_init_socket(tglog_worker *worker, char *addr) {
    int sock_type = (worker->client->conf.network == TGLOG_NETWORK_TCP) ? SOCK_STREAM : SOCK_DGRAM;
    int sock = socket(AF_INET, sock_type, 0);

    if (sock < 0) {
        tglog_error(worker->logger, "Failed to create socket for %s: %s, worker: %d",
                    addr, strerror(errno), worker->id);
        return -1;
    }

    /* Set non-blocking */
    if (evutil_make_socket_nonblocking(sock) < 0) {
        tglog_error(worker->logger, "Failed to set non-blocking for %s, worker: %d", addr, worker->id);
        evutil_closesocket(sock);
        return -1;
    }

    /* Set socket options */
    if (worker->client->conf.socket_recv_buffer_size > 0) {
        setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &worker->client->conf.socket_recv_buffer_size, sizeof(worker->client->conf.socket_recv_buffer_size));
    }
    if (worker->client->conf.socket_send_buffer_size > 0) {
        setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &worker->client->conf.socket_send_buffer_size, sizeof(worker->client->conf.socket_send_buffer_size));
    }

    if (worker->client->conf.network == TGLOG_NETWORK_TCP) {
        int keepalive = 1;
        int keep_idle = 300;

        setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));

//        int no_delay = 1;
//        setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &no_delay, sizeof(no_delay));
//        int quick_ack = 1;
//        setsockopt(sock, IPPROTO_TCP, TCP_QUICKACK, &quick_ack, sizeof(quick_ack));
#ifdef  __APPLE__
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPALIVE, &keep_idle, sizeof(keep_idle));
#else
        int keep_interval = 60;
        int keep_cnt = 5;
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keep_idle, sizeof(keep_idle));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keep_interval, sizeof(keep_interval));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keep_cnt, sizeof(keep_cnt));
#endif
    }

    return sock;
}

static tglog_conn *tglog_worker_dial_new_conn(tglog_worker *worker, char *addr, tglog_conn_create_type create_type, void *user_data) {
    char host[256];
    int port = 0;

    /* Parse address */
    if (!tglog_worker_parse_host_port(worker, addr, host, sizeof(host), &port)) {
        tglog_error(worker->logger, "Parse address failed, worker: %d, addr: %s", worker->id, addr);
        return NULL;
    }

    /* Initialize socket */
    char *network = worker->client->conf.network == TGLOG_NETWORK_TCP ? "tcp" : "udp";
    int sock = tglog_worker_init_socket(worker, addr);
    if (sock < 0) {
        tglog_error(worker->logger, "Create socket failed, worker: %d, addr: %s://%s, errno: %d",
                    worker->id, network, addr, errno);
        return NULL;
    }

    /* Create bufferevent */
    struct bufferevent *bev = bufferevent_socket_new(worker->base, sock, BEV_OPT_CLOSE_ON_FREE);
    if (!bev) {
        tglog_error(worker->logger, "Connect to server failed, worker: %d, addr: %s://%s, errno: %d",
                    worker->id, network, addr, evutil_socket_geterror(sock));
        evutil_closesocket(sock);
        return NULL;
    }

    /* Set connect timeout */
    int timeout = worker->client->conf.conn_timeout;
    if (timeout <= 0) {
        timeout = 3000;
    }

    struct timeval tv;
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;
    bufferevent_set_timeouts(bev, &tv, NULL);

    /* Create connection object */
    tglog_conn *conn = tglog_conn_create(worker, create_type, worker->client->conf.network, addr, sock, bev, user_data,
                                         worker->client->conf.write_high_watermark, worker->client->conf.max_conn_lifetime);
    if (!conn) {
        bufferevent_free(bev);
        evutil_closesocket(sock);
        return NULL;
    }

    /* Set callbacks - note: only enable read event, write event is disabled */
    bufferevent_setcb(bev, tglog_worker_on_read, NULL, tglog_worker_on_event, conn);
    /* because tglog v1 has no response, read event is disabled either */
    /* bufferevent_enable(bev, EV_READ); */

    /* Prepare address and connect */
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &serv_addr.sin_addr);

    if (bufferevent_socket_connect(bev, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        tglog_error(worker->logger, "Connect to server error, worker: %d, addr: %s://%s, errno: %d",
                    worker->id, network, addr, evutil_socket_geterror(sock));
        /* tglog_conn_close will free bev */
        tglog_conn_close(conn);
        return NULL;
    }

    tglog_debug(worker->logger, "Connecting to %s://%s, worker: %d", network, addr, worker->id);
    return conn;
}

static bool tglog_worker_init_conns(tglog_worker *worker) {
    int i = 0;
    /* Create multiple connections per endpoint for better load balancing */

    for (i = 0; i < worker->endpoints->count; i++) {
        int j;
        int successful_conns = 0;

        for (j = 0; j < worker->conns_per_endpoint; j++) {
            tglog_conn *conn = tglog_worker_dial_new_conn(worker, worker->endpoints->items[i]->addr, CREATE_BY_INIT, NULL);
            if (!conn) {
                tglog_warn(worker->logger, "Dial new connection failed for endpoint %s, connection %d/%d",
                           worker->endpoints->items[i]->addr, j + 1, worker->conns_per_endpoint);
                continue;
            }

            if (tglog_worker_add_connecting_conn(worker, conn)) {
                worker->init_conn_count++;
                successful_conns++;
            } else {
                tglog_error(worker->logger, "Add connecting conn failed, close the conn to avoid memory leak, worker: %d, addr: %s",
                            worker->id, worker->endpoints->items[i]->addr);
                tglog_conn_close(conn);
            }
        }

        /* Mark endpoint unavailable only if all connection attempts failed */
        if (successful_conns == 0) {
            tglog_worker_mark_endpoint_unavailable(worker, worker->endpoints->items[i]->addr);
        }
    }

    if (worker->connecting_conns->count == 0) {
        tglog_error(worker->logger, "No available endpoints, worker: %d", worker->id);
        return false;
    }

    return true;
}

static void *tglog_ht_malloc(size_t r) {
    return malloc(r);
}

static void tglog_ht_free(void *p, size_t b, bool r) {
    (void) b;
    (void) r;
    free(p);
}

static struct ck_malloc tglog_ck_allocator = {
        .malloc = tglog_ht_malloc,
        .free = tglog_ht_free
};

static void tglog_worker_recover_endpoints(evutil_socket_t fd, short what, void *user_data) {
    (void) fd;
    (void) what;
    tglog_worker *worker = (tglog_worker *) user_data;
    tglog_debug(worker->logger, "Worker: %d", worker->id);

    /* Check and recover unavailable endpoints with backoff */
    ck_ht_iterator_t iterator = CK_HT_ITERATOR_INITIALIZER;
    ck_ht_entry_t *cursor = NULL;
    while (ck_ht_next(&worker->unavailable_endpoints, &iterator, &cursor)) {
        void *key_ptr = ck_ht_entry_key(cursor);
        time_t unavailable_time = (time_t) (uintptr_t) ck_ht_entry_value(cursor);
        int retry_count = tglog_worker_get_endpoint_retry_count(worker, key_ptr);
        time_t now = time(NULL);
        unsigned int next_retry_time = tglog_backoff_next_interval(&tglog_endpoint_recover_backoff, retry_count, true);

        if ((now - unavailable_time) > next_retry_time) {
            /* Try to create connections when recovering */
            int i;
            int successful_conns = 0;

            for (i = 0; i < worker->conns_per_endpoint; i++) {
                char *endpoint_addr = strdup((char *) key_ptr);
                tglog_conn *new_conn = tglog_worker_dial_new_conn(worker, key_ptr, CREATE_BY_RECOVER, endpoint_addr);

                if (!new_conn) {
                    tglog_info(worker->logger, "Failed to recover endpoint connection %d/%d, worker: %d, addr: %s, retry: %d",
                               i + 1, worker->conns_per_endpoint, worker->id, key_ptr, retry_count);
                    free(endpoint_addr);
                    continue;
                }

                tglog_info(worker->logger, "Trying to recover endpoint connection %d/%d, worker: %d, addr: %s",
                           i + 1, worker->conns_per_endpoint, worker->id, key_ptr);
                if (tglog_worker_add_connecting_conn(worker, new_conn)) {
                    successful_conns++;
                } else {
                    tglog_error(worker->logger, "Add connecting conn failed, close the conn to avoid memory leak, worker: %d, addr: %s",
                                worker->id, (char *) key_ptr);
                    /* Free endpoint_addr before closing conn, as tglog_conn_close won't trigger on_conn_error callback */
                    if (new_conn->user_data) {
                        free(new_conn->user_data);
                        new_conn->user_data = NULL;
                    }
                    tglog_conn_close(new_conn);
                }
            }

            /* Increase retry count after attempting recovery */
            retry_count++;
            tglog_worker_set_endpoint_retry_count(worker, key_ptr, retry_count);
        }
    }

    /* Supplement connections to maintain target count */
    int target_total_conns = worker->endpoints->count * worker->conns_per_endpoint;
    int current_total_conns = worker->conns->count + worker->connecting_conns->count;

    while (current_total_conns < target_total_conns) {
        if (!tglog_worker_supplement_conn(worker)) {
            break;
        }
        current_total_conns++;
    }
}

static void tglog_worker_emergency_supplement(evutil_socket_t fd, short what, void *user_data) {
    (void) fd;
    (void) what;
    tglog_worker *worker = user_data;
    tglog_debug(worker->logger, "Worker: %d", worker->id);

    /* Check if there are no available connections */
    if (worker->conns->count == 0) {
        tglog_info(worker->logger, "Emergency supplement check: no connections available, try to supplement, worker: %d", worker->id);
        tglog_worker_supplement_conn(worker);
        worker->last_emergency_supplement_time = tglog_get_time_ms();
    }
}

static bool tglog_worker_is_conn_expired(tglog_worker *worker, tglog_conn *conn) {
    if (worker == NULL || conn == NULL) {
        return false;
    }

    if (worker->client->conf.max_conn_lifetime == 0) {
        return false;
    }

    return conn->expire_time < time(NULL);
}

static void tglog_worker_replace_expired_conns(evutil_socket_t fd, short what, void *user_data) {
    (void) fd;
    (void) what;
    tglog_worker *worker = (tglog_worker *) user_data;
    tglog_debug(worker->logger, "Worker: %d", worker->id);

    int i;
    for (i = worker->conns->count - 1; i >= 0; i--) {
        tglog_conn *conn = worker->conns->items[i];
        if (tglog_worker_is_conn_expired(worker, conn)) {
            tglog_info(worker->logger, "Connection expired, try to create a new one to replace it, worker: %d, addr: %s", worker->id, conn->endpoint);
            tglog_worker_replace_conn(worker, conn);
        }
    }
}

static tglog_conn *tglog_worker_get_conn(tglog_worker *worker) {
    if (worker->conns->count <= 0) {
        tglog_fatal(worker->logger, "No available connections, worker: %d", worker->id);

        /* Check time interval to avoid frequent emergency supplement attempts */
        uint64_t now = tglog_get_time_ms();
        uint64_t elapsed = now - worker->last_emergency_supplement_time;
        const uint64_t MIN_EMERGENCY_SUPPLEMENT_INTERVAL_MS = 2000; /* 2 seconds */

        if (elapsed >= MIN_EMERGENCY_SUPPLEMENT_INTERVAL_MS) {
            /* All connections are closed, try to supplement new connections */
            tglog_info(worker->logger, "Emergency supplement connections, worker: %d, elapsed: %llu ms",
                       worker->id, (unsigned long long) elapsed);
            tglog_worker_supplement_conn(worker);
            worker->last_emergency_supplement_time = now;
        } else {
            tglog_debug(worker->logger, "Skip emergency supplement due to time interval, worker: %d, elapsed: %llu ms",
                        worker->id, (unsigned long long) elapsed);
        }

        return NULL;
    }

    /* Simple round-robin */
    uint64_t index = ck_pr_faa_64(&worker->conn_index, 1);
    return worker->conns->items[index % worker->conns->count];
}

static bool tglog_worker_send_batch(tglog_worker *worker, bool retry);

static bool tglog_worker_is_retrying(tglog_worker *worker) {
    return worker->tmp_batch->retry_start_time.tv_sec > 0 || worker->tmp_batch->retry_start_time.tv_usec > 0;
}

static void tglog_worker_on_retry_batch_tick(evutil_socket_t fd, short what, void *user_data) {
    (void) fd;
    (void) what;
    tglog_worker *worker = (tglog_worker *) user_data;
    tglog_debug(worker->logger, "Worker: %d", worker->id);

    if (!tglog_worker_is_retrying(worker)) {
        tglog_warn(worker->logger, "Retrying flag is false, should not be here, worker: %d", worker->id);
        return;
    }

    tglog_worker_send_batch(worker, true);
}

static void tglog_worker_start_retrying(tglog_worker *worker) {
    tglog_debug(worker->logger, "Worker: %d", worker->id);

    tglog_metrics_inc_retry_msg_count(worker->metrics, worker->id, 1);
    worker->tmp_batch->retries++;
    unsigned int backoff_time = tglog_backoff_next_interval(&tglog_batch_retry_backoff, worker->tmp_batch->retries, true);

    struct timeval tv;
    tv.tv_sec = backoff_time / 1000;
    tv.tv_usec = (backoff_time % 1000) * 1000;
    if (!worker->batch_retry_event) {
        /* Note: only once, no EV_PERSIST */
        worker->batch_retry_event = event_new(worker->base, -1, EV_TIMEOUT, tglog_worker_on_retry_batch_tick, worker);
    }
    event_add(worker->batch_retry_event, &tv);

    if (worker->tmp_batch->retries == 1) {
        gettimeofday(&worker->tmp_batch->retry_start_time, NULL);
        unsigned int deadline_time = tglog_backoff_get_total_backoff_time(&tglog_batch_retry_backoff, worker->client->conf.max_retries + 1);
        worker->tmp_batch->retry_deadline.tv_sec = worker->tmp_batch->retry_start_time.tv_sec + deadline_time / 1000;
        worker->tmp_batch->retry_deadline.tv_usec = worker->tmp_batch->retry_start_time.tv_usec + (deadline_time % 1000) * 1000;
    }
}

static void tglog_worker_stop_retrying(tglog_worker *worker) {
    tglog_debug(worker->logger, "Worker: %d", worker->id);
    worker->tmp_batch->retries = 0;
    worker->tmp_batch->retry_start_time.tv_sec = 0;
    worker->tmp_batch->retry_start_time.tv_usec = 0;

    worker->tmp_batch->retry_deadline.tv_sec = 0;
    worker->tmp_batch->retry_deadline.tv_usec = 0;

    if (worker->batch_retry_event) {
        event_del(worker->batch_retry_event);
    }
}

static int tglog_timeval_compare(struct timeval *t1, struct timeval *t2) {
    if (t1->tv_sec > t2->tv_sec) {
        return 1;   /* t1 is greater */
    } else if (t1->tv_sec < t2->tv_sec) {
        return -1;  /* t2 is greater */
    } else {
        /* tv_sec is equal, compare tv_usec */
        if (t1->tv_usec > t2->tv_usec) {
            return 1;   /* t1 is greater */
        } else if (t1->tv_usec < t2->tv_usec) {
            return -1;  /* t2 is greater */
        } else {
            return 0;   /* t1 and t2 are equal */
        }
    }
}

static bool tglog_worker_is_retrying_timeout(tglog_worker *worker) {
    if (!tglog_worker_is_retrying(worker)) {
        return false;
    }

    struct timeval now;
    gettimeofday(&now, NULL);
    return tglog_timeval_compare(&now, &worker->tmp_batch->retry_deadline) >= 0;
}

static void tglog_worker_done_batch(tglog_worker *worker, int err_code) {
    tglog_batch_done(worker->tmp_batch, err_code);
    if (tglog_worker_is_retrying(worker)) {
        tglog_worker_stop_retrying(worker);
    }
}

/**
 * Return true if the batch is done, false if the batch is still retrying
 */
static bool tglog_worker_retry_batch(tglog_worker *worker) {
    tglog_debug(worker->logger, "Worker: %d", worker->id);

    /* Examine if the batch has exceeded the maximum retry limit */
    if (worker->tmp_batch->retries >= worker->client->conf.max_retries) {
        tglog_warn(worker->logger, "Batch retry times exceed max retries, stop retrying, worker: %d", worker->id);
        tglog_worker_done_batch(worker, TGLOG_ERR_SEND_TIMEOUT);
        return true;
    }

    /* Check if the worker is ready */
    if (ck_pr_load_32(&worker->state) != TGLOG_WORKER_STATE_READY) {
        tglog_warn(worker->logger, "Worker is not ready, stop retrying, worker: %d", worker->id);
        tglog_worker_done_batch(worker, TGLOG_ERR_PRODUCER_CLOSED);
        return true;
    }

    /* Start retrying */
    tglog_worker_start_retrying(worker);
    return false;
}

static int tglog_worker_send_batch_udp(tglog_worker *worker) {
    tglog_debug(worker->logger, "Worker: %d", worker->id);

    tglog_conn *conn = tglog_worker_get_conn(worker);
    if (!conn) {
        tglog_fatal(worker->logger, "get conn failed, no conn available, worker: %d", worker->id);
        return TGLOG_ERR_NO_AVAILABLE_CONN;
    }

    struct iovec *batch_iov = NULL;
    int batch_iov_count = 0;
    size_t batch_buf_len = 0;

    /* Check if we already have assembled buffer (from retry) */
    if (worker->tmp_batch->iov != NULL && worker->tmp_batch->iov_count > 0) {
        /* Reuse the previously assembled buffer */
        batch_iov = worker->tmp_batch->iov;
        batch_iov_count = worker->tmp_batch->iov_count;
        batch_buf_len = worker->tmp_batch->buffer_len;
        tglog_debug(worker->logger, "Reusing assembled buffer for retry, worker: %d, size: %zu", worker->id, batch_buf_len);
    } else {
        /* Combine the messages into an iov */
        int iov_count = 0;
        int i = 0;
        for (i = 0; i < worker->tmp_batch->msg_count; i++) {
            if (batch_buf_len + worker->tmp_batch->msgs[i]->payload_len > TGLOG_MAX_UDP_REQ_SIZE_V1) {
                tglog_fatal(worker->logger, "batch data size exceeds the maximum size, should not be here, worker: %d, size: %d", worker->id,
                            batch_buf_len + worker->tmp_batch->msgs[i]->payload_len);
                return TGLOG_ERR_BATCH_SIZE_EXCEEDED;
            }

            if (iov_count >= worker->client->conf.batching_max_messages) {
                tglog_fatal(worker->logger, "batch message count exceeds the maximum batch size, should not be here, worker: %d, size: %d", worker->id,
                            iov_count);
                return TGLOG_ERR_BATCH_SIZE_EXCEEDED;
            }

            batch_buf_len += worker->tmp_batch->msgs[i]->payload_len;
            worker->tmp_iov[iov_count].iov_base = worker->tmp_batch->msgs[i]->payload;
            worker->tmp_iov[iov_count].iov_len = worker->tmp_batch->msgs[i]->payload_len;
            iov_count++;
        }

        batch_iov = worker->tmp_iov;
        batch_iov_count = iov_count;

        /* Save the buffer for potential retry */
        worker->tmp_batch->iov = batch_iov;
        worker->tmp_batch->iov_count = batch_iov_count;
        worker->tmp_batch->buffer_len = batch_buf_len;
    }

    size_t sent = 0;
    while (sent < batch_buf_len) {
        ssize_t bytes_sent = tglog_conn_sendv(conn, batch_iov, batch_iov_count);
        if (bytes_sent < 0) {
            int err = evutil_socket_geterror(conn->fd);
            if (err == EAGAIN || err == EWOULDBLOCK) {
                tglog_metrics_inc_eagain_err_count(worker->metrics, worker->id, 1);
                tglog_debug(worker->logger, "Send batch encounter EAGAIN or EWOULDBLOCK and will retry again, worker: %d, addr: %s, errno: %d", worker->id, conn->endpoint, err);
                if (worker->conns->count > 1) {
                    conn = tglog_worker_get_conn(worker);
                    if (!conn) {
                        tglog_fatal(worker->logger, "get conn failed, no conn available, worker: %d", worker->id);
                        return TGLOG_ERR_NO_AVAILABLE_CONN;
                    }
                    tglog_debug(worker->logger, "Send batch encounter EAGAIN or EWOULDBLOCK, now retry with new conn, worker: %d, addr: %s, errno: %d", worker->id, conn->endpoint, err);
                }
                continue;
            }

            tglog_error(worker->logger, "Send batch failed, we will retry later, worker: %d, addr: %s, sent: %d, errno: %d", worker->id, conn->endpoint, bytes_sent, err);

            /* Mark the endpoint as unavailable, remove the connection and supplement a new connection */
            tglog_worker_on_conn_error(worker, conn, BEV_EVENT_WRITING | BEV_EVENT_ERROR);

            if (worker->conns->count > 0) {
                conn = tglog_worker_get_conn(worker);
                if (!conn) {
                    tglog_fatal(worker->logger, "get conn failed, no conn available, worker: %d", worker->id);
                    return TGLOG_ERR_NO_AVAILABLE_CONN;
                }
                tglog_warn(worker->logger, "Send batch failed, now retry with new conn, worker: %d, addr: %s, errno: %d", worker->id, conn->endpoint, err);
                continue;
            }

            return TGLOG_ERR_SEND_FAILED;
        }
        sent += bytes_sent;
    }

    return TGLOG_ERR_OK;
}

static int tglog_worker_send_batch_tcp(tglog_worker *worker) {
    tglog_debug(worker->logger, "Worker: %d", worker->id);

    tglog_conn *conn = tglog_worker_get_conn(worker);
    if (!conn) {
        tglog_fatal(worker->logger, "get conn failed, no conn available, worker: %d", worker->id);
        return TGLOG_ERR_NO_AVAILABLE_CONN;
    }

    struct iovec *batch_iov = NULL;
    int batch_iov_count = 0;
    size_t batch_buf_len = 0;

    /* Check if we already have assembled buffer (from retry) */
    if (worker->tmp_batch->iov != NULL && worker->tmp_batch->iov_count > 0) {
        /* Reuse the previously assembled buffer */
        batch_iov = worker->tmp_batch->iov;
        batch_iov_count = worker->tmp_batch->iov_count;
        batch_buf_len = worker->tmp_batch->buffer_len;
        tglog_debug(worker->logger, "Reusing assembled buffer for retry, worker: %d, size: %zu", worker->id, batch_buf_len);
    } else {
        /* Combine the messages into an iov */
        int iov_count = 0;
        int i = 0;
        for (i = 0; i < worker->tmp_batch->msg_count; i++) {
            if (batch_buf_len + worker->tmp_batch->msgs[i]->payload_len > TGLOG_MAX_TCP_REQ_SIZE_V1) {
                tglog_fatal(worker->logger, "batch data size exceeds the maximum TCP size, should not be here, worker: %d, size: %d", worker->id,
                            batch_buf_len + worker->tmp_batch->msgs[i]->payload_len);
                return TGLOG_ERR_BATCH_SIZE_EXCEEDED;
            }
            if (iov_count >= worker->client->conf.batching_max_messages) {
                tglog_fatal(worker->logger, "batch message count exceeds the maximum batch size, should not be here, worker: %d, size: %d", worker->id,
                            iov_count);
                return TGLOG_ERR_BATCH_SIZE_EXCEEDED;
            }

            batch_buf_len += worker->tmp_batch->msgs[i]->payload_len;
            worker->tmp_iov[iov_count].iov_base = worker->tmp_batch->msgs[i]->payload;
            worker->tmp_iov[iov_count].iov_len = worker->tmp_batch->msgs[i]->payload_len;
            iov_count++;
        }

        batch_iov = worker->tmp_iov;
        batch_iov_count = iov_count;

        /* Save the buffer for potential retry */
        worker->tmp_batch->iov = batch_iov;
        worker->tmp_batch->iov_count = batch_iov_count;
        worker->tmp_batch->buffer_len = batch_buf_len;
    }

    size_t sent = 0;
    int try_count = 0;
    while (sent < batch_buf_len) {
        try_count++;
        ssize_t bytes_sent = tglog_conn_sendv_with_bufferevent(conn, batch_iov, batch_iov_count);
        // tglog_conn_sendv_with_bufferevent return 0 means exceeded the high watermark
        if (bytes_sent <= 0) {
            tglog_error(worker->logger, "Send batch failed, we will retry later, worker: %d, addr: %s, sent: %d, try_count: %d", worker->id, conn->endpoint, bytes_sent, try_count);

            /* Mark the endpoint as unavailable, remove the connection and supplement a new connection */
            if (bytes_sent < 0) {
                tglog_worker_on_conn_error(worker, conn, BEV_EVENT_WRITING | BEV_EVENT_ERROR);
            }

            if (worker->conns->count > 0 && try_count < worker->conns->count) {
                conn = tglog_worker_get_conn(worker);
                if (!conn) {
                    tglog_fatal(worker->logger, "get conn failed, no conn available, worker: %d", worker->id);
                    return TGLOG_ERR_NO_AVAILABLE_CONN;
                }
                tglog_warn(worker->logger, "Send batch failed, now retry with new conn, worker: %d, addr: %s, try_count: %d", worker->id, conn->endpoint, try_count);
                continue;
            }

            return TGLOG_ERR_SEND_FAILED;
        }
        sent += bytes_sent;
    }

    return TGLOG_ERR_OK;
}

/**
 * Return true if the batch is done, false if the batch is still retrying
 */
static bool tglog_worker_send_batch(tglog_worker *worker, bool retry) {
    tglog_debug(worker->logger, "Worker: %d", worker->id);

    int err_code = TGLOG_ERR_SEND_FAILED;

    if (worker->client->conf.network == TGLOG_NETWORK_UDP) {
        err_code = tglog_worker_send_batch_udp(worker);
    } else {
        err_code = tglog_worker_send_batch_tcp(worker);
    }

    /* Send batch successfully, done the batch */
    if (err_code == TGLOG_ERR_OK) {
        tglog_debug(worker->logger, "Send batch success, worker: %d", worker->id);
        tglog_worker_done_batch(worker, err_code);
        return true;
    }

    /* If retry is required, retry with a backoff timer */
    if (retry) {
        tglog_warn(worker->logger, "Send batch failed, now retry with a backoff timer, worker: %d", worker->id);
        return tglog_worker_retry_batch(worker);
    }

    /* Retry is not required, fail it directly */
    tglog_error(worker->logger, "Send batch failed and retry is not required, fail it directly, worker: %d", worker->id);
    tglog_worker_done_batch(worker, err_code);
    return true;
}

static bool tglog_worker_can_send_batch(tglog_worker *worker) {
    return tglog_batch_can_send(worker->tmp_batch,
                                worker->client->conf.batching_max_messages,
                                worker->client->conf.batching_max_size,
                                worker->client->conf.batching_max_publish_delay);
}

static bool tglog_worker_can_send_batch_ignore_timeout(tglog_worker *worker) {
    return tglog_batch_can_send_ignore_timeout(worker->tmp_batch,
                                               worker->client->conf.batching_max_messages,
                                               worker->client->conf.batching_max_size);
}

static bool tglog_worker_batch_send_timeout(tglog_worker *worker) {
    if (!worker->tmp_batch || worker->tmp_batch->msg_count <= 0 || worker->client->conf.send_timeout <= 0) {
        return false;
    }

    long elapsed_ms = tglog_time_since_ms(&worker->tmp_batch->time);
    if (elapsed_ms >= worker->client->conf.send_timeout) {
        return true;
    }

    return false;
}

static int tglog_worker_handle_msg(tglog_worker *worker, bool retry, int max_messages, uint64_t start_us) {
    tglog_debug(worker->logger, "Worker: %d", worker->id);

    /* Check if retrying is timeout every message notify, make sure retrying is not stuck */
    if (tglog_worker_is_retrying(worker)) {
        if (retry && !tglog_worker_is_retrying_timeout(worker) && !tglog_worker_batch_send_timeout(worker)) {
            tglog_debug(worker->logger, "Send batch is retrying, ignore the new message, worker: %d", worker->id);
            return 0;
        }

        /* Force to done batch with timeout error */
        tglog_warn(worker->logger, "Send batch retrying timeout, force to done batch with timeout error, worker: %d", worker->id);
        tglog_worker_done_batch(worker, TGLOG_ERR_SEND_TIMEOUT);
    }

    if (tglog_worker_batch_send_timeout(worker)) {
        tglog_warn(worker->logger, "Send batch timeout, force to done batch with timeout error, worker: %d", worker->id);
        tglog_worker_done_batch(worker, TGLOG_ERR_SEND_TIMEOUT);
    }

    /* If the current batch is timeout or exceeds the max size or max message count, send it first */
    bool done = false;
    if (tglog_worker_can_send_batch(worker)) {
        done = tglog_worker_send_batch(worker, retry);
        if (!done) {
            return 0;
        }

        /* The batch has been sent successfully, continue to handle the messages in the message queue */
    }

    if (ck_ring_size(&worker->message_queue) == 0 && worker->tmp_message == NULL) {
        return 0;
    }

    /* Use provided max_messages or default to TGLOG_MSG_PER_RUN */
    int messages_to_process = (max_messages > 0) ? max_messages : TGLOG_MSG_PER_RUN;

    int msg_count = 0;
    while (msg_count < messages_to_process) {
        tglog_message *msg = NULL;
        /* If there is a message not sent last time, handle it first */
        if (worker->tmp_message != NULL) {
            msg = worker->tmp_message;
            worker->tmp_message = NULL;
        } else if (!ck_ring_dequeue_mpsc_msg_queue(&worker->message_queue, worker->message_queue_buffer, &msg)) {
            return msg_count;
        } else {
            tglog_metrics_inc_dequeue_msg_count(worker->metrics, worker->id, 1);
        }

        /* Increase message count */
        msg_count++;

        /* Check if the total size of the message and the current batch exceeds the maximum allowed size */
        size_t total_len = tglog_message_len(msg) + tglog_batch_len(worker->tmp_batch);
        size_t max_req_size = worker->client->conf.network == TGLOG_NETWORK_UDP ? TGLOG_MAX_UDP_REQ_SIZE_V1 : TGLOG_MAX_TCP_REQ_SIZE_V1;
        if (total_len > max_req_size) {
            /* Save the message temporarily */
            worker->tmp_message = msg;
            /* Send the current batch first */
            done = tglog_worker_send_batch(worker, retry);
            if (!done) {
                return msg_count;
            }

            continue;
        }

        /* Add message to batch, if failed, save the message temporarily and send the current batch first */
        if (!tglog_batch_add(worker->tmp_batch, msg)) {
            tglog_error(worker->logger, "Add message to batch failed, worker: %d", worker->id);
            /* Save the message temporarily */
            worker->tmp_message = msg;
            /* Send the current batch first */
            done = tglog_worker_send_batch(worker, retry);
            if (!done) {
                return msg_count;
            }

            continue;
        }

        /* If the current batch exceeds the max size or max message count, send it immediately */
        if (tglog_worker_can_send_batch_ignore_timeout(worker)) {
            done = tglog_worker_send_batch(worker, retry);
            if (!done) {
                return msg_count;
            }
        }
    }

    if (msg_count > 0) {
        tglog_debug(worker->logger, "Worker: %d, msg count: %d", worker->id, msg_count);
    }

    return msg_count;
}

static void tglog_worker_on_msg_tick(evutil_socket_t fd, short what, void *user_data) {
    (void) fd;
    (void) what;
    tglog_worker *worker = (tglog_worker *) user_data;
    tglog_debug(worker->logger, "Worker: %d", worker->id);

    tglog_worker_handle_msg(worker, true, TGLOG_MSG_PER_RUN, 0);
}

static bool tglog_worker_add_endpoint(tglog_worker *worker, tglog_endpoint *ep) {
    tglog_info(worker->logger, "Add endpoint, worker: %d, addr: %s", worker->id, ep->addr);

    return tglog_endpoint_list_add(worker->endpoints, ep, true);
}

static void tglog_worker_remove_endpoint(tglog_worker *worker, tglog_endpoint *ep) {
    tglog_info(worker->logger, "Remove endpoint, worker: %d, addr: %s", worker->id, ep->addr);

    tglog_endpoint_list_remove(worker->endpoints, ep->addr);
}

static bool tglog_worker_add_conn_by_endpoint(tglog_worker *worker, tglog_endpoint *ep) {
    tglog_info(worker->logger, "Add conns for new endpoint, worker: %d, addr: %s", worker->id, ep->addr);

    /* Create multiple connections per endpoint for better load balancing */
    int i;
    int successful_conns = 0;

    for (i = 0; i < worker->conns_per_endpoint; i++) {
        tglog_conn *new_conn = tglog_worker_dial_new_conn(worker, ep->addr, CREATE_BY_ENDPOINT, NULL);
        if (!new_conn) {
            tglog_warn(worker->logger, "Failed to create connection %d/%d for endpoint %s",
                       i + 1, worker->conns_per_endpoint, ep->addr);
            continue;
        }

        if (tglog_worker_add_connecting_conn(worker, new_conn)) {
            successful_conns++;
        } else {
            tglog_error(worker->logger, "Add connecting conn failed, close the conn to avoid memory leak, worker: %d, addr: %s", worker->id, ep->addr);
            tglog_conn_close(new_conn);
        }
    }

    /* Mark endpoint unavailable only if all connection attempts failed */
    if (successful_conns == 0) {
        tglog_worker_mark_endpoint_unavailable(worker, ep->addr);
        return false;
    }

    return true;
}

static void tglog_worker_remove_conn_by_endpoint(tglog_worker *worker, tglog_endpoint *ep) {
    tglog_info(worker->logger, "Remove conn for deleted endpoint, worker: %d, addr: %s", worker->id, ep->addr);

    int i;
    for (i = worker->conns->count - 1; i >= 0; i--) {
        if (strcmp(worker->conns->items[i]->endpoint, ep->addr) == 0) {
            tglog_worker_close_conn_delay(worker, worker->conns->items[i], TGLOG_DEFAULT_CONN_CLOSE_DELAY);
        }
    }

    tglog_conn_list_remove_by_addr(worker->conns, ep->addr);
}

static void tglog_worker_on_endpoint_update(tglog_worker *worker) {
    tglog_debug(worker->logger, "Worker: %d", worker->id);
    tglog_endpoint_notify *epn;
    if (!ck_ring_dequeue_mpsc_epn_queue(&worker->ep_notify_queue, worker->ep_notify_queue_buffer, &epn)) {
        return;
    }

    int i;
    /* Add endpoints and connections */
    for (i = 0; i < epn->added->count; i++) {
        tglog_endpoint *ep = epn->added->items[i];
        if (!tglog_worker_add_endpoint(worker, ep)) {
            tglog_error(worker->logger, "Add endpoint failed, worker: %d, addr: %s", worker->id, ep->addr);
        }
        if (!tglog_worker_add_conn_by_endpoint(worker, ep)) {
            tglog_error(worker->logger, "Add conn for new endpoint failed, worker: %d, addr: %s", worker->id, ep->addr);
        }

        tglog_metrics_inc_add_endpoint_count(worker->metrics, worker->id, 1);
    }

    /* Remove endpoints and connections */
    for (i = 0; i < epn->removed->count; i++) {
        tglog_endpoint *ep = epn->removed->items[i];
        tglog_worker_remove_endpoint(worker, ep);
        tglog_worker_remove_conn_by_endpoint(worker, ep);
        tglog_worker_del_unavailable_endpoint(worker, ep->addr);
        tglog_worker_del_endpoint_retry_count(worker, ep->addr);

        tglog_metrics_inc_remove_endpoint_count(worker->metrics, worker->id, 1);
    }

    tglog_metrics_set_worker_endpoint_count(worker->metrics, worker->id, worker->endpoints->count);

    tglog_endpoint_notify_destroy(epn);
}

static void tglog_worker_on_close(tglog_worker *worker);

static void tglog_worker_on_notify(evutil_socket_t fd, short what, void *user_data) {
    (void) fd;
    tglog_worker *worker = (tglog_worker *) user_data;
    tglog_debug(worker->logger, "Worker: %d", worker->id);

    if (what & TGLOG_WORKER_NOTIFY_ENDPOINT) {
        tglog_worker_on_endpoint_update(worker);
    }

    if (what & TGLOG_WORKER_NOTIFY_CLOSE) {
        tglog_worker_on_close(worker);
    }
}

static unsigned int tglog_ceil_next_power_of_two(unsigned int n) {
    if (n <= 2) {
        return 2;
    }

    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    return n + 1;
}

static void tglog_worker_on_batch_timeout(evutil_socket_t fd, short what, void *user_data) {
    (void) fd;
    (void) what;
    tglog_worker *worker = (tglog_worker *) user_data;
    tglog_debug(worker->logger, "Worker: %d", worker->id);

    /* Check if retrying is timeout every batch timeout, make sure retrying is not stuck */
    if (tglog_worker_is_retrying(worker)) {
        if (!tglog_worker_is_retrying_timeout(worker)) {
            tglog_warn(worker->logger, "Send batch is retrying, ignore the batch timeout event, worker: %d", worker->id);
            return;
        }

        /* Force to done batch with timeout error */
        tglog_warn(worker->logger, "Send batch retrying timeout, force to done batch with timeout error, worker: %d", worker->id);
        tglog_worker_done_batch(worker, TGLOG_ERR_SEND_TIMEOUT);
    }

    if (tglog_worker_batch_send_timeout(worker)) {
        tglog_warn(worker->logger, "Send batch timeout, force to done batch with timeout error, worker: %d", worker->id);
        tglog_worker_done_batch(worker, TGLOG_ERR_SEND_TIMEOUT);
        return;
    }

    /* Send batch if can */
    if (tglog_worker_can_send_batch(worker)) {
        tglog_worker_send_batch(worker, true);
    }
}

extern tglog_endpoint_list *tglog_client_get_endpoints(tglog_client *client);

tglog_worker *tglog_worker_create(int id, tglog_client *client) {

    if (!client) {
        return NULL;
    }

    tglog_worker *worker = (tglog_worker *) calloc(1, sizeof(tglog_worker));
    if (!worker) {
        return NULL;
    }

    /* Init basic attributes */
    worker->id = id;
    worker->client = client;
    worker->logger = client->logger;
    worker->metrics = client->metrics;
    ck_pr_store_32(&worker->state, TGLOG_WORKER_STATE_INIT);
    worker->last_emergency_supplement_time = 0;  /* Initialize to 0, allow first supplement immediately */

    /* Init delay close conns, unavailable endpoints, endpoint retry counts hash table */
    ck_ht_init(&worker->delay_close_conns, CK_HT_MODE_DIRECT, NULL, &tglog_ck_allocator, 8, 20250606);
    ck_ht_init(&worker->unavailable_endpoints, CK_HT_MODE_BYTESTRING, NULL, &tglog_ck_allocator, 8, 20250606);
    ck_ht_init(&worker->endpoint_retry_counts, CK_HT_MODE_BYTESTRING, NULL, &tglog_ck_allocator, 8, 20250606);

    /* Init message queue */
    unsigned int message_queue_capacity = tglog_ceil_next_power_of_two(worker->client->conf.max_pending_messages);
    worker->message_queue_buffer = (tglog_message **) calloc(message_queue_capacity, sizeof(tglog_message *));
    if (!worker->message_queue_buffer) {
        goto error;
    }
    ck_ring_init(&worker->message_queue, message_queue_capacity);

    /* Init endpoint notify queue */
    worker->ep_notify_queue_buffer = (tglog_endpoint_notify **) calloc(8, sizeof(tglog_endpoint_notify *));
    if (!worker->ep_notify_queue_buffer) {
        goto error;
    }
    ck_ring_init(&worker->ep_notify_queue, 8);

    /* Get init endpoints */
    worker->endpoints = tglog_client_get_endpoints(client);
    if (!worker->endpoints || worker->endpoints->count <= 0) {
        tglog_error(worker->logger, "Get endpoints failed, Worker: %d ", id);
        goto error;
    }
    tglog_metrics_set_worker_endpoint_count(worker->metrics, worker->id, worker->endpoints->count);

    /* Init connecting conn list (multiple connections per endpoint) */
    worker->conns_per_endpoint = worker->endpoints->count <= 2 ? 2 : 1;
    tglog_info(worker->logger, "Using %d connections per endpoint, worker: %d, endpoints: %d", worker->conns_per_endpoint, worker->id, worker->endpoints->count);

    worker->connecting_conns = tglog_conn_list_create(worker->endpoints->count * worker->conns_per_endpoint);
    if (!worker->connecting_conns) {
        tglog_error(worker->logger, "Create connecting conn list failed, worker: %d", worker->id);
        goto error;
    }

    /* Init conn list (multiple connections per endpoint) */
    worker->conns = tglog_conn_list_create(worker->endpoints->count * worker->conns_per_endpoint);
    if (!worker->conns) {
        tglog_error(worker->logger, "Create conn list failed, worker: %d", worker->id);
        goto error;
    }

    worker->tmp_message = NULL;
    worker->tmp_batch = tglog_batch_create(worker->client->conf.batching_max_messages);
    worker->tmp_iov = (struct iovec *) malloc(worker->client->conf.batching_max_messages * sizeof(struct iovec));
    if (!worker->tmp_iov) {
        goto error;
    }

    /**
     * Now all the resources have been initialized, we start to run the worker thread
     */

    /* Init event base */
    worker->base = event_base_new();
    if (!worker->base) {
        tglog_error(worker->logger, "Create event base failed, worker: %d", worker->id);
        goto error;
    }

    /* Init recover endpoint timer */
    struct timeval interval = {30, 0}; /* Check every 30 seconds */
    worker->recover_endpoint_event = event_new(worker->base, -1, EV_TIMEOUT | EV_PERSIST, tglog_worker_recover_endpoints, worker);
    if (worker->recover_endpoint_event) {
        event_add(worker->recover_endpoint_event, &interval);
    }

    /* Create emergency supplement timer (will be started when worker is ready) */
    worker->emergency_supplement_event = event_new(worker->base, -1, EV_TIMEOUT | EV_PERSIST, tglog_worker_emergency_supplement, worker);
    if (!worker->emergency_supplement_event) {
        tglog_error(worker->logger, "Create emergency supplement event failed, worker: %d", worker->id);
    }

    /* Init replace expired connections timer if needed */
    if (worker->client->conf.max_conn_lifetime > 0) {
        int interval_sec = worker->client->conf.max_conn_lifetime / 10;
        interval_sec = interval_sec > 20 ? 20 : (interval_sec > 0 ? interval_sec : 10);// at most 20s
        interval.tv_sec = interval_sec;
        interval.tv_usec = 0;
        worker->replace_conns_event = event_new(worker->base, -1, EV_TIMEOUT | EV_PERSIST, tglog_worker_replace_expired_conns, worker);
        if (worker->replace_conns_event) {
            event_add(worker->replace_conns_event, &interval);
        }
    }

    /* Init batch timeout timer */
    interval.tv_sec = worker->client->conf.batching_max_publish_delay / 1000;
    interval.tv_usec = worker->client->conf.batching_max_publish_delay % 1000 * 1000;
    worker->batch_timeout_event = event_new(worker->base, -1, EV_TIMEOUT | EV_PERSIST, tglog_worker_on_batch_timeout, worker);
    if (worker->batch_timeout_event) {
        event_add(worker->batch_timeout_event, &interval);
    }

    /* batch_retry_event is created on demand when retrying */
    worker->batch_retry_event = NULL;

    /* Init message timer, handle messages in the queue every 2ms */
    interval.tv_sec = 0;
    interval.tv_usec = 2000;
    worker->msg_event = event_new(worker->base, -1, EV_TIMEOUT | EV_PERSIST, tglog_worker_on_msg_tick, worker);
    if (worker->msg_event) {
        event_add(worker->msg_event, &interval);
    }

    /* Init notify event */
    worker->notify_event = event_new(worker->base, -1, EV_PERSIST, tglog_worker_on_notify, worker);
    if (worker->notify_event) {
        event_add(worker->notify_event, NULL);
    }

    /* Initialize connections */
    worker->init_conn_count = 0;
    worker->ready_conn = 0;
    worker->failed_conn = 0;

    if (!tglog_worker_init_conns(worker)) {
        tglog_error(worker->logger, "Init conns failed, worker: %d", id);
        goto error;
    }

    /* Start the worker thread and wait for initial connections are ready */
#ifdef _WIN32
    InitializeCriticalSection(&worker->ready_lock);
    InitializeConditionVariable(&worker->ready_cond);
    worker->thread = CreateThread(NULL, 0, tglog_worker_thread, worker, 0, NULL);
    if (worker->thread == NULL) {
        tglog_error(worker->logger, "Create worker thread failed");
        goto error;
    }

    EnterCriticalSection(&worker->ready_lock);
    while (worker->ready_conn + worker->failed_conn < worker->init_conn_count) {
        SleepConditionVariableCS(&worker->ready_cond, &worker->ready_lock, INFINITE);
    }
    LeaveCriticalSection(&worker->ready_lock);
#else
    pthread_mutex_init(&worker->ready_lock, NULL);
    pthread_cond_init(&worker->ready_cond, NULL);
    int ret = pthread_create(&worker->thread, NULL, tglog_worker_thread, worker);
    if (ret != 0) {
        tglog_error(worker->logger, "Create worker thread failed");
        goto error;
    }

    pthread_mutex_lock(&worker->ready_lock);
    while (worker->ready_conn + worker->failed_conn < worker->init_conn_count) {
        pthread_cond_wait(&worker->ready_cond, &worker->ready_lock);
    }
    pthread_mutex_unlock(&worker->ready_lock);
#endif

    if (worker->ready_conn <= 0) {
        tglog_error(worker->logger, "Worker init conns failed, close it, id: %d", id);
        goto error;
    }

    /* Start emergency supplement timer now that worker is ready */
    if (worker->emergency_supplement_event) {
        interval.tv_sec = 5;
        interval.tv_usec = 0;
        event_add(worker->emergency_supplement_event, &interval);
        tglog_info(worker->logger, "Started emergency supplement timer, worker: %d", id);
    }

    tglog_info(worker->logger, "Worker is ready, id: %d", id);
    return worker;

    error:
    tglog_worker_destroy(worker);
    return NULL;
}

static void tglog_worker_drain_and_stop(int fd, short what, void *user_data) {
    (void) fd;
    (void) what;
    tglog_worker *worker = (tglog_worker *) user_data;
    tglog_info(worker->logger, "Draining worker, id: %d", worker->id);
    if (worker->drain_and_stop_time == 0) {
        worker->drain_and_stop_time = tglog_get_time_ms();
    }

    uint64_t stop_timeout_ms = worker->client->conf.stop_timeout > 0 ? worker->client->conf.stop_timeout : 3000;

    /** Flush all data in the connection buffers */
    if (worker->base) {
        /* Check if all data is sent */
        bool all_sent = true;

        /* Check active connections */
        if (worker->conns && worker->conns->count > 0) {
            int i;
            for (i = 0; i < worker->conns->count; i++) {
                tglog_conn *c = worker->conns->items[i];
                if (tglog_conn_get_out_buffer_len(c) > 0) {
                    all_sent = false;
                    tglog_conn_flush(c);
                }
            }
        }

        /* Check delay-close connections tracked in hash */
        ck_ht_iterator_t it = CK_HT_ITERATOR_INITIALIZER;
        ck_ht_entry_t *cur = NULL;
        while (ck_ht_next(&worker->delay_close_conns, &it, &cur)) {
            tglog_conn *c = (tglog_conn *) ck_ht_entry_key(cur);
            if (tglog_conn_get_out_buffer_len(c) > 0) {
                all_sent = false;
                tglog_conn_flush(c);
            }
        }

        if (all_sent) {
            tglog_info(worker->logger, "All connection buffers are drained before timeout, id: %d", worker->id);
            goto stop_worker;
        }

        /* Check timeout */
        if (tglog_get_time_ms() - worker->drain_and_stop_time > stop_timeout_ms) {
            tglog_warn(worker->logger, "Destroy worker timeout after %llu ms",
                       (unsigned long long) stop_timeout_ms);
            goto stop_worker;
        }

        /* Small delay */
        struct timeval tv = {0, 5000}; /* 5ms */
        event_base_once(worker->base, -1, EV_TIMEOUT, tglog_worker_drain_and_stop, worker, &tv);
        return;
    }

    stop_worker:
    /* Stop the event loop thread */
    if (worker->thread) {
        tglog_info(worker->logger, "Stop worker thread, id: %d", worker->id);
        event_base_loopbreak(worker->base);
    }

    /* Update the worker state to closed */
    ck_pr_cas_32(&worker->state, TGLOG_WORKER_STATE_CLOSING, TGLOG_WORKER_STATE_CLOSED);
    tglog_info(worker->logger, "Worker is closed, id: %d", worker->id);
}

static void tglog_worker_on_close(tglog_worker *worker) {
    tglog_debug(worker->logger, "Worker: %d", worker->id);
    if (!ck_pr_cas_32(&worker->state, TGLOG_WORKER_STATE_READY, TGLOG_WORKER_STATE_CLOSING)) {
        return;
    }

    /* Now, no new message can enqueue */
    tglog_info(worker->logger, "Worker is closing, id: %d", worker->id);
    /* Stop all events/timers */
    if (worker->notify_event) {
        tglog_info(worker->logger, "Stop notify event, id: %d", worker->id);
        event_del(worker->notify_event);
    }

    if (worker->msg_event) {
        tglog_info(worker->logger, "Stop msg tick, id: %d", worker->id);
        event_del(worker->msg_event);
    }

    if (worker->batch_retry_event) {
        tglog_info(worker->logger, "Stop batch retry tick, id: %d", worker->id);
        event_del(worker->batch_retry_event);
    }

    if (worker->batch_timeout_event) {
        tglog_info(worker->logger, "Stop batch timeout tick, id: %d", worker->id);
        event_del(worker->batch_timeout_event);
    }

    if (worker->replace_conns_event) {
        tglog_info(worker->logger, "Stop expired conns tick, id: %d", worker->id);
        event_del(worker->replace_conns_event);
    }

    if (worker->emergency_supplement_event) {
        tglog_info(worker->logger, "Stop emergency supplement tick, id: %d", worker->id);
        event_del(worker->emergency_supplement_event);
    }

    if (worker->recover_endpoint_event) {
        tglog_info(worker->logger, "Stop recover endpoint tick, id: %d", worker->id);
        event_del(worker->recover_endpoint_event);
    }

    /* Flush all the messages without retry */
    tglog_info(worker->logger, "Flush all the messages without retry, id: %d, left: %u", worker->id, ck_ring_size(&worker->message_queue));
    while (ck_ring_size(&worker->message_queue) > 0 || worker->tmp_message != NULL) {
        tglog_worker_handle_msg(worker, false, 100, 0);
    }

    /**
     * Maybe there are some messages still stay in the batch and are not flushed out by
     * batching_max_publish_delay/batching_max_messages/batching_max_size,
     * we need to send the batch immediately
     */
    if (worker->tmp_batch && worker->tmp_batch->msg_count > 0) {
        tglog_worker_send_batch(worker, false);
    }

    /* Drain the pending data in connections and stop the worker */
    tglog_worker_drain_and_stop(0, 0, worker);
}

static void tglog_worker_close(tglog_worker *worker) {
    /* Check if the worker is ready */
    if (ck_pr_load_32(&worker->state) != TGLOG_WORKER_STATE_READY) {
        tglog_warn(worker->logger, "Try to close worker but worker is not ready yet, just stop the worker thread, id: %d", worker->id);
        /* Stop the event loop thread */
        if (worker->thread) {
            event_base_loopbreak(worker->base);
        }

        ck_pr_store_32(&worker->state, TGLOG_WORKER_STATE_CLOSED);
        return;
    }

    /* Notify the worker thread to handle it */
    event_active(worker->notify_event, TGLOG_WORKER_NOTIFY_CLOSE, 0);
}

void tglog_worker_destroy(tglog_worker *worker) {
    if (!worker) {
        return;
    }

    /* Try to close the worker */
    tglog_worker_close(worker);

    /* Wait for the worker stopped */
    if (worker->thread) {
#ifdef _WIN32
        WaitForSingleObject(worker->thread, INFINITE);
        CloseHandle(worker->thread);
        DeleteCriticalSection(&worker->ready_lock);
#else
        pthread_join(worker->thread, NULL);
        pthread_mutex_destroy(&worker->ready_lock);
        pthread_cond_destroy(&worker->ready_cond);
        tglog_info(worker->logger, "Worker thread stopped, id: %d", worker->id);
#endif
    }

    /* Free events */
    if (worker->notify_event) {
        tglog_info(worker->logger, "Stop notify event, id: %d", worker->id);
        event_free(worker->notify_event);
    }

    if (worker->msg_event) {
        tglog_info(worker->logger, "Stop msg tick, id: %d", worker->id);
        event_free(worker->msg_event);
    }

    if (worker->batch_retry_event) {
        tglog_info(worker->logger, "Stop batch retry tick, id: %d", worker->id);
        event_free(worker->batch_retry_event);
    }

    if (worker->batch_timeout_event) {
        tglog_info(worker->logger, "Stop batch timeout tick, id: %d", worker->id);
        event_free(worker->batch_timeout_event);
    }

    if (worker->replace_conns_event) {
        tglog_info(worker->logger, "Stop expired conns tick, id: %d", worker->id);
        event_free(worker->replace_conns_event);
    }

    if (worker->emergency_supplement_event) {
        tglog_info(worker->logger, "Free emergency supplement event, id: %d", worker->id);
        event_free(worker->emergency_supplement_event);
    }

    if (worker->recover_endpoint_event) {
        tglog_info(worker->logger, "Stop recover endpoint tick, id: %d", worker->id);
        event_free(worker->recover_endpoint_event);
    }

    /* Destroy temp batch */
    if (worker->tmp_batch) {
        if (worker->tmp_batch->msg_count > 0) {
            tglog_worker_done_batch(worker, TGLOG_ERR_PRODUCER_CLOSED);
        }
        tglog_batch_destroy(worker->tmp_batch);
    }

    /* Destroy tmp_message if exists */
    if (worker->tmp_message) {
        tglog_message_done(worker->tmp_message, TGLOG_ERR_PRODUCER_CLOSED);
    }

    /* Free tmp_iov if exists */
    if (worker->tmp_iov) {
        free(worker->tmp_iov);
    }

    /* Consume the message queue and destroy the messages */
    if (worker->message_queue_buffer) {
        tglog_message *msg;
        while (ck_ring_dequeue_mpsc_msg_queue(&worker->message_queue, worker->message_queue_buffer, &msg)) {
            tglog_message_done(msg, TGLOG_ERR_PRODUCER_CLOSED);
            /* Note: message is freed by tglog_message_done if needed */
        }
        free(worker->message_queue_buffer);
    }

    /* Consume the endpoint notify queue and destroy the notifies */
    if (worker->ep_notify_queue_buffer) {
        tglog_endpoint_notify *epn;
        while (ck_ring_dequeue_mpsc_epn_queue(&worker->ep_notify_queue, worker->ep_notify_queue_buffer, &epn)) {
            tglog_endpoint_notify_destroy(epn);
        }
        free(worker->ep_notify_queue_buffer);
    }

    /* Destroy hash tables */
    ck_ht_iterator_t iterator = CK_HT_ITERATOR_INITIALIZER;
    ck_ht_entry_t *cursor = NULL;
    while (ck_ht_next(&worker->endpoint_retry_counts, &iterator, &cursor)) {
        void *key_ptr = ck_ht_entry_key(cursor);
        uint16_t key_len = ck_ht_entry_key_length(cursor);

        ck_ht_hash_t hash;
        ck_ht_hash(&hash, &worker->endpoint_retry_counts, key_ptr, key_len);

        ck_ht_entry_t entry;
        ck_ht_entry_key_set(&entry, key_ptr, key_len);
        if (ck_ht_remove_spmc(&worker->endpoint_retry_counts, hash, &entry)) {
            free(key_ptr);
        }
    }
    ck_ht_destroy(&worker->endpoint_retry_counts);

    ck_ht_iterator_init(&iterator);
    cursor = NULL;
    while (ck_ht_next(&worker->unavailable_endpoints, &iterator, &cursor)) {
        void *key_ptr = ck_ht_entry_key(cursor);
        uint16_t key_len = ck_ht_entry_key_length(cursor);

        ck_ht_hash_t hash;
        ck_ht_hash(&hash, &worker->unavailable_endpoints, key_ptr, key_len);

        ck_ht_entry_t entry;
        ck_ht_entry_key_set(&entry, key_ptr, key_len);
        if (ck_ht_remove_spmc(&worker->unavailable_endpoints, hash, &entry)) {
            free(key_ptr);
        }
    }
    ck_ht_destroy(&worker->unavailable_endpoints);

    ck_ht_iterator_init(&iterator);
    cursor = NULL;
    while (ck_ht_next(&worker->delay_close_conns, &iterator, &cursor)) {
        void *key_ptr = ck_ht_entry_key(cursor);
        uint16_t key_len = ck_ht_entry_key_length(cursor);

        ck_ht_hash_t hash;
        ck_ht_hash(&hash, &worker->delay_close_conns, key_ptr, key_len);

        ck_ht_entry_t entry;
        ck_ht_entry_key_set(&entry, key_ptr, key_len);
        if (ck_ht_remove_spmc(&worker->delay_close_conns, hash, &entry)) {
            tglog_conn *conn = (tglog_conn *) key_ptr;
            tglog_conn_close(conn);
        }
    }
    ck_ht_destroy(&worker->delay_close_conns);

    /* Close all the connections */
    tglog_conn_list_destroy(worker->connecting_conns);
    tglog_conn_list_destroy(worker->conns);

    tglog_endpoint_list_destroy(worker->endpoints);

    /* Free event base at last */
    if (worker->base) {
        event_base_free(worker->base);
    }

    tglog_info(worker->logger, "Worker is destroyed, id: %d", worker->id);
    free(worker);
}

int tglog_worker_send(tglog_worker *worker, const char *log_payload, tglog_send_callback callback, void *user_data, tglog_msg_flag msg_flag) {
    tglog_debug(worker->logger, "Worker: %d", worker->id);

    tglog_metrics_inc_request_msg_count(worker->metrics, worker->id, 1);

    size_t log_payload_len = strlen(log_payload);
    /* Check log_payload validity */
    if (log_payload_len <= 0) {
        tglog_error(worker->logger, "Invalid argument, log_payload is empty, worker: %d", worker->id);
        // no need to callback
        if (msg_flag == TGLOG_MSG_F_FREE) {
            free((void *) log_payload);
        }

        tglog_metrics_inc_dropped_msg_count(worker->metrics, worker->id, 1);
        tglog_metrics_inc_failed_msg_count(worker->metrics, worker->id, 1);
        return TGLOG_ERR_BAD_LOG;
    }

    /* Check log payload length */
    if (log_payload[log_payload_len - 1] != TGLOG_DELIMITER) {
        tglog_error(worker->logger, "Invalid argument, log_payload does not end with delimiter, worker: %d", worker->id);
        // no need to callback
        if (msg_flag == TGLOG_MSG_F_FREE) {
            free((void *) log_payload);
        }

        tglog_metrics_inc_dropped_msg_count(worker->metrics, worker->id, 1);
        tglog_metrics_inc_failed_msg_count(worker->metrics, worker->id, 1);
        return TGLOG_ERR_BAD_LOG;
    }

    bool too_long = false;

    /* Check length based on network type */
    if (worker->client->conf.network == TGLOG_NETWORK_UDP) {
        if (log_payload_len > TGLOG_MAX_UDP_REQ_SIZE_V1) {
            too_long = true;
        }
    } else {  /* TCP */
        if (log_payload_len > TGLOG_MAX_TCP_REQ_SIZE_V1) {
            too_long = true;
        }
    }

    if (too_long) {
        tglog_error(worker->logger, "Invalid argument, log_payload is too long, worker: %d, size: %zu", worker->id, log_payload_len);
        // no need to callback
        if (msg_flag == TGLOG_MSG_F_FREE) {
            free((void *) log_payload);
        }

        tglog_metrics_inc_dropped_msg_count(worker->metrics, worker->id, 1);
        tglog_metrics_inc_failed_msg_count(worker->metrics, worker->id, 1);
        return TGLOG_ERR_LOG_TOO_LONG;
    }

    /* Check worker state */
    if (ck_pr_load_32(&worker->state) != TGLOG_WORKER_STATE_READY) {
        tglog_warn(worker->logger, "Try to send msg but worker is not ready yet, worker: %d", worker->id);
        // no need to callback
        if (msg_flag == TGLOG_MSG_F_FREE) {
            free((void *) log_payload);
        }

        tglog_metrics_inc_failed_msg_count(worker->metrics, worker->id, 1);
        return TGLOG_ERR_PRODUCER_CLOSED;
    }

    /* Create message with metrics */
    tglog_message *msg = tglog_message_create(worker->client->mem_pool, log_payload, log_payload_len, callback, user_data, msg_flag, worker->metrics, worker->id);
    if (!msg) {
        tglog_fatal(worker->logger, "Create msg failed, worker: %d", worker->id);
        // no need to callback
        if (msg_flag == TGLOG_MSG_F_FREE) {
            free((void *) log_payload);
        }

        tglog_metrics_inc_failed_msg_count(worker->metrics, worker->id, 1);
        return TGLOG_ERR_MEMORY_ALLOC_FAILED;
    }

    /* Try to add to queue */
    if (!ck_ring_enqueue_mpsc_msg_queue(&worker->message_queue, worker->message_queue_buffer, &msg)) {
        tglog_fatal(worker->logger, "Enqueue msg failed, worker: %d", worker->id);
        // no need to callback
        tglog_message_destroy(msg);
        tglog_metrics_inc_failed_msg_count(worker->metrics, worker->id, 1);
        return TGLOG_ERR_SEND_QUEUE_IS_FULL;
    }

    tglog_metrics_inc_enqueue_msg_count(worker->metrics, worker->id, 1);
    tglog_debug(worker->logger, "Message queued, queue size: %d", (int) ck_ring_size(&worker->message_queue));

    return TGLOG_ERR_OK;
}

void tglog_worker_on_endpoint_updated(tglog_worker *worker,
                                      const tglog_endpoint_list *all,
                                      const tglog_endpoint_list *added,
                                      const tglog_endpoint_list *removed) {
    /* Check if the worker is ready */
    if (ck_pr_load_32(&worker->state) != TGLOG_WORKER_STATE_READY) {
        tglog_warn(worker->logger, "Try to update endpoint list but worker is not ready yet, worker: %d", worker->id);
        return;
    }

    /* Create an endpoint notify */
    tglog_endpoint_notify *epn = tglog_endpoint_notify_create(all, added, removed);
    if (!epn) {
        tglog_fatal(worker->logger, "Create endpoint notify failed, worker: %d", worker->id);
        return;
    }

    /* Enqueue data */
    if (!ck_ring_enqueue_mpsc_epn_queue(&worker->ep_notify_queue, worker->ep_notify_queue_buffer, &epn)) {
        tglog_fatal(worker->logger, "Enqueue endpoint notify failed, worker: %d", worker->id);
        tglog_endpoint_notify_destroy(epn);
        return;
    }

    /* Notify the worker to handle it */
    event_active(worker->notify_event, TGLOG_WORKER_NOTIFY_ENDPOINT, 0);
}

uint64_t tglog_worker_get_msg_queue_size(tglog_worker *worker) {
    if (!worker) {
        return 0;
    }
    return (uint64_t) ck_ring_size(&worker->message_queue);
}

uint64_t tglog_worker_get_msg_queue_capacity(tglog_worker *worker) {
    if (!worker) {
        return 0;
    }
    return (uint64_t) ck_ring_capacity(&worker->message_queue);
}

void tglog_worker_on_conn_closed(tglog_worker *worker) {
    if (!worker) {
        return;
    }

    /* Update worker-level metrics for multithreaded version */
    tglog_metrics_inc_closed_conn_count(worker->metrics, worker->id, 1);
}

void tglog_worker_on_conn_eagain(tglog_worker *worker) {
    if (!worker) {
        return;
    }

    /* Update client-level metrics for single-threaded version */
    tglog_metrics_inc_eagain_err_count(worker->metrics, worker->id, 1);
}
