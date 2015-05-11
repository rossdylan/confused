#ifndef _H_CONFUSED_
#define _H_CONFUSED_

#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
#define FUSE_USE_VERSION 30

#include <fuse.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <stdbool.h>
#include <libmemcached-1.0/memcached.h>
#include <linux/limits.h>

static pthread_key_t mcd_conn;

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
    char link[PATH_MAX];
    size_t link_size;
} cf_link_data_t;

static memcached_st *get_or_create_memcached(void);
static cf_link_data_t *get_link_data(const char *path);


int cf_getattr(const char *path, struct stat *stat_buf);
int cf_readlink(const char *path, char *link, size_t size);
int cf_mknod(const char *path, mode_t mode, dev_t dev);
int cf_mkdir(const char *path, mode_t mode);
int cf_unlink(const char *path);
int cf_rmdir(const char *path);
int cf_symlink(const char *path, const char *link);
int cf_rename(const char *path, const char *new_path);
int cf_link(const char *path, const char *new_path);
int cf_chmod(const char *path, mode_t mode);
int cf_chown(const char *path, uid_t uid, gid_t gid);
int cf_truncate(const char *path, off_t new_size);
int cf_utime(const char *path, struct utimbuf *ubuf);
int cf_open(const char *path, struct fuse_file_info *fi);
int cf_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int cf_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int cf_statfs(const char *path, struct statvfs *statv);
int cf_flush(const char *path, struct fuse_file_info *fi);
int cf_release(const char *path, struct fuse_file_info *fi);
int cf_fsync(const char *path, int datasync, struct fuse_file_info *fi);

#ifdef HAVE_SYS_XATTR_H
int cf_setxattr(const char *path, const char *name, const char *value, size_t size, int flags);
int cf_getxattr(const char *path, const char *name, char *value, size_t size);
int cf_listxattr(const char *path, char *list, size_t size);
int cf_removexattr(const char *path, const char *name);
#endif

int cf_opendir(const char *path, struct fuse_file_info *fi);
int cf_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
int cf_releasedir(const char *path, struct fuse_file_info *fi);
int cf_fsyncdir(const char *path, int datasync, struct fuse_file_info *fi);
int cf_access(const char *path, int mask);
int cf_create(const char *path, mode_t mode, struct fuse_file_info *fi);
int cf_ftruncate(const char *path, off_t offset, struct fuse_file_info *fi);
int cf_fgetattr(const char *path, struct stat *statbuf, struct fuse_file_info *fi);

void *cf_init(struct fuse_conn_info *conn);
void cf_destroy(void *userdata);
int cf_opt_parse(void *data, const char *arg, int key, struct fuse_args *outargs);
int main(int argc, char *argv[]);


static struct fuse_opt cf_opts[] = {
    FUSE_OPT_KEY("root=%s", CF_OPT_ROOT),
    FUSE_OPT_KEY("memcached=%s", CF_OPT_MCCONF),
    FUSE_OPT_END
};


static struct fuse_operations cf_oper = {
    .getattr = cf_getattr,
    .readlink = cf_readlink,
    .getdir = NULL,
    .mknod = cf_mknod,
    .mkdir = cf_mkdir,
    .unlink = cf_unlink,
    .rmdir = cf_rmdir,
    .symlink = cf_symlink,
/**
    .rename = cf_rename,
    .link = cf_link,
    .chmod = cf_chmod,
    .chown = cf_chown,
    .truncate = cf_truncate,
    .open = cf_open,
    .read = cf_read,
    .write = cf_write,
    .statfs = cf_statfs,
    .flush = cf_flush,
    .release = cf_release,
    .fsync = cf_fsync,
#ifdef HAVE_SYS_XATTR_H
    .setxattr = cf_setxattr,
    .getxattr = cf_getxattr,
    .listxattr = cf_listxattr,
    .removexattr = cf_removexattr,
#endif
    .opendir = cf_opendir,
    .readdir = cf_readdir,
    .releasedir = cf_releasedir,
    .fsyncdir = cf_fsyncdir,
    .access = cf_access,
    .create = cf_create,
    .ftruncate = cf_ftruncate,
    .fgetattr = cf_fgetattr,
    .destroy = cf_destroy
    .init = cf_init,
**/
};
#endif
