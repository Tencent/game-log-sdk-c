/* Standard library headers */
#include <string.h>

/* System headers */

/* Third-party library headers */

/* Project headers */
#include "conf.h"

tglog_conf *tglog_conf_create(tglog_network network, const char *host, int port, can_log_func can_log, write_log_func write_log, void *user_data) {
    if (network != TGLOG_NETWORK_TCP && network != TGLOG_NETWORK_UDP) {
        return NULL;
    }

    if (!host || port <= 0 || port > 65535) {
        return NULL;
    }

    tglog_conf *conf = (tglog_conf *) calloc(1, sizeof(tglog_conf));
    if (!conf) {
        return NULL;
    }

    tglog_conf_set_default(conf);

    conf->network = network;
    strncpy(conf->host, host, sizeof(conf->host) - 1);
    conf->host[sizeof(conf->host) - 1] = '\0';
    conf->port = port;

    if (can_log) {
        conf->can_log = can_log;
    }

    if (write_log) {
        conf->write_log = write_log;
    }

    conf->user_data = user_data;

    return conf;
}

void tglog_conf_set_default(tglog_conf *conf) {
    if (!conf) {
        return;
    }

    conf->network = TGLOG_NETWORK_TCP;
    conf->port = 0;
    conf->update_interval = 60;
    conf->conn_timeout = 3000;
    conf->write_high_watermark = 32 * 1024 * 1024;
    conf->socket_send_buffer_size = 64 * 1024 * 1024;
    conf->socket_recv_buffer_size = 16 * 1024;
    conf->max_conn_lifetime = 300;

    /* batching and sending defaults */
    conf->batching_max_publish_delay = 10;
    conf->batching_max_messages = 50;
    conf->batching_max_size = 32 * 1024;
    conf->max_pending_messages = 1000000;
    conf->send_timeout = 30000;
    conf->max_retries = 10;
    conf->stop_timeout = 3000;

    /* debug logger defaults */
    conf->can_log = tglog_can_log_info;
    conf->write_log = tglog_std_write_log;
    conf->user_data = NULL;

    conf->message_pool_cap = 100000;
    conf->payload_pool_256b_cap = 2000;
    conf->payload_pool_512b_cap = 2000;
    conf->payload_pool_1kb_cap = 2000;
    conf->payload_pool_2kb_cap = 1000;
    conf->payload_pool_4kb_cap = 1000;
    conf->payload_pool_8kb_cap = 1000;
    conf->payload_pool_16kb_cap = 500;
    conf->payload_pool_32kb_cap = 500;
    conf->payload_pool_48kb_cap = 100;
    conf->payload_pool_64kb_cap = 100;

    /* single-threaded specific defaults */
    conf->max_update_us = 200;
    conf->max_messages_per_update = 1000;
}

bool tglog_conf_set_server(tglog_conf *conf, tglog_network network, const char *host, int port) {
    if (!conf) {
        return false;
    }

    if (network != TGLOG_NETWORK_TCP && network != TGLOG_NETWORK_UDP) {
        return false;
    }

    if (!host || port <= 0 || port > 65535) {
        return false;
    }

    conf->network = network;
    strncpy(conf->host, host, sizeof(conf->host) - 1);
    conf->host[sizeof(conf->host) - 1] = '\0';
    conf->port = port;

    return true;
}

void tglog_conf_set_debug_logger(tglog_conf *conf, can_log_func can_log, write_log_func write_log, void *user_data) {
    if (!conf) {
        return;
    }

    if (can_log) {
        conf->can_log = can_log;
    }

    if (write_log) {
        conf->write_log = write_log;
    }

    conf->user_data = user_data;
}

void tglog_conf_destroy(tglog_conf *conf) {
    if (!conf) {
        return;
    }

    free(conf);
}