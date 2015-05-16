#include "cf_link.h"
#include "cf_mcd.h"


/**
 * Grab the link stored in memcached with the given path
 */
cf_link_t *cf_link_get(const char *path) {
    return (cf_link_t *)cf_mcd_get(path, sizeof(cf_link_t));
}

/**
 * Set a link in memcached with the given path
 */
bool cf_link_set(const char *path, cf_link_t *link) {
    return cf_mcd_set(path, link, sizeof(cf_link_t));
}

/**
 * Remove a link from memcached with the given path
 */
bool cf_link_delete(const char *path) {
    return cf_mcd_delete(path);
}

bool cf_link_exists(const char *path) {
    return cf_mcd_exists(path);
}
