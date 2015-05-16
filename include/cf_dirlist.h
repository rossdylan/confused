#ifndef _H_CF_DIR_LIST
#define _H_CF_DIR_LIST

#include "cf_types.h"
#include <stdbool.h>



/**
 * Add a new link to a directory
 */
int cf_dirlist_update(const char *link);

/**
 * Grab a directory listing from memcached
 */
cf_dirlist_t * cf_dirlist_get(const char *dir);

/**
 * create/replace a cf_dirlist_t in memcached
 */
bool cf_dirlist_set(const char *dir, cf_dirlist_t *dl);

/**
 * Delete a cf_dirlist_t in memcached
 */
bool cf_dirlist_delete(const char *dir);

bool cf_dirlist_exists(const char *dir);
#endif
