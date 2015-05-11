#include "confused.h"
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdio.h>

/**
 * constructor for initializing the per thread memcache variable
 * This way each thread of fuse has its own memcached connection.
 */
__attribute__((constructor)) static void init_memcache(void) {
    pthread_key_create(&mcd_conn, NULL);
}

/**
 * Translate a path within the overlay back into a path within the root
 */
static inline void overlay_path(const char *path, char *overlay) {
    const char *root = ((cf_config_t *)fuse_get_context()->private_data)->cwd;
    if(strcmp("/", path) == 0) {
        memcpy(overlay, root, strlen(root));
    }
    else {
        if(path[0] == '/') {
            snprintf(overlay, PATH_MAX, "%s%s", root, path);
        }
        else {
            snprintf(overlay, PATH_MAX, "%s/%s", root, path);
        }
    }
}


/**
 * Create a new connection to memcached or grab the existing thread local
 * connection.
 */
static memcached_st *get_or_create_memcached(void) {
    memcached_st *server = (memcached_st *)pthread_getspecific(mcd_conn);
    if(server == NULL) {
        cf_config_t *config = (cf_config_t *)fuse_get_context()->private_data;
        if((server = memcached(config->config_str, config->config_len)) == NULL) {
            // well shit...
            return NULL;
        }
        pthread_setspecific(mcd_conn, server);
    }
    return server;
}

/**
 * Return the link data from memcached if it exists and NULL if it doesn't
 */
static cf_link_data_t *get_link_data(const char *path) {
    memcached_st *ms = get_or_create_memcached();
    size_t path_size = strlen(path);
    if(memcached_exist(ms, path, path_size) == MEMCACHED_SUCCESS) {
        uint32_t flags;
        memcached_return_t error;
        size_t value_size = sizeof(cf_link_data_t);
        cf_link_data_t *data = (cf_link_data_t *)memcached_get(ms,
                path,
                path_size,
                &value_size,
                &flags,
                &error);
        printf("Retrieved symlink: %s\n", path);
        printf("\t link=%s\n", data->link);
        return data;
    }
    printf("No symlink found: %s\n", path);
    return NULL;
}

/**
 * ==========================
 * FS implementation is below
 * ==========================
 */

int cf_getattr(const char *path, struct stat *stat_buf) {
    char opath[PATH_MAX];
    overlay_path(path, opath);

    cf_link_data_t *link = get_link_data(path);
    printf("Getting attrs for %s\n", opath);
    if(link == NULL) {
        printf("getattr passed through\n");
        printf("lstat(%s %p)\n", path, stat_buf);
        if(lstat(opath, stat_buf) == -1) {
            return -errno;
        }
        return 0;
    }
    else {
        memcpy(stat_buf, &link->stat_buf, sizeof(struct stat));
        free(link);
        return 0;
    }
}

/**
 * All links are stored in memcached so readlink should always go there for
 * link information.
 */
int cf_readlink(const char *path, char *link, size_t size) {
    printf("Reading link for %s\n", path);
    size_t path_size = strlen(path);
    cf_link_data_t *data = get_link_data(path);
    if(data == NULL) {
        errno = ENOENT; // Maybe this could be chose better?
        return -1;
    }
    else {
        strncpy(link, data->link, size);
        free(data);
        return 0;
    }
}

int cf_mknod(const char *path, mode_t mode, dev_t dev) {
    char opath[PATH_MAX];
    overlay_path(path, opath);
    int res;
    printf("I'm trying to make %s\n", opath);
    if(S_ISREG(mode)) {
        printf("Making a regular file\n");
        res = open(opath, O_CREAT | O_EXCL | O_WRONLY, mode);
        if(res >= 0) {
            close(res);
        }
    }
    else if(S_ISFIFO(mode)) {
        res = mkfifo(opath, mode);
    }
    else {
        res = mknod(opath, mode, dev);
    }
    if(res == -1) {
        return -errno;
    }
    return 0;
}

int cf_mkdir(const char *path, mode_t mode) {
    char opath[PATH_MAX];
    overlay_path(path, opath);
    if(mkdir(opath, mode) == -1) {
        return -errno;
    }
    return 0;
}

/**
 * Unlink can operate on a symlink so we need to check memcached
 */
int cf_unlink(const char *path) {
    memcached_st *ms = get_or_create_memcached();
    size_t path_size = strlen(path);
    printf("Unlinking %s\n", path);
    if(memcached_exist(ms, path, path_size) != MEMCACHED_SUCCESS) {
        // its not in memcache, maybe its on the real fs
        char opath[PATH_MAX];
        overlay_path(path, opath);
        size_t opath_size = strlen(opath);
        if(unlink(opath) == -1) {
            return -errno;
        }
        return 0;
    }
    else {
        if(memcached_delete(ms, path, path_size, 0) == MEMCACHED_SUCCESS) {
            return 0;
        }
        else {
            errno = EIO;
            return -errno;
        }
    }
}


int cf_rmdir(const char *path) {
    char opath[PATH_MAX];
    overlay_path(path, opath);
    if(rmdir(opath) == -1) {
        return -errno;
    }
    return 0;
}

/**
 * WOO time to make us some symlinks in memcached
 */
int cf_symlink(const char *path, const char *link) {
    printf("Link %s\n", link);
    printf("Path %s\n", path);

    size_t path_size = strlen(path);
    size_t link_size = strlen(link);
    int res;
    if(path_size+1 > PATH_MAX || link_size+1 > PATH_MAX) {
        errno = ENAMETOOLONG;
        res = -errno;
        goto cf_symlink_return;
    }
    memcached_st *ms = get_or_create_memcached();
    if(memcached_exist(ms, link, link_size) == MEMCACHED_SUCCESS) {
        errno = EEXIST;
        res = -errno;
        goto cf_symlink_return;
    }
    else {
        cf_link_data_t new_link;
        memset(new_link.link, 0, sizeof(new_link.link));
        strncpy(new_link.link, path, path_size);
        // set up our modes and other file permission shits.
        new_link.stat_buf.st_mode = S_IFLNK | S_IRWXU | S_IRWXG | S_IRWXO;
        new_link.stat_buf.st_size = (long)path_size;
        new_link.stat_buf.st_blocks = path_size / 512;
        new_link.stat_buf.st_blksize = 4096;
        new_link.stat_buf.st_uid = getuid();
        new_link.stat_buf.st_gid = getgid();
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        memcpy(&new_link.stat_buf.st_atim, &ts, sizeof(ts));
        memcpy(&new_link.stat_buf.st_mtim, &ts, sizeof(ts));
        memcpy(&new_link.stat_buf.st_ctim, &ts, sizeof(ts));

        void * totes_a_str = (void *)&new_link;
        if(memcached_set(ms, link, link_size, (const char *)totes_a_str, sizeof(new_link), (time_t)0, 0) == MEMCACHED_SUCCESS) {
            printf("I made a symlink %s -> %s\n", link, new_link.link);
            res = 0;
            goto cf_symlink_return;
        }
        else {
            errno = EIO;
            res = -errno;
            goto cf_symlink_return;
        }
    }
cf_symlink_return:
    return res;
}


/***
 * ================================================
 * FUSE setup and configuration is below this point
 * ================================================
 */
 

/**
 * I've borrowed this from overlayfs-fuse. 
 */
static char *get_opt_str(const char *arg, char *opt_name) {
        char *str = index(arg, '=');
        if (!str) {
                fprintf(stderr,
                        "-o %s parameter not properly specified, aborting!\n",
                        opt_name);
                exit(1); // still early phase, we can abort
        }
        if (strlen(str) < 3) {
                fprintf(stderr,
                        "%s path has not sufficient characters, aborting!\n",
                        opt_name);
                exit(1);
        }
        str++; // just jump over the '='
        // copy of the given parameter, just in case something messes around
        // with command line parameters later on
        str = strdup(str);
        if (!str) {
                fprintf(stderr, "strdup failed: %s Aborting!\n", strerror(errno));
                exit(1);
        }
        return str;
}


int cf_opt_parse(void *data, const char *arg, int key, struct fuse_args *outargs) {
    cf_config_t *config = (cf_config_t *)data;
    int res = 0;
    size_t arg_len = strlen(arg) + 1; //+1 for \0
    if(key == CF_OPT_ROOT) {
        char *thing = get_opt_str(arg, "root");
        realpath(thing, config->cwd);
        printf("I'm expanding %s\n", arg);
        return 0;
    }
    if(key == CF_OPT_MCCONF) {
        if((config->config_str = malloc(arg_len)) == NULL) {
            perror("malloc");
            return -1;
        }
        config->config_str = get_opt_str(arg, "memcached");
        config->config_len = strlen(config->config_str);
        printf("My memcached config is: %s\n", config->config_str);
        return 0;
    }
    else {
        return 1;
    }
}


int main(int argc, char *argv[]) {
    umask(0);
    cf_config_t *config = malloc(sizeof(cf_config_t));
    memset(config, 0, sizeof(cf_config_t));
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    fuse_opt_parse(&args, config, cf_opts, cf_opt_parse);
    printf("My path is: %s\n", config->cwd);
    return fuse_main(args.argc, args.argv, &cf_oper, config);
}
