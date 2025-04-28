// this is the implementation of a queue for the commands that have to wait for a worker to be free
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "queue_implementation.h"


static commandNode* head = NULL;


// check if our queue is empty
int is_queue_empty() {
    if (head==NULL)
        return 1;
    else
        return 0;
}

// add the new command at the end of the line
void add_command_in_queue(char* source, char* target, char* filename, char* operation){
    commandNode* new_command = malloc(sizeof(commandNode));
    if (!new_command) {
        perror("Malloc failed");
        exit(EXIT_FAILURE);
    }

    strcpy(new_command->command.source, source);
    strcpy(new_command->command.target, target);
    strcpy(new_command->command.filename, filename);
    strcpy(new_command->command.operation, operation);

    if (!head) {
        head = new_command;
    }
    else {
        commandNode* current = head;
        while (current->next) {
            current = current->next;
        }
        current->next = new_command;
    }
    
}


// returns the next in line command, so the worker can execute it
commandItem* get_next_command(){
    if (!head) return NULL;

    commandItem* next_command = malloc(sizeof(commandItem));
    if (!next_command) {
        perror("Malloc failed");
        exit(EXIT_FAILURE);
    }

    // copy the data of the next command
    *next_command = head->command;

    // free the previous head node
    commandNode* temp = head;
    head = head->next;
    free(temp);

    return next_command;
}
