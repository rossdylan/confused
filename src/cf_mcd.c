
#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
#define FUSE_USE_VERSION 30

#include "cf_mcd.h"
#include "cf_types.h"
#include <fuse.h>
#include <stdio.h>
#include <string.h>


void cf_mcd_init(void) {
    pthread_key_create(&mcd_conn, NULL);
}


/**
 * Get or create the thread local connection to the memcached server located in
 * mcd_conn. Each thread has its own instance.
 */
memcached_st *cf_mcd_get_server(void) {
    memcached_st *server = (memcached_st *)pthread_getspecific(mcd_conn);
    if(server == NULL) {
        cf_config_t *config = (cf_config_t *)fuse_get_context()->private_data;
        if((server = memcached(config->config_str, config->config_len)) == NULL) {
            // well shit...
            return NULL;
        }
        memcached_behavior_set(server, MEMCACHED_BEHAVIOR_BINARY_PROTOCOL, 1);
        pthread_setspecific(mcd_conn, server);
    }
    return server;
}


/**
 * Given a key, a ptr to some memory, and the size of that memory, store it in
 * memcache under the given key.
 */
bool cf_mcd_set(const char *key, void *ptr, size_t size) {
    memcached_st *ms = cf_mcd_get_server();
    memcached_return_t rt = memcached_set(ms, key, strlen(key)+1, (const char *)ptr, size, (time_t)0, 0);
    return rt == MEMCACHED_SUCCESS;
}


/**
 * Given a key and the size of the data to return, return a void pointer to
 * the data store in memcached with the given key.
 */
void *cf_mcd_get(const char *key, size_t size) {
    memcached_st *ms = cf_mcd_get_server();
    uint32_t flags;
    memcached_return_t error;
    return (void *)memcached_get(ms,
            key,
            strlen(key)+1,
            &size,
            &flags,
            &error);
}

bool cf_mcd_delete(const char *key) {
    memcached_st *ms = cf_mcd_get_server();
    return memcached_delete(ms, key, strlen(key)+1, 0) == MEMCACHED_SUCCESS;
}

bool cf_mcd_exists(const char *key) {
    memcached_st *ms = cf_mcd_get_server();
    return memcached_exist(ms, key, strlen(key)+1) == MEMCACHED_SUCCESS;
}

uint32_t cf_mcd_hash(const char *key) {
    return memcached_generate_hash_value(key, strlen(key)+1, MEMCACHED_HASH_DEFAULT);
}
