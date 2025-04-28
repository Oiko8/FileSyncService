// this is the interface for the queue of commands

#define PATH_LENGTH 256

typedef struct {
    // keeping the infos we need to run a command
    char source[PATH_LENGTH];
    char target[PATH_LENGTH];
    char filename[PATH_LENGTH]; 
    char operation[16];         
} commandItem;


typedef struct commandTag {
    commandItem command;
    struct commandTag* next;
} commandNode;

int is_queue_empty();
void add_command_in_queue(char*, char*, char*, char*);
commandItem* get_next_command();
