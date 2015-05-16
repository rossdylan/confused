#include "cf_dirlist.h"
#include "cf_mcd.h"
#include "cf_types.h"
#include "cf_link.h"
#include <libgen.h>
#include <stdio.h>
#include <string.h>


/**
 * Add a new link to a directory
 */
int cf_dirlist_update(const char *link) {
    size_t link_size = strlen(link)+1;

    char link_copy[PATH_MAX];
    memset(&link_copy, 0, sizeof(link_copy));
    memcpy(&link_copy, link, strlen(link)+1);

    char *dir = dirname(link_copy);

    cf_dirlist_t *dirlist = cf_dirlist_get(dir);
    if(dirlist != NULL) {
        cf_link_t *old_tail = cf_link_get(dirlist->tail);

        // Update the linked list prev/next pointers
        memset(old_tail->next, 0, sizeof(old_tail->next));
        memcpy(old_tail->next, link, link_size);

        if(cf_link_set(dirlist->tail, old_tail)) {
            printf("Updated old-tail: %s\n", dirlist->tail);
        }
        else {
            printf("Updating old-tail: %s HAS GONE WRONG\n", dirlist->tail);
        }

        // update the directory list
        memset(dirlist->tail, 0, sizeof(dirlist->tail));
        memcpy(dirlist->tail, link, link_size);
        dirlist->size += 1;
        if(cf_dirlist_set(dir, dirlist)) {
            printf("Updated dirlist: %s\n", dir);
        }
        else {
            printf("dirlist update: %s HAS GONE WRONG\n", dir);
        }
        // free everything
        free(dirlist);
        free(old_tail);
        return 0;
    }
    else {
        cf_dirlist_t dirlist;
        memset(&dirlist, 0, sizeof(cf_dirlist_t));
        memcpy(dirlist.head, link, link_size);
        memcpy(dirlist.tail, link, link_size);
        dirlist.size = 1;
        if(cf_dirlist_set(dir, &dirlist)) {
            printf("New dirlist: %s\n", dir);
            return 0;
        }
        else {
            return -1;
        }
    }
}

/**
 * Grab a directory listing from memcached
 */
cf_dirlist_t * cf_dirlist_get(const char *dir) {
    char *dir_key = NULL;
    asprintf(&dir_key, "dirent-%s", dir);
    cf_dirlist_t *dirlist = (cf_dirlist_t *) cf_mcd_get(dir_key, sizeof(cf_dirlist_t));
    free(dir_key);
    return dirlist;
}

/**
 * create/replace a cf_dirlist_t in memcached
 */
bool cf_dirlist_set(const char *dir, cf_dirlist_t *dl) {
    char *dir_key = NULL;
    asprintf(&dir_key, "dirent-%s", dir);
    bool res = cf_mcd_set(dir_key, dl, sizeof(cf_dirlist_t));
    free(dir_key);
    return res;
}

bool cf_dirlist_delete(const char *dir) {
    char *dir_key = NULL;
    asprintf(&dir_key, "dirent-%s", dir);
    bool res = cf_mcd_delete(dir_key);
    free(dir_key);
    return res;
}

bool cf_dirlist_exists(const char *dir) {
    char *dir_key = NULL;
    asprintf(&dir_key, "dirent-%s", dir);
    bool res = cf_mcd_exists(dir_key);
    free(dir_key);
    return res;
}
