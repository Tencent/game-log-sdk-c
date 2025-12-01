/* Standard library headers */

/* System headers */

/* Third-party library headers */
#include "event2/event.h"

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

static void tglog_client_on_endpoint_updated(const tglog_endpoint_list *all,
                                             const tglog_endpoint_list *added,
                                             const tglog_endpoint_list *removed,
                                             void *user_data) {
    tglog_client *client = (tglog_client *) user_data;
    if (!client) {
        return;
    }

    tglog_worker_on_endpoint_updated(client->worker, all, added, removed);
}

tglog_client *tglog_client_create(const tglog_conf *conf) {
    if (!conf || !conf->host[0] || conf->port <= 0 || conf->port > 65535) {
        return NULL;
    }

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

    tglog_info(client->logger, "Creating TGLog client (single-threaded version)");
    tglog_info(client->logger, "Server address: %s://%s:%d", conf->network == TGLOG_NETWORK_TCP ? "tcp" : "udp", conf->host, conf->port);

    /* Create mem pool */
    client->mem_pool = tglog_mem_pool_create(&client->conf);
    if (!client->mem_pool) {
        tglog_error(client->logger, "Failed to create mem pool");
        tglog_client_destroy(client);
        return NULL;
    }

    /* Create event base */
    struct event_config *ev_config = event_config_new();
    if (!ev_config) {
        tglog_error(client->logger, "Failed to create event config");
        tglog_client_destroy(client);
        return NULL;
    }

    event_config_set_flag(ev_config, EVENT_BASE_FLAG_NOLOCK | EVENT_BASE_FLAG_EPOLL_USE_CHANGELIST);
//    struct timeval max_interval = {0, 200};
//    event_config_set_max_dispatch_interval(ev_config, &max_interval, 20, 0);

    client->base = event_base_new_with_config(ev_config);
    event_config_free(ev_config);
    if (!client->base) {
        tglog_error(client->logger, "Failed to create event base");
        tglog_client_destroy(client);
        return NULL;
    }

    /* Create discoverer (includes initial DNS resolution) */
    client->discoverer = tglog_discoverer_create(conf->host, conf->port, conf->update_interval, client->logger, client->base, 3000);
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

    /* Create metrics */
    client->metrics = tglog_metrics_create();
    if (!client->metrics) {
        tglog_error(client->logger, "Failed to create metrics");
        tglog_client_destroy(client);
        return NULL;
    }

    /* Create worker (worker will manage connections itself) */
    client->worker = tglog_worker_create(0, client);  /* id is 0 for single-threaded */
    if (!client->worker) {
        tglog_error(client->logger, "Failed to create worker");
        tglog_client_destroy(client);
        return NULL;
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

    if (client->worker) {
        tglog_worker_destroy(client->worker);
    }

    if (client->metrics) {
        tglog_metrics_destroy(client->metrics);
    }

    if (client->logger) {
        tglog_info(client->logger, "TGLog client destroyed");
        tglog_debug_logger_destroy(client->logger);
    }

    if (client->base) {
        event_base_free(client->base);
    }

    if (client->mem_pool) {
        tglog_mem_pool_destroy(client->mem_pool);
    }

    free(client);
}

static tglog_worker *tglog_client_get_worker(tglog_client *client) {
    return client->worker;
}

int tglog_client_send(tglog_client *client, const char *log_payload, tglog_send_callback callback, void *user_data, char msg_flag) {
    if (!log_payload) {
        if (callback) {
            callback(log_payload, TGLOG_ERR_BAD_LOG, "Payload is NULL", user_data);
        }
        return TGLOG_ERR_BAD_LOG;
    }

    if (msg_flag < TGLOG_MSG_F_NONE || msg_flag > TGLOG_MSG_F_COPY) {
        if (callback){
            callback(log_payload, TGLOG_ERR_BAD_FLAG, "Message flag is invalid", user_data);
        }
        return TGLOG_ERR_BAD_FLAG;
    }

    if (!client) {
        if (callback) {
            callback(log_payload, TGLOG_ERR_UNKNOWN, "Client is NULL", user_data);
        }
        if (msg_flag == TGLOG_MSG_F_FREE) {
            free((void *) log_payload);
        }
        return TGLOG_ERR_UNKNOWN;
    }

    tglog_worker *worker = tglog_client_get_worker(client);
    if (!worker) {
        tglog_fatal(client->logger, "Get worker failed, should not be here");
        if (callback) {
            callback(log_payload, TGLOG_ERR_UNKNOWN, "Get worker failed", user_data);
        }
        if (msg_flag == TGLOG_MSG_F_FREE) {
            free((void *) log_payload);
        }
        return TGLOG_ERR_UNKNOWN;
    }

    int ret = tglog_worker_send(worker, log_payload, callback, user_data, msg_flag);
    if ((int) tglog_worker_get_msg_queue_size(worker) >= client->conf.batching_max_messages) {
        tglog_client_update(client);
    }

    return ret;
}

size_t tglog_client_get_msg_queue_size(tglog_client *client) {
    if (!client) {
        return 0;
    }
    return tglog_worker_get_msg_queue_size(client->worker);
}

size_t tglog_client_get_msg_queue_capacity(tglog_client *client) {
    if (!client) {
        return 0;
    }
    return tglog_worker_get_msg_queue_capacity(client->worker);
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

    total_msg_queue_size = tglog_worker_get_msg_queue_size(client->worker);
    total_msg_queue_capacity = tglog_worker_get_msg_queue_capacity(client->worker);
    tglog_metrics_set_msg_queue_size(client->metrics, total_msg_queue_size);
    tglog_metrics_set_msg_queue_capacity(client->metrics, total_msg_queue_capacity);
    tglog_metrics_set_limit_update_time(client->metrics, client->conf.max_update_us);

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

int tglog_client_update(tglog_client *client) {
    if (!client || !client->base) {
        return -1;
    }

    uint64_t update_start_time = tglog_get_time_us();

    /* Dispatch events (non-blocking) */
    tglog_debug(client->logger, "Dispatch events");
    event_base_loop(client->base, EVLOOP_NONBLOCK);
//#if 0
    uint64_t event_dispatch_end_time = tglog_get_time_us();
    uint64_t event_dispatch_time = event_dispatch_end_time - update_start_time;

    tglog_metrics_set_max_event_dispatch_time(client->metrics, event_dispatch_time);
    /* Check if event dispatch took too long */
    if (event_dispatch_time > 1000) {
        tglog_debug(client->logger, "Event dispatch took %llu us", (unsigned long long) event_dispatch_time);
    }
//#endif

    /* Dispatch messages */
    tglog_debug(client->logger, "Dispatch messages");
    int max_msg_per_update = client->conf.max_messages_per_update > client->conf.batching_max_messages ? client->conf.max_messages_per_update : client->conf.batching_max_messages;
    int msg_dispatched = tglog_worker_process(client->worker, max_msg_per_update, update_start_time);

    tglog_metrics_set_max_msg_dispatch_count(client->metrics, msg_dispatched);
    if (msg_dispatched == 0) {
        tglog_metrics_inc_idle_update_count(client->metrics, 1);
    }

//#if 0
    /* Update message dispatch time */
    uint64_t update_end_time = tglog_get_time_us();
    uint64_t msg_dispatch_time = update_end_time - event_dispatch_end_time;
    tglog_metrics_set_max_msg_dispatch_time(client->metrics, msg_dispatch_time);
    /* Check if message dispatch took too long */
    if (msg_dispatch_time > 1000) {
        tglog_debug(client->logger, "Dispatch messages took %llu us", (unsigned long long) msg_dispatch_time);
    }

    /* Update total update time */
    uint64_t total_update_time = update_end_time - update_start_time;
    tglog_metrics_set_max_update_time(client->metrics, total_update_time);
    /* Check if update timeout */
    if ((int64_t) total_update_time > client->conf.max_update_us) {
        tglog_debug(client->logger, "Update took %llu/%llu/%llu(event_dispatch_time/total_time/limit_time) us, msg_dispatched %d messages",
                    (unsigned long long) event_dispatch_time, (unsigned long long) total_update_time, client->conf.max_update_us, msg_dispatched);
        tglog_metrics_inc_timeout_update_count(client->metrics, 1);
    }
// #endif

    tglog_metrics_inc_total_update_count(client->metrics, 1);

    /* Update complete */
    return msg_dispatched;
}
