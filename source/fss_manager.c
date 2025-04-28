#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include "sync_info_mem_store.h"
#include "queue_implementation.h"

#define IS_INDIR 0x40000000 //mask to chech if a file or a folder is modified

int active_workers = 0;
void read_config_file(const char*, const char*, int);
void call_worker(char* ,char* ,char* ,char* ,const char*, int);
void update_logfile_file(char*,const char*);

/* ===================================================== main function ======================================================== */

int main(int argc, char* argv[]){
    if (argc != 7 || strcmp(argv[1], "-l") != 0 || strcmp(argv[3], "-c") != 0 || strcmp(argv[5], "-n") != 0) {
        fprintf(stderr, "Problem with the format: %s -l <logfile> -c <config_file> -n <worker_limit>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    // getting the args from the command line
    const char *logfile = argv[2];
    const char *config_file = argv[4];
    int worker_limit = atoi(argv[6]);
    if (worker_limit <= 0) worker_limit = 5;

	// First we create our named pipes 
    if (mkfifo("fss_in", 0777) == -1){
        if (errno != EEXIST){
            printf("Fifo could not be made.\n");
            exit(EXIT_FAILURE);
        }
    }
    if (mkfifo("fss_out", 0777) == -1){
        if (errno != EEXIST){
            printf("Fifo could not be made.\n");
            exit(EXIT_FAILURE);
        }
    }

    // read and sychronize the pairs of directories that exist in the config_file
    read_config_file(config_file, logfile, worker_limit);

    /* ----------------------------- open the fifos (named pipes) ------------------------------------------- */
    int fd_in = open("fss_in", O_RDONLY | O_NONBLOCK);
    if (fd_in == -1) {
        perror("Problem occured while opening the fss_in\n");
        exit(EXIT_FAILURE);
    }

    int fd_out = open("fss_out", O_WRONLY);
    if (fd_out == -1){
        perror("Problem occured while opening the fss_out\n");
        exit(EXIT_FAILURE);
    }


    /* -------------------------- read the changes of the directories through inotify ----------------------- */
    /* -------------------------------------------------- or ------------------------------------------------ */    
    /* ---------------------------------- waiting for commands from console  -------------------------------- */    
    /* ---------------------------------- calling worker if anything occured -------------------------------- */ 
    char filename[256];
    char command_buffer[128];
    FileEvent event_infos;
    while (1) {
        /* ----------------------------- inotify events ------------------------ */
        Node* current = get_head();
        while (current) {
            syncInfo* info = &current->item;
            event_infos = read_inotify_events(info);
            strcpy(filename, event_infos.filename);
            if (event_infos.event_type!=0){
                if (event_infos.event_type == 2)
                    call_worker(info->source_dir, info->target_dir, filename, "ADDED", logfile, worker_limit);
                else if (event_infos.event_type == 3)
                    call_worker(info->source_dir, info->target_dir, filename, "MODIFIED", logfile, worker_limit);
                else if (event_infos.event_type == 4)
                    call_worker(info->source_dir, info->target_dir, filename, "DELETED", logfile, worker_limit);
            }
            current = current->Link;
        }
        /* --------------------------------------------------------------------- */

        /* ------------------------- console commands -------------------------- */
        
        // getting the command right
        ssize_t read_command = read(fd_in, command_buffer, sizeof(command_buffer));
        if (read_command == -1){
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            } else {
                perror("Problem with reading from the fss_in\n");
                exit(EXIT_FAILURE);
            }
        }
        else if (read_command > 0) {
            command_buffer[read_command] = '\0'; 
            

            // parsing the command
            char *command = strtok(command_buffer, " \n");
            if (!command) continue;
            
            
            // apply each command ( ADD , STATUS , CANCEL , SYNC , SHUTDOWN )
            if (strcmp(command, "add") == 0) {
                char *source = strtok(NULL, " \n");
                char *target = strtok(NULL, " \n");
                char message[128];
                time_t now = time(NULL);
                struct tm *t = localtime(&now);
                char timestamp[64];
                strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);
                if (source && target) {
                    if (find_sync_by_source(source)!=NULL){
                        snprintf(message, sizeof(message),"[%s] Already in queue: %s\n", timestamp, source);
                        write(fd_out, message, strlen(message));
                    }
                    else {
                        add_sync(source, target);
                        call_worker(source, target, "ALL", "FULL", logfile, worker_limit);
                        snprintf(message, sizeof(message),"[%s] Added directory: %s -> %s\n", timestamp, source, target);
                        write(fd_out, message, strlen(message));
                    }
                } else {
                    snprintf(message, sizeof(message),"[%s] Failed to add: %s\n", timestamp, source);
                    write(fd_out, message, strlen(message));
                }
            }
            else if (strcmp(command, "cancel") == 0) {
                char *source = strtok(NULL, " \n");
                char message[128];
                time_t now = time(NULL);
                struct tm *t = localtime(&now);
                char timestamp[64];
                strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);
                if (source) {
                    syncInfo* info = find_sync_by_source(source);
                    if (info) {
                        cancel_inotify(source);
                        snprintf(message, sizeof(message),"[%s] Monitoring stop for %s\n", timestamp, source);
                        write(fd_out, message, strlen(message));
                    } else {
                        snprintf(message, sizeof(message),"[%s] Directory not monitored: %s\n", timestamp, source);
                        write(fd_out, message, strlen(message));
                    }
                } else {
                    write(fd_out, "Invalid cancel command.\n", 25);
                }

            }
            else if (strcmp(command, "sync") == 0) {
                char *source = strtok(NULL, " \n");
                char message[512];
                if (source){
                    time_t now = time(NULL);
                    struct tm *t = localtime(&now);
                    char timestamp[64];
                    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);
                    syncInfo* info = find_sync_by_source(source);
                    if (info){
                        char* target = info->target_dir;
                        call_worker(source, target, "ALL", "FULL", logfile, worker_limit);
                        snprintf(message, sizeof(message),"[%s] Syncing directory: %s -> %s\n", timestamp, source, target);
                        write(fd_out, message, strlen(message));
                    } else {
                        snprintf(message, sizeof(message),"[%s] Failed to sync %s\n", timestamp, source);
                        write(fd_out, message, strlen(message));
                    }
                } else {
                    write(fd_out, "Invalid sync command.\n", 23);
                }   
            }
            else if (strcmp(command, "status") == 0){
                char *source = strtok(NULL, " \n");
                time_t now = time(NULL);
                struct tm *t = localtime(&now);
                char timestamp[64];
                char message[256];
                strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);
                if (source) {
                    syncInfo* info = find_sync_by_source(source);
                    if (info) {
                        char time_str[64] = "N/A";

                        if (info->last_sync_time != 0) {
                            struct tm *last_t = localtime(&info->last_sync_time);
                            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", last_t);
                        }

                        snprintf(message, sizeof(message),
                            "[%s] Status for %s:\nActive: %s\nErrors: %d\nLast Sync: %s\n",
                            timestamp,
                            source,
                            info->active ? "YES" : "NO",
                            info->error_count,
                            time_str);

                        write(fd_out, message, strlen(message));
                    } else {
                        snprintf(message, sizeof(message),"[%s] Directory not monitored %s\n", timestamp, source);
                        write(fd_out, message, strlen(message));
                    }
                } else {
                    snprintf(message, sizeof(message),"[%s] Failed to read directory name\n", timestamp);
                    write(fd_out, message, strlen(message));

                }
            }
            else if (strcmp(command, "shutdown") == 0) {
                time_t now = time(NULL);
                struct tm *t = localtime(&now);
                char timestamp[64];
                char message[256];
                strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);
                snprintf(message, sizeof(message),"[%s] Shutting down manager...\n[%s] Waiting for all active workers to finish\n",
                                                    timestamp, timestamp);
                write(fd_out, message, strlen(message));

                // wait for all the workers to finish
                
                while (wait(NULL) > 0);
                
                break;
            } else {
                char message[64];
                snprintf(message,sizeof(message), "Invalid command!\n");
                write(fd_out, message, strlen(message));
            }

        }

        if (!is_queue_empty() && active_workers < worker_limit) {
            commandItem* task = get_next_command();
            call_worker(task->source, task->target, task->filename, task->operation, logfile, worker_limit);
            free(task);
        }

        usleep(300000); // wait for the CPU to handle the changes
    }
    /* ------------------------------------------------------------------------------------------------------------------------------ */

    // closing the manager after the shutdown command
    void free_sync_list();
    unlink("fss_in");
    unlink("fss_out");
    close(fd_in);
    close(fd_out);
    exit(0);
}

/* ============================================================================================================================ */



/* ========================== extra functions: worker process and reading the config_file ===================================== */


// function that open the config file, read the line
void read_config_file(const char* cfile, const char* logfile, int worker_limit){
    size_t buffer = 20;
    FILE * fp;
    char * line = NULL;
    // char *lines[buffer];
    size_t len = 0;
    ssize_t read;

    fp = fopen(cfile, "r");
    if (fp == NULL)
        exit(EXIT_FAILURE);

    // store each line, that contains the source file and the destination file , to an array
    int i = 0;
    while ((read = getline(&line, &len, fp)) != -1) {
        // get the name of the source file and the name of the destination
        char* source_directory;
        char* dest_directory; 
        source_directory = strtok(line, " ");
        dest_directory = strtok(NULL, " \n");
        if (source_directory != NULL && dest_directory != NULL){
            call_worker(source_directory, dest_directory, "ALL", "FULL", logfile, worker_limit);
        }else {
            fprintf(stderr, "Invalid entry in config_file\n");
        }
    }
}

// fork the process and calls the worker through the child
void call_worker(char* source_directory, char* dest_directory,char* filename, char* operation, const char* logfile, int worker_limit){
    if (active_workers == worker_limit){
        add_command_in_queue(source_directory, dest_directory, filename, operation);
        return;
    }
    
    // we add a new sync for the source file, if there is not one existing already
    if (!find_sync_by_source(source_directory)) {
        add_sync(source_directory, dest_directory);
    }
    
    // the current file is undergoing sychronization
    set_syncing_flag(source_directory, 1);
    update_last_sync_time(source_directory);

    // create pipe for communication parent-child
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe failed");
        exit(EXIT_FAILURE);
    }

    // fork to create a child process that will execute the worker.c through exec()
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }


    // the child process will call worker.c through exec
    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        execlp("./worker_process", "worker_process", source_directory, dest_directory, filename, operation, NULL);
        increment_error_count(source_directory);
        perror("exec failed");
        exit(1);
    } else {
        active_workers ++;

        close(pipefd[1]);
    
        char buffer[2048];
        char exec_report[2048] = "";
        ssize_t n;
        printf("Worker report from %d:\n", pid);
        while ((n = read(pipefd[0], buffer, sizeof(buffer)-1)) > 0) {
            buffer[n] = '\0';
            printf("%s", buffer);
            strcat(exec_report, buffer);  // I write the report so i can make the right format and put it on the logfile
        }
    
        close(pipefd[0]);
        waitpid(pid, NULL, 0);

        // Write the report to the logfile_file
        update_logfile_file(exec_report, logfile);

        // synchronization is finished
        set_syncing_flag(source_directory, 0);

        active_workers --;
    }
    
}

void update_logfile_file(char* exec_report, const char* logfile){
    // opening the logfile file and write the report
    int log_fd = open(logfile, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (log_fd < 0) {
        perror("Could not open logfile.txt");
    } else {
        
        write(log_fd, exec_report, strlen(exec_report));
        close(log_fd);
    }
}

/* ============================================================================================================================ */