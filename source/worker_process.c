// This program will run when a worker process is called

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#include "sync_info_mem_store.h"


void report(char*, char*,const char*, int, int, const char* );
int copy_files(char*, char*);
void clean_directory(char*);

/* ====================================================== main function of the worker ============================================= */

int main(int argc, char *argv[]) {

    /* ----------- checking if all the correct arguments were passed in the worker ---------- */
    if (argc != 5) {
        printf("Number of arguments not correct!\n");
        exit(EXIT_FAILURE);
    }

    /* ---------- checking the successful and unsuccessful attempts to copy files -------------- */
    int copy_success = 0;
    int copy_failure = 0;

    /* ------------------------------- retrieve the directories and open the source directory ----------------------------- */
    DIR *dir;
    struct dirent *entry;
    char *source_dir = argv[1]; 
    char *dest_dir = argv[2];
    const char *filename = argv[3];
    const char *operation = argv[4];
    
    if (filename == NULL || operation == NULL) {
        fprintf(stderr, "Invalid arguments to worker\n");
        exit(EXIT_FAILURE);
    }
    dir = opendir(source_dir);
    if (dir == NULL) {
        perror("error with opendir\n");
        exit(EXIT_FAILURE);
    }

    // create the target directory if it does not exist
    mkdir(dest_dir, 0755);

    /* --------------------------------------------------------------------------------------------------------------------- */

    /* ---------------------------------------------------- OPERATION FULL ------------------------------------------------- */
    if (strcmp(operation, "FULL") == 0){
        clean_directory(dest_dir);

        while ((entry = readdir(dir)) != NULL) {
            if ((strcmp(entry->d_name, ".") == 0) || (strcmp(entry->d_name, "..") == 0)){
                continue;;
            }

            // path of the source file ( existing in the source directory)
            char src_path[512];
            snprintf(src_path, sizeof(src_path), "%s/%s", source_dir, entry->d_name);
            
            // path of the destination file ( existing in the destination directory)
            char dst_path[512];
            snprintf(dst_path, sizeof(dst_path), "%s/%s", dest_dir, entry->d_name);


            if (copy_files(src_path, dst_path)==0) {
                copy_success += 1;
            }else{
                copy_failure += 1;
            }
        }
    }
    /* -------------------------------------------------------------------------------------------------------------------- */

    /* -------------------------------------------------- OPERATION ADDED ------------------------------------------------- */
    if (strcmp(operation, "ADDED") == 0){
        while ((entry = readdir(dir)) != NULL) {
            if ( strcmp(entry->d_name, filename) == 0) {
                // we find that one file that created and we copy only this in the target directory
                // path of the source file in the source directory
                char src_path[512];
                snprintf(src_path, sizeof(src_path), "%s/%s", source_dir, entry->d_name);
                
                // path of the destination file ( existing in the destination directory)
                char dst_path[512];
                snprintf(dst_path, sizeof(dst_path), "%s/%s", dest_dir, entry->d_name);


                if (copy_files(src_path, dst_path)==0) {
                    copy_success += 1;
                }else{
                    copy_failure += 1;
                }
            }
        }
    }
    /* --------------------------------------------------------------------------------------------------------------------- */


    /* --------------------------------------------- OPERATION MODIFIED ---------------------------------------------------- */
    if (strcmp(operation, "MODIFIED") == 0){
        while ((entry = readdir(dir)) != NULL) {
            if ( strcmp(entry->d_name, filename) == 0) {
                // we find that one file that was modified and we add it in the target directory
                // path of the source file in source directory
                char src_path[512];
                snprintf(src_path, sizeof(src_path), "%s/%s", source_dir, entry->d_name);
                
                // path of the destination file ( existing in the destination directory)
                char dst_path[512];
                snprintf(dst_path, sizeof(dst_path), "%s/%s", dest_dir, entry->d_name);

                // delete the file as it was, so we can copy the modified one
                unlink(dst_path);

                if (copy_files(src_path, dst_path)==0) {
                    copy_success += 1;
                }else{
                    copy_failure += 1;
                }
            }
        }
    }

    /* --------------------------------------------------------------------------------------------------------------------- */

    /* --------------------------------------------- OPERATION DELETED ----------------------------------------------------- */
    if (strcmp(operation, "DELETED") == 0){
        // path of the destination file ( existing in the destination directory)
        char dst_path[512];
        snprintf(dst_path, sizeof(dst_path), "%s/%s", dest_dir,filename);
        
        // check if the file exists in the destination dictionary, before attempt to delete it
        if (access(dst_path, F_OK) == 0){
            // check if the deletion was successful and we increase the appropriate variable
            if (unlink(dst_path) == 0) {
                copy_success += 1; 
            } else {
                copy_failure += 1;      
            }
        }
    }
    

    /* --------------------------------------------------------------------------------------------------------------------- */


    closedir(dir);  // Don't forget to close!

    report(source_dir, dest_dir, operation, copy_failure, copy_success, filename);

    exit(EXIT_SUCCESS);

}

/* ==================================================================================================================================== */



/* ============================= functions for the operations of worker ==============================*/

void report(char *source, char *destination,const char* operation, int fail, int success, const char* filename){
    // preparing the report in the acceptable format so the manager can write it on the manager log 
    // without having to parse it first. Less work for the manager.
    int pid = getpid();
    
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);
    
    char* result;
    if (fail == 0)
        result = "SUCCESS";
    else if (success == 0)
        result = "ERROR";
        
    else
        result = "PARTIAL";
    
    char details[100];
    if (strcmp(filename, "ALL") == 0)
        snprintf(details, sizeof(details), "%d files copied, %d failed", success, fail);
    else 
        snprintf(details, sizeof(details), "%s", filename);

    char log_entry[2048];
    snprintf(log_entry, sizeof(log_entry),
        "[%s] [%s] [%s] [%d] [%s] [%s] [%s]\n",
        timestamp,
        source,
        destination,
        pid,
        operation,
        result,
        details);


    printf("%s",log_entry);
    fflush(stdout);

}


int copy_files(char *source_file, char *dest_file){
    /* ----------------- open the files: one to read from and one to write to ----------------------  */
    int source_fd = open(source_file, O_RDONLY);
    if (source_fd < 0) {
        fprintf(stderr, "Error opening source file %s: %s\n", source_file, strerror(errno));
        return -1;
    }

    int dest_fd = open(dest_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dest_fd < 0) {
        fprintf(stderr,"Error opening source file %s: %s\n", source_file, strerror(errno));
        return -1;
    }


    /* ------------------- read from the source file and write eveything to destination file ----------------*/
    char buffer[1024];
    ssize_t bytes_read;
    while ((bytes_read = read(source_fd, buffer, 1024)) > 0) {
        if (write(dest_fd, buffer, bytes_read) < 0 ){
            fprintf(stderr,"Error at writing bytes from source to destination: %s\n",strerror(errno));
            return -1;
        }
    }

    /* ------------------- close the files ------------------------- */
    close(source_fd);
    close(dest_fd);

    return 0;
}


void clean_directory(char *dest_dir){
    // Clean up all files in target directory
    DIR *target = opendir(dest_dir);
    if (target) {
        struct dirent *entry;
        while ((entry = readdir(target)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            char target_file_path[512];
            snprintf(target_file_path, sizeof(target_file_path), "%s/%s", dest_dir, entry->d_name);
            unlink(target_file_path);  // delete this file from the dictionary
        }
        closedir(target);
    }
}

    
    /* ==============================================================================================================*/