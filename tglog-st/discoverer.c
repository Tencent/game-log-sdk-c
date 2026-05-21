/* Standard library headers */
#include <stdlib.h>
#include <string.h>

/* System headers */
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#endif

/* Platform compatibility */
#include "tglog_platform.h"

/* Third-party library headers */
#include "event2/event.h"
#include "event2/dns.h"

/* Project headers */
#include "discoverer.h"
#include "request.h"

typedef struct {
    endpoint_update_func callback;
    void *user_data;
} endpoint_event_handler;

struct tglog_discoverer {
    char *domain;                           /* Domain/host to resolve */
    int port;                               /* Port number */
    int interval_sec;                       /* Update interval in seconds */
    tglog_endpoint_list *endpoints;         /* Current endpoints */
    endpoint_event_handler *handlers;       /* Update callbacks array */
    size_t handler_count;                   /* Number of handlers */
    tglog_debug_logger *logger;             /* Debug logger */
    struct event_base *base;                /* Event base */
    struct evdns_base *dns_base;            /* DNS base */
    struct event *timer_event;              /* DNS update timer */
    bool ready;                             /* Is initial resolution complete */
};

static void tglog_discoverer_lookup(tglog_discoverer *discoverer);

static void tglog_discoverer_update_endpoints(tglog_discoverer *discoverer,
                                              const char **hosts,
                                              size_t host_count);

static void tglog_discoverer_notify_handlers(tglog_discoverer *discoverer,
                                             const tglog_endpoint_list *all,
                                             const tglog_endpoint_list *added,
                                             const tglog_endpoint_list *removed);

static void tglog_discoverer_on_timer(evutil_socket_t fd, short event, void *arg) {
    tglog_discoverer *discoverer = (tglog_discoverer *) arg;
    tglog_debug(discoverer->logger, "Discoverer refresh");
    (void) (fd);
    (void) (event);
    tglog_discoverer_lookup(discoverer);
}

tglog_discoverer *tglog_discoverer_create(const char *domain, int port, int interval_sec, tglog_debug_logger *logger, struct event_base *base, int timeout_ms) {
    if (!base || !domain || port <= 0 || port > 65535) {
        return NULL;
    }

    tglog_discoverer *discoverer = (tglog_discoverer *) calloc(1, sizeof(tglog_discoverer));
    if (!discoverer) {
        return NULL;
    }

    discoverer->domain = strdup(domain);
    if (!discoverer->domain) {
        tglog_discoverer_destroy(discoverer);
        return NULL;
    }
    discoverer->port = port;
    discoverer->logger = logger;
    discoverer->endpoints = tglog_endpoint_list_create(NULL, 0, 0);
    discoverer->interval_sec = 60;
    discoverer->ready = false;

    if (interval_sec > 0) {
        discoverer->interval_sec = interval_sec;
    }
    discoverer->base = base;
    if (!discoverer->base) {
        tglog_discoverer_destroy(discoverer);
        return NULL;
    }

    discoverer->dns_base = evdns_base_new(discoverer->base, EVDNS_BASE_INITIALIZE_NAMESERVERS);
    if (!discoverer->dns_base) {
        tglog_discoverer_destroy(discoverer);
        return NULL;
    }

    discoverer->timer_event = event_new(discoverer->base, -1, EV_TIMEOUT | EV_PERSIST, tglog_discoverer_on_timer, discoverer);
    if (!discoverer->timer_event) {
        tglog_discoverer_destroy(discoverer);
        return NULL;
    }

    tglog_discoverer_lookup(discoverer);

    uint64_t start_time = tglog_get_time_ms();
    uint64_t timeout = timeout_ms > 0 ? timeout_ms : 5000; /* Default 5 seconds */
    struct timeval tv;

    while (1) {
        /* Process events */
        event_base_loop(base, EVLOOP_ONCE | EVLOOP_NONBLOCK);

        /* Check if ready after processing events */
        if (discoverer->ready) {
            break;
        }

        /* Check timeout only if not ready */
        if (tglog_get_time_ms() - start_time > timeout) {
            tglog_error(logger, "Initial DNS lookup timeout after %llu ms, domain: %s",
                        (unsigned long long) timeout, domain);
            tglog_discoverer_destroy(discoverer);
            return NULL;
        }

        usleep(10000); /* 10ms */
    }

    if (!discoverer->endpoints || discoverer->endpoints->count == 0) {
        tglog_error(logger, "No endpoints resolved, domain: %s", domain);
        tglog_discoverer_destroy(discoverer);
        return NULL;
    }

    tv.tv_sec = discoverer->interval_sec;
    tv.tv_usec = 0;
    if (event_add(discoverer->timer_event, &tv) != 0) {
        tglog_error(logger, "Failed to start discoverer update timer");
        tglog_discoverer_destroy(discoverer);
        return NULL;
    }

    tglog_info(discoverer->logger, "Discoverer is ready");
    return discoverer;
}

void tglog_discoverer_destroy(tglog_discoverer *discoverer) {
    if (!discoverer) {
        return;
    }

    if (discoverer->timer_event) {
        event_del(discoverer->timer_event);
        event_free(discoverer->timer_event);
    }

    if (discoverer->dns_base) {
        evdns_base_free(discoverer->dns_base, 0);
    }

    if (discoverer->base) {
        discoverer->base = NULL;
    }

    if (discoverer->endpoints) {
        tglog_endpoint_list_destroy(discoverer->endpoints);
    }

    if (discoverer->handlers) {
        free(discoverer->handlers);
    }

    if (discoverer->domain) {
        free(discoverer->domain);
    }

    free(discoverer);
}

tglog_endpoint_list *tglog_discoverer_get_endpoints(tglog_discoverer *discoverer) {
    if (!discoverer) {
        return NULL;
    }

    tglog_endpoint_list *result = tglog_endpoint_list_copy(discoverer->endpoints);
    return result;
}

int tglog_discoverer_get_endpoint_count(tglog_discoverer *discoverer) {
    if (!discoverer || !discoverer->endpoints) {
        return 0;
    }

    return discoverer->endpoints->count;
}

bool tglog_discoverer_add_handler(tglog_discoverer *discoverer,
                                  endpoint_update_func callback,
                                  void *user_data) {
    if (!discoverer || !callback) {
        return false;
    }

    size_t i = 0;
    for (i = 0; i < discoverer->handler_count; i++) {
        if (discoverer->handlers[i].callback == callback) {
            return true;  /* Already exists */
        }
    }

    endpoint_event_handler *new_handlers = (endpoint_event_handler *) realloc(discoverer->handlers, (discoverer->handler_count + 1) * sizeof(endpoint_event_handler));
    if (!new_handlers) {
        return false;
    }

    discoverer->handlers = new_handlers;
    discoverer->handlers[discoverer->handler_count].callback = callback;
    discoverer->handlers[discoverer->handler_count].user_data = user_data;
    discoverer->handler_count++;

    return true;
}

bool tglog_discoverer_remove_handler(tglog_discoverer *discoverer,
                                     endpoint_update_func callback) {
    if (!discoverer || !callback) {
        return false;
    }

    size_t i = 0;
    for (i = 0; i < discoverer->handler_count; i++) {
        if (discoverer->handlers[i].callback == callback) {
            if (i < discoverer->handler_count - 1) {
                discoverer->handlers[i] = discoverer->handlers[discoverer->handler_count - 1];
            }

            discoverer->handler_count--;
            if (discoverer->handler_count > 0) {
                endpoint_event_handler *new_handlers = (endpoint_event_handler *) realloc(discoverer->handlers, discoverer->handler_count * sizeof(endpoint_event_handler));
                if (new_handlers) {
                    discoverer->handlers = new_handlers;
                }
            } else {
                free(discoverer->handlers);
                discoverer->handlers = NULL;
            }
            return true;
        }
    }

    return false;
}

static void tglog_discoverer_set_ready_once(tglog_discoverer *discoverer) {
    if (!discoverer->ready) {
        discoverer->ready = true;
        tglog_debug(discoverer->logger, "Discoverer ready after first DNS callback");
    }
}

static void dns_callback(int result, struct evutil_addrinfo *addr, void *user_data) {
    tglog_discoverer *discoverer = (tglog_discoverer *) user_data;

    if (result) {
        tglog_error(discoverer->logger, "DNS resolution failed: %s\n", evutil_gai_strerror(result));
        tglog_discoverer_set_ready_once(discoverer);
        return;
    }

    int host_capacity = 8;
    const char **hosts = calloc(host_capacity, sizeof(char *));
    if (!hosts) {
        tglog_fatal(discoverer->logger, "Failed to allocate memory for hosts");
        evutil_freeaddrinfo(addr);
        tglog_discoverer_set_ready_once(discoverer);
        return;
    }

    int host_count = 0;
    int i = 0;

    struct evutil_addrinfo *p;
    for (p = addr; p != NULL; p = p->ai_next) {
        char host[INET6_ADDRSTRLEN];

        if (p->ai_family == AF_INET) {
            struct sockaddr_in *ipv4 = (struct sockaddr_in *) p->ai_addr;
            inet_ntop(p->ai_family, &(ipv4->sin_addr), host, sizeof host);
        } else {
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *) p->ai_addr;
            inet_ntop(p->ai_family, &(ipv6->sin6_addr), host, sizeof host);
        }

        bool exists = false;
        for (i = 0; i < host_count; i++) {
            if (strcmp(hosts[i], host) == 0) {
                exists = true;
                break;
            }
        }

        if (!exists) {
            if (host_count == host_capacity) {
                host_capacity *= 2;
                const char **new_host = (const char **) realloc(hosts, host_capacity * sizeof(char *));
                if (!new_host) {
                    tglog_fatal(discoverer->logger, "Failed to allocate memory for hosts");
                    goto free_all;
                }

                hosts = new_host;
            }

            hosts[host_count] = strdup(host);
            host_count++;
        }
    }

    if (host_count > 0) {
        tglog_discoverer_update_endpoints(discoverer, hosts, host_count);
    }

    free_all:
    for (i = 0; i < host_count; i++) {
        free((void *) hosts[i]);
    }
    free(hosts);

    evutil_freeaddrinfo(addr);
    tglog_discoverer_set_ready_once(discoverer);
}

static void tglog_discoverer_lookup(tglog_discoverer *discoverer) {
    struct evdns_base *dns_base = discoverer->dns_base;
    evdns_base_clear_host_addresses(dns_base);
#ifdef _WIN32
    evdns_base_load_hosts(dns_base, "C:\\Windows\\System32\\drivers\\etc\\hosts");
#else
    evdns_base_load_hosts(dns_base, "/etc/hosts");
#endif

    struct evutil_addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    evdns_getaddrinfo(dns_base, discoverer->domain, NULL, &hints, dns_callback, discoverer);
}

static void tglog_discoverer_update_endpoints(tglog_discoverer *discoverer,
                                              const char **hosts,
                                              size_t host_count) {
    tglog_endpoint_list *new_endpoints = tglog_endpoint_list_create(hosts, (int) host_count, discoverer->port);
    if (!new_endpoints) {
        tglog_fatal(discoverer->logger, "Failed to allocate memory for endpoints");
        return;
    }

    tglog_endpoint_list *added = tglog_endpoint_list_create(NULL, 0, 0);
    tglog_endpoint_list *removed = tglog_endpoint_list_create(NULL, 0, 0);

    int i = 0;
    for (i = 0; i < new_endpoints->count; i++) {
        bool found = false;
        int j = 0;
        for (j = 0; j < discoverer->endpoints->count; j++) {
            if (strcmp(new_endpoints->items[i]->host, discoverer->endpoints->items[j]->host) == 0) {
                found = true;
                break;
            }
        }

        if (!found) {
            tglog_endpoint_list_add(added, new_endpoints->items[i], true);
        }
    }

    for (i = 0; i < discoverer->endpoints->count; i++) {
        bool found = false;
        int j = 0;
        for (j = 0; j < new_endpoints->count; j++) {
            if (strcmp(discoverer->endpoints->items[i]->host, new_endpoints->items[j]->host) == 0) {
                found = true;
                break;
            }
        }

        if (!found) {
            tglog_endpoint_list_add(removed, discoverer->endpoints->items[i], true);
        }
    }

    tglog_endpoint_list_destroy(discoverer->endpoints);
    discoverer->endpoints = new_endpoints;

    if (added->count > 0 || removed->count > 0) {
        tglog_discoverer_notify_handlers(discoverer, new_endpoints, added, removed);
    }

    tglog_endpoint_list_destroy(added);
    tglog_endpoint_list_destroy(removed);
}

static void tglog_discoverer_notify_handlers(tglog_discoverer *discoverer,
                                             const tglog_endpoint_list *all,
                                             const tglog_endpoint_list *added,
                                             const tglog_endpoint_list *removed) {
    size_t i = 0;
    for (i = 0; i < discoverer->handler_count; i++) {
        if (discoverer->handlers[i].callback) {
            discoverer->handlers[i].callback(all, added, removed, discoverer->handlers[i].user_data);
        }
    }
}