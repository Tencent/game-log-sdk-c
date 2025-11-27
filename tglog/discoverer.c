/* Standard library headers */
#include <stdlib.h>
#include <string.h>

/* System headers */
#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else

#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/time.h>

#endif

/* Third-party library headers */
#include "event2/event.h"
#include "event2/dns.h"

/* Project headers */
#include "discoverer.h"


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

#ifdef _WIN32
    HANDLE thread;
    CONDITION_VARIABLE ready_cond;
    CRITICAL_SECTION ready_lock;
    SRWLOCK lock;                           /* Read-write lock for handlers and endpoints */
#else
    pthread_t thread;
    pthread_cond_t ready_cond;
    pthread_mutex_t ready_lock;
    pthread_rwlock_t lock;                  /* Read-write lock for handlers and endpoints */
#endif
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

#ifdef _WIN32
static DWORD WINAPI tglog_discoverer_thread(LPVOID arg)
#else

static void *tglog_discoverer_thread(void *arg)
#endif
{
    tglog_discoverer *discoverer = (tglog_discoverer *) arg;

    event_base_dispatch(discoverer->base);

    return NULL;
}

static void tglog_discoverer_on_timer(evutil_socket_t fd, short event, void *arg) {
    tglog_discoverer *discoverer = (tglog_discoverer *) arg;
    tglog_debug(discoverer->logger, "Discoverer refresh");
    (void) (fd);
    (void) (event);
    tglog_discoverer_lookup(discoverer);
}

tglog_discoverer *tglog_discoverer_create(const char *domain, int port, int interval_sec, tglog_debug_logger *logger) {
    if (!domain || port <= 0 || port > 65535) {
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
    discoverer->base = event_base_new();
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

#ifdef _WIN32
    InitializeSRWLock(&discoverer->lock);
    InitializeCriticalSection(&discoverer->ready_lock);
    InitializeConditionVariable(&discoverer->ready_cond);
#else
    pthread_rwlock_init(&discoverer->lock, NULL);
    pthread_mutex_init(&discoverer->ready_lock, NULL);
    pthread_cond_init(&discoverer->ready_cond, NULL);
#endif

    tglog_discoverer_lookup(discoverer);

#ifdef _WIN32
    discoverer->thread = CreateThread(NULL, 0, tglog_discoverer_thread, discoverer, 0, NULL);
    if (discoverer->thread == NULL) {
        tglog_error(discoverer->logger, "Create discoverer thread failed");
        tglog_discoverer_destroy(discoverer);
        return NULL;
    }

    EnterCriticalSection(&discoverer->ready_lock);
    while (!discoverer->ready) {
        SleepConditionVariableCS(&discoverer->ready_cond, &discoverer->ready_lock, INFINITE);
    }
    LeaveCriticalSection(&discoverer->ready_lock);
#else
    int ret = pthread_create(&discoverer->thread, NULL, tglog_discoverer_thread, discoverer);
    if (ret != 0) {
        tglog_error(discoverer->logger, "Create discoverer thread failed");
        tglog_discoverer_destroy(discoverer);
        return NULL;
    }

    pthread_mutex_lock(&discoverer->ready_lock);
    while (!discoverer->ready) {
        pthread_cond_wait(&discoverer->ready_cond, &discoverer->ready_lock);
    }
    pthread_mutex_unlock(&discoverer->ready_lock);
#endif

    if (!discoverer->endpoints || discoverer->endpoints->count == 0) {
        tglog_error(logger, "No endpoints resolved, domain: %s", domain);
        tglog_discoverer_destroy(discoverer);
        return NULL;
    }

    struct timeval tv = {discoverer->interval_sec, 0};
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

    if (discoverer->base) {
        event_base_loopbreak(discoverer->base);
    }
#ifdef _WIN32
    WaitForSingleObject(discoverer->thread, INFINITE);
    CloseHandle(discoverer->thread);
    DeleteCriticalSection(&discoverer->ready_lock);
    /* SRWLock doesn't need explicit destruction */
#else
    if (discoverer->thread) {
        pthread_join(discoverer->thread, NULL);
    }
    pthread_mutex_destroy(&discoverer->ready_lock);
    pthread_cond_destroy(&discoverer->ready_cond);
    pthread_rwlock_destroy(&discoverer->lock);
#endif

    if (discoverer->timer_event) {
        event_del(discoverer->timer_event);
        event_free(discoverer->timer_event);
    }

    if (discoverer->dns_base) {
        evdns_base_free(discoverer->dns_base, 0);
    }

    if (discoverer->base) {
        event_base_free(discoverer->base);
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

#ifdef _WIN32
    AcquireSRWLockShared(&discoverer->lock);
#else
    pthread_rwlock_rdlock(&discoverer->lock);
#endif

    tglog_endpoint_list *result = tglog_endpoint_list_copy(discoverer->endpoints);

#ifdef _WIN32
    ReleaseSRWLockShared(&discoverer->lock);
#else
    pthread_rwlock_unlock(&discoverer->lock);
#endif

    return result;
}

int tglog_discoverer_get_endpoint_count(tglog_discoverer *discoverer) {
    if (!discoverer || !discoverer->endpoints) {
        return 0;
    }

#ifdef _WIN32
    AcquireSRWLockShared(&discoverer->lock);
#else
    pthread_rwlock_rdlock(&discoverer->lock);
#endif

    int count = discoverer->endpoints->count;

#ifdef _WIN32
    ReleaseSRWLockShared(&discoverer->lock);
#else
    pthread_rwlock_unlock(&discoverer->lock);
#endif

    return count;
}

bool tglog_discoverer_add_handler(tglog_discoverer *discoverer,
                                  endpoint_update_func callback,
                                  void *user_data) {
    if (!discoverer || !callback) {
        return false;
    }

#ifdef _WIN32
    AcquireSRWLockExclusive(&discoverer->lock);
#else
    pthread_rwlock_wrlock(&discoverer->lock);
#endif

    size_t i = 0;
    for (i = 0; i < discoverer->handler_count; i++) {
        if (discoverer->handlers[i].callback == callback) {
#ifdef _WIN32
            ReleaseSRWLockExclusive(&discoverer->lock);
#else
            pthread_rwlock_unlock(&discoverer->lock);
#endif
            return true; /* Already exists */
        }
    }

    endpoint_event_handler *new_handlers = (endpoint_event_handler *) realloc(discoverer->handlers, (discoverer->handler_count + 1) * sizeof(endpoint_event_handler));
    if (!new_handlers) {
        tglog_fatal(discoverer->logger, "Failed to allocate memory for handlers");
#ifdef _WIN32
        ReleaseSRWLockExclusive(&discoverer->lock);
#else
        pthread_rwlock_unlock(&discoverer->lock);
#endif
        return false;
    }

    discoverer->handlers = new_handlers;
    discoverer->handlers[discoverer->handler_count].callback = callback;
    discoverer->handlers[discoverer->handler_count].user_data = user_data;
    discoverer->handler_count++;

#ifdef _WIN32
    ReleaseSRWLockExclusive(&discoverer->lock);
#else
    pthread_rwlock_unlock(&discoverer->lock);
#endif
    return true;
}

bool tglog_discoverer_remove_handler(tglog_discoverer *discoverer,
                                     endpoint_update_func callback) {
    if (!discoverer || !callback) {
        return false;
    }

#ifdef _WIN32
    AcquireSRWLockExclusive(&discoverer->lock);
#else
    pthread_rwlock_wrlock(&discoverer->lock);
#endif

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

#ifdef _WIN32
            ReleaseSRWLockExclusive(&discoverer->lock);
#else
            pthread_rwlock_unlock(&discoverer->lock);
#endif
            return true;
        }
    }

#ifdef _WIN32
    ReleaseSRWLockExclusive(&discoverer->lock);
#else
    pthread_rwlock_unlock(&discoverer->lock);
#endif
    return false;
}

static void tglog_discoverer_set_ready_once(tglog_discoverer *discoverer) {
#ifdef _WIN32
    EnterCriticalSection(&discoverer->ready_lock);
    if (!discoverer->ready) {
        discoverer->ready = true;
        WakeConditionVariable(&discoverer->ready_cond);
        tglog_debug(discoverer->logger, "Discoverer ready after first DNS callback");
    }
    LeaveCriticalSection(&discoverer->ready_lock);
#else
    pthread_mutex_lock(&discoverer->ready_lock);
    if (!discoverer->ready) {
        discoverer->ready = true;
        pthread_cond_signal(&discoverer->ready_cond);
        tglog_debug(discoverer->logger, "Discoverer ready after first DNS callback");
    }
    pthread_mutex_unlock(&discoverer->ready_lock);
#endif
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

#ifdef _WIN32
    AcquireSRWLockExclusive(&discoverer->lock);
#else
    pthread_rwlock_wrlock(&discoverer->lock);
#endif

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

#ifdef _WIN32
    ReleaseSRWLockExclusive(&discoverer->lock);
#else
    pthread_rwlock_unlock(&discoverer->lock);
#endif

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
#ifdef _WIN32
    AcquireSRWLockShared(&discoverer->lock);
#else
    pthread_rwlock_rdlock(&discoverer->lock);
#endif

    size_t i = 0;
    for (i = 0; i < discoverer->handler_count; i++) {
        if (discoverer->handlers[i].callback) {
            discoverer->handlers[i].callback(all, added, removed, discoverer->handlers[i].user_data);
        }
    }
#ifdef _WIN32
    ReleaseSRWLockShared(&discoverer->lock);
#else
    pthread_rwlock_unlock(&discoverer->lock);
#endif
}
