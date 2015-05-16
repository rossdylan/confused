#ifndef _H_CF_MCD
#define _H_CF_MCD


#include <libmemcached-1.0/memcached.h>
#include <pthread.h>
#include <stdbool.h>


/**
 * Thread local for accessing the connection to memcached.
 * Each thread makes its own connection, rather then sharing one between
 * threads.
 */
pthread_key_t mcd_conn;

/**
 * Called before fuse makes a bunch of threads. Right now all this does is
 * make sure the pthread_ket_t mcd_conn is all set up and ready to be used.
 */
void cf_mcd_init(void);

/**
 * Get the memcached_st for this thread of make a new one.
 */
memcached_st *cf_mcd_get_server(void);

/**
 * Set the value of the given key to the value of the struct located at ptr and
 * with the given size.
 */
bool cf_mcd_set(const char *key, void *ptr, size_t size);

/**
 * Return the memory located at the given key of the given size
 */
void *cf_mcd_get(const char *key, size_t size);

/**
 * Remove the data stroed with the given key in memcached
 */
bool cf_mcd_delete(const char *key);

/**
 * Check if a key exists in memcached
 */
bool cf_mcd_exists(const char *key);

uint32_t cf_mcd_hash(const char *key);
#endif
