#include "confused.h"
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <libgen.h>
#include <dirent.h>
#include "cf_dirlist.h"
#include "cf_link.h"
#include "cf_mcd.h"


/**
 * Translate a path within the overlay back into a path within the root
 */
static void overlay_path(const char *path, char *overlay) {
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
 * Reuturn an array containing all links in a cf_dirent
 */
int cf_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    char opath[PATH_MAX];
    memset(opath, 0, sizeof(opath));
    overlay_path(path, opath);

    // Do our standard root fs readdir
    DIR *dp = NULL;
    struct dirent *de = NULL;
    if((dp = opendir(opath)) == NULL) {
        return -errno;
    }
    struct stat st;
    while((de = readdir(dp)) != NULL) {
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        if(filler(buf, de->d_name, &st, 0)) {
            break;
        }
    }

    // How we take a look at the links
    cf_dirlist_t *dirlist = cf_dirlist_get(path);
    if(dirlist != NULL && dirlist->size > 0) {
        cf_link_t *prev = cf_link_get(dirlist->head);

        char prev_link[PATH_MAX];
        memset(prev_link, 0, sizeof(prev_link));
        memcpy(prev_link, dirlist->head, strlen(dirlist->head)+1);

        cf_link_t *current = NULL;
        struct stat st;
        for(size_t index=0; index<dirlist->size; index++) {
            memset(&st, 0, sizeof(st));
            st.st_ino = prev->stat_buf.st_ino;
            st.st_mode = prev->stat_buf.st_mode;
            char trimmed[PATH_MAX];
            memcpy(trimmed, &prev_link[1], strlen(prev_link));
            if(filler(buf, trimmed, &st, 0)) {
                free(prev);
                prev = NULL;
                break;
            }
            if(prev->next[0] == '\0') {
                free(prev);
                prev = NULL;
                break;
            }
            else {
                memset(prev_link, 0, sizeof(prev_link));
                memcpy(prev_link, prev->next, strlen(prev->next)+1);
                free(prev);
                prev = NULL;
                prev = cf_link_get(prev_link);
            }
        }
    }
    free(dirlist);
    closedir(dp);
    return 0;
}


/**
 * ==========================
 * FS implementation is below
 * ==========================
 */

int cf_getattr(const char *path, struct stat *stat_buf) {
    char opath[PATH_MAX];
    memset(opath, 0, sizeof(opath));
    overlay_path(path, opath);

    cf_link_t *link = cf_link_get(path);
    if(link == NULL) {
        printf("cf_getattr(%s) -> lstat(%s)\n", path, opath);
        if(lstat(opath, stat_buf) == -1) {
            return -errno;
        }
        return 0;
    }
    else {
        printf("cf_getattr(%s) -> %s\n", path, link->target);
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
    cf_link_t *data = cf_link_get(path);
    if(data == NULL) {
        errno = ENOENT;
        return -1;
    }
    else {
        strncpy(link, data->target, size);
        free(data);
        return 0;
    }
}


int cf_mknod(const char *path, mode_t mode, dev_t dev) {
    char opath[PATH_MAX];
    overlay_path(path, opath);
    int res;
    if(S_ISREG(mode)) {
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
    printf("Unlinking %s\n", path);
    if(!cf_link_delete(path)) {
        // its not in memcache, maybe its on the real fs
        char opath[PATH_MAX];
        overlay_path(path, opath);
        if(unlink(opath) == -1) {
            return -errno;
        }
    }
    return 0;
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

    size_t path_size = strlen(path)+1;
    size_t link_size = strlen(link)+1;
    int res = 0;
    if(path_size > PATH_MAX || link_size > PATH_MAX) {
        errno = ENAMETOOLONG;
        res = -errno;
        goto cf_symlink_return;
    }
    if(cf_link_exists(path)) {
        errno = EEXIST;
        res = -errno;
        goto cf_symlink_return;
    }
    else {
        cf_link_t new_link;
        memset(&new_link, 0, sizeof(cf_link_t));
        strncpy(new_link.target, path, path_size);
        // set up our modes and other file permission shits.
        new_link.stat_buf.st_ino = cf_mcd_hash(path);
        new_link.stat_buf.st_mode = S_IFLNK | S_IRWXU | S_IRWXG | S_IRWXO;
        new_link.stat_buf.st_size = (long)path_size;
        new_link.stat_buf.st_blocks = path_size / 512;
        new_link.stat_buf.st_blksize = 4096;
        new_link.stat_buf.st_uid = getuid();
        new_link.stat_buf.st_gid = getgid();
        struct timespec ts;
        size_t ts_size = sizeof(ts);
        clock_gettime(CLOCK_REALTIME, &ts);
        memcpy(&new_link.stat_buf.st_atim, &ts, ts_size);
        memcpy(&new_link.stat_buf.st_mtim, &ts, ts_size);
        memcpy(&new_link.stat_buf.st_ctim, &ts, ts_size);

        if(cf_link_set(link, &new_link)) {
            printf("I made a symlink %s\n", new_link.target);
            cf_dirlist_update(link);
            res = 0;
            goto cf_symlink_return;
        }
        else {
            printf("[ERROR] symlink creation failed\n");
            errno = EIO;
            res = -errno;
            goto cf_symlink_return;
        }
    }
cf_symlink_return:
    return res;
}

int cf_open(const char *path, struct fuse_file_info *fi) {
    int fd;
    cf_link_t *link = cf_link_get(path);
    if(link != NULL) {
        fd = open(link->target, fi->flags);
    }
    else {
        char opath[PATH_MAX];
        overlay_path(path, opath);
        fd = open(opath, fi->flags);
    }
    if(fd == -1) {
        return -errno;
    }
    fi->fh =fd;
    return 0;
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
    cf_mcd_init();
    return fuse_main(args.argc, args.argv, &cf_oper, config);
}
