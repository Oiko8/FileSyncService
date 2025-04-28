// this file is the implementation of a linked list for the sync info -> sync_info_mem_store.h
// this file contains our struct and names of the function we will use 

#include <time.h>

#define PATH_LENGTH 256


/* ================================================= useful structs =================================================*/
typedef struct{
    char source_dir[PATH_LENGTH];
    char target_dir[PATH_LENGTH];
    int active;
    time_t last_sync_time;
    int error_count;
    int is_syncing;
    int fd_watch;
    int watch_d;
} syncInfo;

typedef struct NodeTag Node;
typedef struct NodeTag {
    syncInfo item;
    struct NodeTag *Link;
} Node;

// keeping a struct for the event that happened and noticed from inotify, so i can keep the filename as well
typedef struct {
    int event_type; // 2=ADDED, 3=MODIFIED, 4=DELETED
    char filename[PATH_LENGTH];  // filename
} FileEvent;

/* ===================================================================================================================*/

/* =============================================== Functions Declaration =============================================*/

void add_sync(const char *src, const char *tgt);
syncInfo* find_sync_by_source(const char *src);
void deactivate_sync(const char *src);
void print_status(const char *src);
void update_last_sync_time(const char* src);
void increment_error_count(const char* src);
void set_syncing_flag(const char* src, int flag);
void remove_sync_by_source(const char* source);
void free_sync_list();
void cancel_inotify(const char* src);
int start_monitoring(Node*);
void stop_monitoring(syncInfo*);
FileEvent read_inotify_events(syncInfo*);
syncInfo* get_currently_syncing();
Node* get_head();

/* ===================================================================================================================*/