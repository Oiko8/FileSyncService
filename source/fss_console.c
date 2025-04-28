// program that works as an interface between a user and the fss_manager

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

void console_logfile_report(int , char* );
int fd_in, fd_out;

/* -------- signal in case the fss_console close without shutdown ------------ */
void handle_sigint(int sig) {
    write(fd_in, "shutdown", strlen("shutdown"));
    exit(0);
}


/* ====================================================== main function ================================================ */
int main(int argc, char *argv[]){

    // in case the fss_console close unexpected close the fss_manager
    signal(SIGINT, handle_sigint);

    if (argc != 3 || strcmp(argv[1], "-l") != 0) {
        fprintf(stderr, "Problem with the arguments: -l <console_logfile>\n");
        exit(EXIT_FAILURE);
    }

    const char *logfilename = argv[2];

    // open the console_logfile, if there is not any it is created 
    int log_fd = open(logfilename, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (log_fd < 0) {
        perror("open console_logfile");
        exit(EXIT_FAILURE);
    }

    // open the fifos (named pipes)
    fd_in = open("fss_in", O_WRONLY);
    if (fd_in == -1) {
        perror("Problem occured while opening the fss_in\n");
        exit(EXIT_FAILURE);
    }

    fd_out = open("fss_out", O_RDONLY);
    if (fd_out == -1){
        perror("Problem occured while opening the fss_out\n");
        exit(EXIT_FAILURE);
    }

    printf("Console is ready and waiting for commands\n");
    char command_buffer[128];
    char results_buffer[256];
    while (1)
    {   
        // write the commands and send them to the fss_manager through the fss_in pipe
        fflush(stdout);

        // printf(">");
        if (!fgets(command_buffer, sizeof(command_buffer), stdin)) break;
        command_buffer[strcspn(command_buffer, "\n")] = '\0'; // delete the new line

        // if the user just press Enter and nothing else, we continue
        if (command_buffer[0] == '\0') {
            fprintf(stderr,"Please write a command\n");
            continue; 
        }
        
        // copy all the command so the copy can be used by the function to print the right format at the console_logfile
        char command_copy[128];
        strncpy(command_copy, command_buffer, sizeof(command_copy));
        console_logfile_report(log_fd, command_copy);

         
        if (write(fd_in, command_buffer, strlen(command_buffer)) == -1){
            perror("Problem with writing in fss_in\n");
            exit(EXIT_FAILURE);
        }


        // read the feedback from the manager through the fss_out
        ssize_t results = read(fd_out, results_buffer, sizeof(results_buffer));
        if (results == -1){
            perror("Problem with reading from the fss_out\n");
            exit(EXIT_FAILURE);
        }
        if (results == 0){
            continue;
        }
        results_buffer[results] = '\0'; 

        if (results > 0) {
            results_buffer[results] = '\0';
        
            // Εκτύπωση στην οθόνη
            printf("%s", results_buffer);
        
        }

        if (strcmp(command_buffer, "shutdown") == 0) {
            // end the loop if the last command was "shutdown"
            break;
        }

    }
    
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);

    printf("[%s] Manager shutdown complete\n", timestamp);

    close(fd_in);
    close(fd_out);
    close(log_fd);

    exit(0);
}

/* ==================================================================================================================== */


/* ================================================= extra functions ==================================================*/

void console_logfile_report(int log_fd, char* command){
    // timestamp
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);

    char log_entry[1024];

    // only for the command add
    if (strncmp(command, "add", 3) == 0) {
        char *source = strtok(command + 4, " ");
        char *target = strtok(NULL, " ");
        if (source && target) {
            snprintf(log_entry, sizeof(log_entry), "[%s] Command add %s -> %s\n", timestamp, source, target);
        }
    } else {
        snprintf(log_entry, sizeof(log_entry), "[%s] Command %s\n", timestamp, command);
    }

    write(log_fd, log_entry, strlen(log_entry));
}