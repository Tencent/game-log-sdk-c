#ifndef TGLOG_DISCOVERER_H
#define TGLOG_DISCOVERER_H

/* Standard library headers */
#include <stdbool.h>

/* Third-party library headers */

/* Project headers */
#include "debug_logger.h"
#include "endpoint.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef struct tglog_discoverer tglog_discoverer;

/**
 * Create a DNS discoverer.
 *
 * @param domain The domain name of the service.
 * @param port The port of the service.
 * @param interval_sec Update interval in seconds.
 * @param logger The debug logger
 * @return A new discoverer instance or NULL on error.
 */
tglog_discoverer *tglog_discoverer_create(const char *domain, int port, int interval_sec, tglog_debug_logger *logger);

/**
 * Destroy a DNS discoverer.
 *
 * @param discoverer The discoverer to destroy.
 */
void tglog_discoverer_destroy(tglog_discoverer *discoverer);

/**
 * Get current endpoint list.
 *
 * The caller need to destroy the returned tglog_endpoint_list.
 * @param discoverer The discoverer.
 * @return The current endpoint list (internal pointer, do not free).
 */
tglog_endpoint_list *tglog_discoverer_get_endpoints(tglog_discoverer *discoverer);

/**
 * Get the number of available endpoints.
 *
 * @param discoverer The discoverer.
 * @return The number of available endpoints.
 */
int tglog_discoverer_get_endpoint_count(tglog_discoverer *discoverer);

/**
 * Callback function to be called when the endpoints are updated.
 *
 * @param all The current endpoints.
 * @param added The added endpoints.
 * @param removed The removed endpoints.
 * @param user_data The user data passed to the callback.
 */
typedef void (*endpoint_update_func)(const tglog_endpoint_list *all,
                                     const tglog_endpoint_list *added,
                                     const tglog_endpoint_list *removed,
                                     void *user_data);

/**
 * Add a callback to be called when the endpoints are updated.
 *
 * @param discoverer The discoverer.
 * @param callback The callback function.
 * @param user_data The user data to be passed to the callback.
 * @return true on success, false on error.
 */
bool tglog_discoverer_add_handler(tglog_discoverer *discoverer,
                                  endpoint_update_func callback,
                                  void *user_data);

/**
 * Remove a callback.
 *
 * @param discoverer The discoverer.
 * @param callback The callback function.
 * @return true on success, false on error.
 */
bool tglog_discoverer_remove_handler(tglog_discoverer *discoverer,
                                     endpoint_update_func callback);

#ifdef __cplusplus
}
#endif

#endif /* TGLOG_DISCOVERER_H */