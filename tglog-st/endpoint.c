/* Standard library headers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* System headers */
#include "tglog_platform.h"

/* Third-party library headers */

/* Project headers */
#include "endpoint.h"


tglog_endpoint *tglog_endpoint_create(const char *host, int port) {
    tglog_endpoint *ep = calloc(1, sizeof(tglog_endpoint));
    if (!ep) {
        return NULL;
    }

    ep->host = strdup(host);
    ep->port = port;
    char addr[256];
    /* Use [host]:port format for IPv6 addresses (those containing ':') */
    if (strchr(host, ':')) {
        snprintf(addr, sizeof(addr), "[%s]:%d", host, port);
    } else {
        snprintf(addr, sizeof(addr), "%s:%d", host, port);
    }
    ep->addr = strdup(addr);

    return ep;
}

void tglog_endpoint_destroy(struct tglog_endpoint *ep) {
    if (!ep) {
        return;
    }
    if (ep->host) {
        free(ep->host);
    }
    if (ep->addr) {
        free(ep->addr);
    }
    free(ep);
}

#define DEFAULT_ENDPOINT_LIST_CAPACITY 8

tglog_endpoint_list *tglog_endpoint_list_create(const char **hosts, int host_count, int port) {
    tglog_endpoint_list *list = calloc(1, sizeof(tglog_endpoint_list));
    if (!list) {
        return NULL;
    }

    list->capacity = host_count > DEFAULT_ENDPOINT_LIST_CAPACITY ? host_count : DEFAULT_ENDPOINT_LIST_CAPACITY;
    list->items = (tglog_endpoint **) malloc(list->capacity * sizeof(tglog_endpoint *));
    if (!list->items) {
        free(list);
        return NULL;
    }

    if (hosts) {
        int i = 0;
        for (i = 0; i < host_count; i++) {
            tglog_endpoint *ep = tglog_endpoint_create(hosts[i], port);
            if (!ep) {
                continue;
            }

            list->items[list->count++] = ep;
        }
    }

    return list;
}

void tglog_endpoint_list_destroy(tglog_endpoint_list *list) {
    if (!list) {
        return;
    }

    int i = 0;
    for (i = 0; i < list->count; i++) {
        tglog_endpoint_destroy(list->items[i]);
    }
    free(list->items);
    free(list);
}

tglog_endpoint_list *tglog_endpoint_list_copy(const tglog_endpoint_list *src) {
    if (!src) {
        return NULL;
    }

    tglog_endpoint_list *dst = tglog_endpoint_list_create(NULL, src->count, 0);
    int i = 0;
    for (i = 0; i < src->count; i++) {
        tglog_endpoint *ep = tglog_endpoint_create(src->items[i]->host, src->items[i]->port);
        if (!ep) {
            continue;
        }

        dst->items[dst->count++] = ep;
    }

    return dst;
}

bool tglog_endpoint_list_add(tglog_endpoint_list *list, tglog_endpoint *ep, bool copy) {
    if (!list || !ep) {
        return false;
    }

    if (list->count >= list->capacity) {
        tglog_endpoint **new_items = (tglog_endpoint **) realloc(list->items, 2 * list->capacity * sizeof(tglog_endpoint *));
        if (!new_items) {
            return false;
        }

        list->items = new_items;
        list->capacity *= 2;
    }

    if (copy) {
        tglog_endpoint *new_ep = tglog_endpoint_create(ep->host, ep->port);
        if (!new_ep) {
            return false;
        }
        list->items[list->count++] = new_ep;
    } else {
        list->items[list->count++] = ep;
    }

    return true;
}

void tglog_endpoint_list_remove(tglog_endpoint_list *list, char *addr) {
    int i = 0;
    for (i = list->count - 1; i >= 0; i--) {
        if (strcmp(list->items[i]->addr, addr) == 0) {
            tglog_endpoint_destroy(list->items[i]);

            if (i == list->count - 1) {
                list->items[i] = NULL;
            } else {
                list->items[i] = list->items[list->count - 1];
                list->items[list->count - 1] = NULL;
            }

            list->count--;
        }
    }
}