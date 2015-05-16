#ifndef _H_CF_TYPES
#define _H_CF_TYPES

// We need to get the PATH_MAX define in different ways for each OS
#ifdef __linux__
#include <linux/limits.h>
#endif

#if defined(__FreeBSD__)
#include <sys/cdefs.h>
#endif

#if defined(__APPLE__) || defined(__FreeBSD__)
#include <sys/syslimits.h>
#endif

#include <sys/stat.h>
#include <stdlib.h>


/**
 * Custom options for calling fuse
 */
enum {
    CF_OPT_ROOT, // location we are overlaying
    CF_OPT_MCCONF // memcache config string
};


/**
 * Config struct populated by the arg parser
 */
typedef struct {
    char *config_str;
    size_t config_len;
    char cwd[PATH_MAX];
} cf_config_t;


/**
 * Struct for the data we store in memcached for a symlink
 */
typedef struct {
    struct stat stat_buf;
    char target[PATH_MAX];
    char next[PATH_MAX];
} cf_link_t;

/**
 * Directory entry in memcached Keeps track of the first link in a directory
 * which points to all the others. LL style
 */
typedef struct {
    char head[PATH_MAX];
    char tail[PATH_MAX];
    size_t size;
} cf_dirlist_t;

#endif
