// this is the interface of the sync info

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/inotify.h>
#include "sync_info_mem_store.h"


typedef Node *NodePtr;

// Head pointer to the list
static NodePtr head = NULL;


NodePtr get_head(){
    return head;
} 

void add_sync(const char *src, const char *tgt){
    NodePtr new_node = malloc(sizeof(Node));
    if (!new_node) {
        perror("Malloc failed");
        exit(EXIT_FAILURE);
    }

    strncpy(new_node->item.source_dir, src, PATH_LENGTH);
    strncpy(new_node->item.target_dir, tgt, PATH_LENGTH);
    new_node->item.last_sync_time = 0;
    new_node->item.active = 1;
    new_node->item.error_count = 0;
    new_node->item.is_syncing = 0;
    new_node->Link = NULL;

    start_monitoring(new_node);

    if (!head) {
        head = new_node;
    } else {
        NodePtr current = head;
        while (current->Link) {
            current = current->Link;
        }
        current->Link = new_node;
    }
}

syncInfo* find_sync_by_source(const char *src){
    NodePtr current = head;
    while (current != NULL) {
        if (strcmp(current->item.source_dir, src) == 0) {
            return &current->item;
        }
        current = current->Link;
    }
    return NULL; // Not found
}

void deactivate_sync(const char *src){
    syncInfo *directory_info = find_sync_by_source(src);
    if (directory_info){
        directory_info->active = 0;
    }

}

void print_status(const char *src){
    syncInfo* info = find_sync_by_source(src);
    if (!info) {
        printf("No sync found for: %s\n", src);
        return;
    }

    printf("Source: %s\n", info->source_dir);
    printf("Target: %s\n", info->target_dir);
    printf("%s\n", info->active ? "Active" : "Inactive");
    printf("Last Sync: %s", info->last_sync_time ? ctime(&info->last_sync_time) : "Never\n");
    printf("Errors: %d\n", info->error_count);
    printf("Syncing Now: %s\n", info->is_syncing ? "Yes" : "No");
}

void update_last_sync_time(const char* src) {
    syncInfo* info = find_sync_by_source(src);
    if (info) {
        info->last_sync_time = time(NULL);
    }
}

void increment_error_count(const char* src) {
    syncInfo* info = find_sync_by_source(src);
    if (info) {
        info->error_count++;
    }
}

void set_syncing_flag(const char* src, int flag) {
    syncInfo* info = find_sync_by_source(src);
    if (info) {
        info->is_syncing = flag;
    }
}

syncInfo* get_currently_syncing() {
    NodePtr current = head;
    while (current) {
        if (current->item.is_syncing)
            return &current->item;
        current = current->Link;
    }
    return NULL;
}


void remove_sync_by_source(const char* source) {
    NodePtr current = head;
    NodePtr prev = NULL;

    while (current) {
        if (strcmp(current->item.source_dir, source) == 0) {
            if (prev) {
                prev->Link = current->Link;
            } else {
                head = current->Link;
            }

            stop_monitoring(&current->item); // remove inotify watch
            free(current);
            return;
        }

        prev = current;
        current = current->Link;
    }
}


void free_sync_list() {
    NodePtr current = head;
    while (current) {
        NodePtr temp = current;
        current = current->Link;
        free(temp);
    }
    head = NULL;
}


/* ==================================================== functions for using Inotify ============================================== */


// I update the fd and the wd in the item of the nodes. The items are structs that have all the infos for the duos of source-destinatino dir
int start_monitoring(NodePtr node) {
    char *src = node->item.source_dir; 
    int fd = inotify_init1(IN_NONBLOCK);
    node->item.fd_watch = fd;
    if (fd < 0) {
        perror("inotify_init");
        return -1;
    }

    int wd = inotify_add_watch(fd, src, IN_CREATE | IN_DELETE | IN_MODIFY);
    node->item.watch_d = wd;
    if (wd < 0) {
        perror("inotify_add_watch");
        close(fd);
        return -1;
    }

    return 0;  // on successful start of monitoring return 0 
}

// remove the inotify from a duo of source-destinatino directories
void cancel_inotify(const char* src){
    remove_sync_by_source(src);
}


void stop_monitoring(syncInfo* item) {
    int fd = item->fd_watch;
    int wd = item->watch_d;
    inotify_rm_watch(fd, wd); 
    close(fd);
}


// Read and print events from inotify descriptor in syncInfo
FileEvent read_inotify_events(syncInfo* info) {
    FileEvent result = {0, ""};

    if (!info || info->fd_watch < 0) return result;

    char buffer[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
    ssize_t len;

    int event_found = 0;

    while ((len = read(info->fd_watch, buffer, sizeof(buffer))) > 0) {
        for (char *ptr = buffer; ptr < buffer + len; ) {
            struct inotify_event *event = (struct inotify_event *) ptr;

            // ignore specific temporary files
            if (event->len > 0) {
                const char* name = event->name;
                size_t nlen = strlen(name);

                if (
                    strstr(name, ".swp") != NULL ||
                    strstr(name, ".swx") != NULL ||
                    strstr(name, ".tmp") != NULL ||
                    (nlen > 0 && name[nlen - 1] == '~')
                ) {
                    ptr += sizeof(struct inotify_event) + event->len;
                    continue; 
                }
            }

            event_found = 1; 
            printf("[inotify] Detected in %s ",info->source_dir);

            // const char *operation = NULL;
            // we change the # of the return value according to the event that happened
            if (event->mask & IN_CREATE) {
                printf("CREATE ");
                // operation = "ADDED";
                result.event_type = 2;
            } else if (event->mask & IN_MODIFY) {
                printf("MODIFY ");
                // operation = "MODIFIED";
                result.event_type = 3;
            } else if (event->mask & IN_DELETE) {
                printf("DELETE ");
                // operation = "DELETED";
                result.event_type = 4;
            } else {
                continue;
            }

            if (event->len > 0){
                printf("on file: %s\n", event->name);
                strncpy(result.filename, event->name, PATH_LENGTH);
            }

            // here i must trigger the worker again, to make changes

            ptr += sizeof(struct inotify_event) + event->len;
        }
    }
    return result;
}

/* =============================================================================================================================== */


