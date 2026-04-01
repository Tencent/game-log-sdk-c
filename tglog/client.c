/* Standard library headers */
#include <signal.h>

/* System headers */
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#endif

/* Third-party library headers */
#include "event2/thread.h"
#include "tglog_atomic.h"

/* Project headers */
#include "client.h"
#include "debug_logger.h"
#include "def.h"
#include "discoverer.h"
#include "err.h"
#include "pool.h"
#include "metrics.h"
#include "request.h"
#include "worker.h"

void tglog_init_network() {
#ifdef _WIN32
    static uint32_t init_state = 0; /* 0: uninitialized, 1: initializing, 2: initialized */

    if (tglog_atomic_load_32(&init_state) == 2) {
        return;
    }

    if (tglog_atomic_cas_32(&init_state, 0, 1)) {
        WSADATA wsaData;
        int ret = WSAStartup(MAKEWORD(2, 2), &wsaData);
        tglog_atomic_store_32(&init_state, ret == 0 ? 2 : 0);
        return;
    }

    while (tglog_atomic_load_32(&init_state) == 1) {
        tglog_pause();
    }
#endif
}

static void tglog_client_on_endpoint_updated(const tglog_endpoint_list *all,
                                             const tglog_endpoint_list *added,
                                             const tglog_endpoint_list *removed,
                                             void *user_data) {
    tglog_client *client = (tglog_client *) user_data;
    if (!client) {
        return;
    }

    int i = 0;
    for (i = 0; i < client->worker_count; i++) {
        tglog_worker *worker = client->workers[i];
        tglog_worker_on_endpoint_updated(worker, all, added, removed);
    }
}

tglog_client *tglog_client_create(const tglog_conf *conf) {
    if (!conf || !conf->host[0] || conf->port <= 0 || conf->port > 65535) {
        return NULL;
    }

    tglog_init_network();

    /* Ignore SIGPIPE to prevent process crash when writing to a closed socket.
     * writev() cannot use MSG_NOSIGNAL, so we must ignore SIGPIPE globally.
     * Only set SIG_IGN if the process has not installed a custom handler. */
#ifndef _WIN32
    struct sigaction sa;
    if (sigaction(SIGPIPE, NULL, &sa) == 0 && sa.sa_handler == SIG_DFL) {
        signal(SIGPIPE, SIG_IGN);
    }
#endif

    tglog_client *client = (tglog_client *) calloc(1, sizeof(tglog_client));
    if (!client) {
        return NULL;
    }

    /* Copy configuration */
    client->conf = *conf;
    if (client->conf.network == TGLOG_NETWORK_UDP && client->conf.batching_max_size > TGLOG_MAX_UDP_REQ_SIZE_V1) {
        client->conf.batching_max_size = TGLOG_MAX_UDP_REQ_SIZE_V1;
    }

    if (client->conf.network == TGLOG_NETWORK_TCP && client->conf.batching_max_size > TGLOG_MAX_TCP_REQ_SIZE_V1) {
        client->conf.batching_max_size = TGLOG_MAX_TCP_REQ_SIZE_V1;
    }

    client->logger = tglog_debug_logger_create(conf->can_log, conf->write_log, conf->user_data);

    tglog_info(client->logger, "Creating TGLog client (multithreaded version)");
    tglog_info(client->logger, "Server address: %s://%s:%d", conf->network == TGLOG_NETWORK_TCP ? "tcp" : "udp", conf->host, conf->port);

    /* Create mem pool */
    client->mem_pool = tglog_mem_pool_create(&client->conf);
    if (!client->mem_pool) {
        tglog_error(client->logger, "Failed to create mem pool");
        tglog_client_destroy(client);
        return NULL;
    }
    /* Create discoverer (includes initial DNS resolution) */
    client->discoverer = tglog_discoverer_create(conf->host, conf->port, conf->update_interval, client->logger);
    if (!client->discoverer) {
        tglog_error(client->logger, "Failed to create discoverer");
        tglog_client_destroy(client);
        return NULL;
    }

    /* Check if endpoints are available (DNS resolution already done in create) */
    if (tglog_discoverer_get_endpoint_count(client->discoverer) == 0) {
        tglog_error(client->logger, "No endpoints available");
        tglog_client_destroy(client);
        return NULL;
    }

    tglog_info(client->logger, "Start to create workers");

    /*init workers*/
    int worker_capacity = 8;
    if (conf->worker_num > 0) {
        worker_capacity = conf->worker_num;
    }

    /* Create metrics */
    client->metrics = tglog_metrics_create(worker_capacity);
    if (!client->metrics) {
        tglog_error(client->logger, "Failed to create metrics");
        tglog_client_destroy(client);
        return NULL;
    }

    /* Create worker (worker will manage connections itself) */
    client->worker_index = 0;
    client->workers = (tglog_worker **) malloc(sizeof(tglog_worker *) * worker_capacity);
    if (!client->workers) {
        tglog_error(client->logger, "Create worker array failed");
        tglog_client_destroy(client);
        return NULL;
    }

    int i = 0;
    for (i = 0; i < worker_capacity; i++) {
        tglog_worker *worker = tglog_worker_create(i, client);
        if (!worker) {
            tglog_error(client->logger, "Failed to create worker, id: %d", i);
            tglog_client_destroy(client);
            return NULL;
        }
        client->workers[client->worker_count++] = worker;
    }

    /* Register callback with discoverer after worker is created */
    tglog_discoverer_add_handler(client->discoverer, tglog_client_on_endpoint_updated, client);

    tglog_info(client->logger, "TGLog client is ready");
    return client;
}

void tglog_client_destroy(tglog_client *client) {
    if (!client) {
        return;
    }

    tglog_info(client->logger, "Destroying TGLog client");

    if (client->discoverer) {
        tglog_discoverer_destroy(client->discoverer);
    }

    if (client->workers) {
        int i;
        for (i = 0; i < client->worker_count; i++) {
            tglog_worker_destroy(client->workers[i]);
        }
        free(client->workers);
    }

    if (client->metrics) {
        tglog_metrics_destroy(client->metrics);
    }

    if (client->logger) {
        tglog_info(client->logger, "TGLog client destroyed");
        tglog_debug_logger_destroy(client->logger);
    }

    if (client->mem_pool) {
        tglog_mem_pool_destroy(client->mem_pool);
    }

    free(client);
}

static tglog_worker *tglog_client_get_worker(tglog_client *client) {
    uint64_t index = tglog_atomic_faa_64(&client->worker_index, 1);
    return client->workers[index % client->worker_count];
}

int tglog_client_send(tglog_client *client, const char *log_payload, tglog_send_callback callback, void *user_data, tglog_msg_flag msg_flag) {
    if (!log_payload) {
        // no need to callback
        return TGLOG_ERR_BAD_LOG;
    }

    if (msg_flag < TGLOG_MSG_F_NONE || msg_flag > TGLOG_MSG_F_COPY) {
        // no need to callback
        return TGLOG_ERR_BAD_FLAG;
    }

    if (!client) {
        // no need to callback
        if (msg_flag == TGLOG_MSG_F_FREE) {
            free((void *) log_payload);
        }
        return TGLOG_ERR_UNKNOWN;
    }

    tglog_worker *worker = tglog_client_get_worker(client);
    if (!worker) {
        tglog_fatal(client->logger, "Get worker failed, should not be here");
        // no need to callback
        if (msg_flag == TGLOG_MSG_F_FREE) {
            free((void *) log_payload);
        }
        return TGLOG_ERR_UNKNOWN;
    }

    return tglog_worker_send(worker, log_payload, callback, user_data, msg_flag);
}

size_t tglog_client_get_msg_queue_size(tglog_client *client) {
    if (!client) {
        return 0;
    }

    size_t size = 0;
    for (int i = 0; i < client->worker_count; ++i) {
        size += tglog_worker_get_msg_queue_size(client->workers[i]);
    }

    return size;
}

size_t tglog_client_get_msg_queue_capacity(tglog_client *client) {
    if (!client) {
        return 0;
    }

    size_t capacity = 0;
    for (int i = 0; i < client->worker_count; ++i) {
        capacity += tglog_worker_get_msg_queue_capacity(client->workers[i]);
    }

    return capacity;
}

tglog_endpoint_list *tglog_client_get_endpoints(tglog_client *client) {
    if (!client || !client->discoverer) {
        return NULL;
    }
    return tglog_discoverer_get_endpoints(client->discoverer);
}

int tglog_client_print_metrics(tglog_client *client, char *buffer, size_t size, bool json) {
    if (!client || !buffer || size == 0) {
        return -1;
    }

    /* Update metrics before returning */
    uint64_t endpoint_count = tglog_discoverer_get_endpoint_count(client->discoverer);
    tglog_metrics_set_endpoint_count(client->metrics, endpoint_count);

    uint64_t total_msg_queue_size = 0;
    uint64_t total_msg_queue_capacity = 0;
    int i = 0;
    for (i = 0; i < client->worker_count; i++) {
        tglog_worker *worker = client->workers[i];
        uint64_t worker_msg_queue_size = tglog_worker_get_msg_queue_size(worker);
        uint64_t worker_msg_queue_capacity = tglog_worker_get_msg_queue_capacity(worker);

        tglog_metrics_set_worker_msg_queue_size(client->metrics, i, worker_msg_queue_size);
        tglog_metrics_set_worker_msg_queue_capacity(client->metrics, i, worker_msg_queue_capacity);
        total_msg_queue_size += worker_msg_queue_size;
        total_msg_queue_capacity += worker_msg_queue_capacity;
    }

    tglog_metrics_set_msg_queue_size(client->metrics, total_msg_queue_size);
    tglog_metrics_set_msg_queue_capacity(client->metrics, total_msg_queue_capacity);

    if (!json) {
        return tglog_metrics_print(client->metrics, buffer, size);
    }

    return tglog_metrics_print_json(client->metrics, buffer, size);
}

int tglog_client_print_mem_pool_stats(tglog_client *client, char *buffer, size_t size, bool json) {
    if (!client || !buffer || size == 0) {
        return -1;
    }

    tglog_mem_pool_stats stats;
    tglog_mem_pool_get_stats(client->mem_pool, &stats);

    if (!json) {
        return tglog_mem_pool_stats_print(&stats, buffer, size);
    }
    return tglog_mem_pool_stats_print_json(&stats, buffer, size);
}

void tglog_use_thread() {
#ifdef _WIN32
    evthread_use_windows_threads();
#else
    evthread_use_pthreads();
#endif
}