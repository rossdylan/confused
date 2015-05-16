#ifndef _H_CF_LINK
#define _H_CF_LINK

#include "cf_types.h"
#include <stdbool.h>

/**
 * Grab the link stored in memcached with the given path
 */
cf_link_t *cf_link_get(const char *path);

/**
 * Set a link in memcached with the given path
 */
bool cf_link_set(const char *path, cf_link_t *link);

/**
 * Remove a link from memcached with the given path
 */
bool cf_link_delete(const char *path);

bool cf_link_exists(const char *path);
#endif
