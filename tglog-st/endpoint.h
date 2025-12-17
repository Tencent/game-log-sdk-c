#ifndef TGLOG_ENDPOINT_H
#define TGLOG_ENDPOINT_H

/* Standard library headers */
#include <stdbool.h>

/* System headers */

/* Third-party library headers */

/* Project headers */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * tglog_endpoint is a TGLog server endpoint
 */
typedef struct tglog_endpoint {
    char *host;             /* host name or ip */
    int port;               /* port */
    char *addr;             /* address: host:port */
} tglog_endpoint;

/**
 * Create a new endpoint.
 *
 * @param host The host name or IP address.
 * @param port The port number.
 * @return An endpoint or NULL on error.
 */
tglog_endpoint *tglog_endpoint_create(const char *host, int port);

/**
 * Destroy an endpoint.
 *
 * @param ep The endpoint to destroy.
 */
void tglog_endpoint_destroy(tglog_endpoint *ep);

/**
 * tglog_endpoint_list is a list of tglog_endpoint.
 */
typedef struct tglog_endpoint_list {
    struct tglog_endpoint **items;
    int capacity;
    int count;
} tglog_endpoint_list;

/**
 * Create a new endpoint list.
 *
 * @param hosts An array of host names.
 * @param host_count The number of host names.
 * @param port The port of the service.
 * @return An endpoint list or NULL on error.
 */
tglog_endpoint_list *tglog_endpoint_list_create(const char **hosts, int host_count, int port);

/**
 * Destroy an endpoint list.
 *
 * @param list The list to destroy.
 */
void tglog_endpoint_list_destroy(tglog_endpoint_list *list);

/**
 * Copy an endpoint list.
 *
 * @param src The source list.
 * @return A copy of the list or NULL on error.
 */
tglog_endpoint_list *tglog_endpoint_list_copy(const tglog_endpoint_list *src);

/**
 * Add an endpoint to a list.
 *
 * @param list The endpoint list.
 * @param ep The endpoint to add.
 * @param copy Whether to copy the endpoint, if not, the endpoint will be stored in the list, and it MUST be allocated on heap.
 * @return true on success, false on error.
 */
bool tglog_endpoint_list_add(tglog_endpoint_list *list, tglog_endpoint *ep, bool copy);

/**
 * Remove all the endpoints with the same address from a list.
 *
 * @param list The endpoint list.
 * @param addr The endpoint address to remove.
 */
void tglog_endpoint_list_remove(tglog_endpoint_list *list, char *addr);

#ifdef __cplusplus
}
#endif

#endif /* TGLOG_ENDPOINT_H */