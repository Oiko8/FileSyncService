# In order to execute this "Makefile" just type "make"-A. Ntoulas (antoulas@di.uoa.gr)

OBJS = fss_manager.o fss_console.o worker_process.o sync_info_mem_store.o queue_implementation.o
SOURCE = fss_manager.c fss_console.c worker_process.c sync_info_mem_store.c queue_implementation.c
# HEADER =  
OUT = myexe
CC = gcc
FLAGS = -g -c

#-g for debugging mode ;-c for objects file creation
all: fss_manager fss_console worker_process

# fss_manager: fss_manager.o
# 	$(CC) -g -o $@ fss_manager.o

fss_console: fss_console.o
	$(CC) -g -o $@ fss_console.o

worker_process: worker_process.o
	$(CC) -g -o $@ worker_process.o

fss_manager: fss_manager.o sync_info_mem_store.o queue_implementation.o
	$(CC) -g -o $@ fss_manager.o sync_info_mem_store.o queue_implementation.o


# create/compile the individual files >>separately<<
fss_manager.o: fss_manager.c
	$(CC) $(FLAGS) fss_manager.c

fss_console.o: fss_console.c
	$(CC) $(FLAGS) fss_console.c

worker_process.o: worker_process.c
	$(CC) $(FLAGS) worker_process.c

sync_info_mem_store.o: sync_info_mem_store.c sync_info_mem_store.h
	$(CC) $(FLAGS) sync_info_mem_store.c

queue_implementation.o: queue_implementation.c queue_implementation.h
	$(CC) $(FLAGS) queue_implementation.c


# clean house
clean:
	rm -f $(OBJS) fss_manager fss_console worker_process
# do a bit of accounting
count:
	wc $(SOURCE) 