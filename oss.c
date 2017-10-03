/*
$Id$
$Date$
$Revision$
$Log$
$Author$
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include "ipchelper.h"
#include "sighandler.h"
#include "filehelper.h"

int initsighandlers();
int initsharedmemory(int shmid);
int initsemaphores(int semid);
void printusage();

int main (int argc, char** argv) {
	/*************** Get options  *****************/
	int maxslaves = 5;
	int timeout = 20;
	char* fname = "default.log";
	opterr = 0;
	char c;
	while ((c = getopt(argc, argv, "hs:l:t:")) != -1) {
		switch(c) {
			case 'h':
				printusage();
				return 0;
			case 's':
				maxslaves = atoi(optarg);	
				if (maxslaves < 1 || maxslaves > 19) {
					printusage();
					return 1;
				}
				break;
			case 'l':
				fname = optarg;
				break;
			case 't':
				timeout	= atoi(optarg);
				if (timeout < 1 || timeout > 1000) {
					printusage();
					return 1;
				}
				break;
			case '?':
				fprintf(stderr, "Invalid option %c\n", c);
				printusage();
				return 1;
			default:
				return 1;
		}
	}

	// get key from file
	key_t mkey, skey, shmkey;
	if (((mkey = ftok(KEYPATH, MSG_ID)) == -1) ||
		((skey = ftok(KEYPATH, SEM_ID)) == -1) ||
		((shmkey = ftok(KEYPATH, SHM_ID)) == -1)) {
		perror("Failed to retreive keys.");
		return 1;
	}

	/*************** Set up signal handler ********/
	if (initsighandlers() == -1) {
		perror("Failed to set up signal handlers.");
		return 1;
	}

	/*************** Set up shared memory *********/
	int shmid;
	oss_clock_t* clock;
	if ((shmid = getclockshmid(shmkey)) == -1) {
		perror("Failed to create shared memory segment.");
		return 1;
	}	
	if ((clock = attachshmclock(shmid)) == (void*)-1) {
		perror("Failed to attack shared memory.");	
		return 1;
	}
	clock->sec = 0;
	clock->nsec = 0;

	/*************** Set up semaphore *************/
	// semaphore contains 3 sems:
	// 0 = file i/o lock
	// 1 = master knows when done
	// 2 = for limiting to 19 children at one time
	int semid;
	if ((semid = getsemid(skey, 2)) == -1) {
		perror("Failed to create semaphore.");
		return 1;
	}	
	struct sembuf waitfordone[1];
	setsembuf(waitfordone, 1, 0, 0);
	if (initsemaphores(semid) == -1) {
		perror("Failed to init semaphores.");
		return 1;
	}
	/************** Set up message queue *********/
	// Initiate message queue	
	int msgid;
	if ((msgid = getmsgid(mkey)) == -1) {
		perror("Failed to create message queue.");
		return 1;
	}

	/****************** Spawn Children ***********/
	// make child
	// char[]'s for sending id and index to child process
	int childpid;
	char palinid[16];
	char palinindex[16];
	int j = 0;
	while (j < 9) {
		if ((childpid = fork()) <= 0) {
			// set id and msg index
			sprintf(palinid, "%d", j+1 % 20);
			sprintf(palinindex, "%d", j);
			break;
		}  
		fprintf(stderr, "... child  w/ index: %d spawned.\n", j);
		if (childpid != -1)	
			j++;
	}
	// If master fails at spawning children
	if (childpid == -1) {
		perror("Failed to create child.");
		if (removeshmem(msgid, semid, shmid, clock) == -1) {
			perror("failed to remove shared mem segment");
			return 1;
		}
	}
	/***************** Child ******************/
	if (childpid == 0) {
		sleep(1);
		// execute child with id
		execl("./child", "child", palinid, palinindex, (char*)NULL);
		perror("Exec failure.");
		return 1; // if error
	}
	/***************** Parent *****************/
	if (childpid > 0) {
		fprintf(stderr, "master: done spawning, waiting for done.\n");	
		// Waits for all children to be done
		while (wait(NULL)) {
			if (errno == ECHILD)
				break;
		}
		if (removeshmem(msgid, semid, shmid, clock) == -1) {
			// failed to remove shared mem segment
			perror("Failed to remove shared memory.");
			return 1;
		}
		fprintf(stderr, "Done. %ld\n", (long)getpgid(getpid()));
	}	
	return 0;	

}

// initialize signal handlers, return -1 on error
int initsighandlers() {
	struct sigaction newact = {{0}};
	struct sigaction timer = {{0}};
	timer.sa_handler = handletimer;
	timer.sa_flags = 0;
	newact.sa_handler = catchctrlc;
	newact.sa_flags = 0;
	// set timer handler
	if ((sigemptyset(&timer.sa_mask) == -1) ||
	    (sigaction(SIGALRM, &timer, NULL) == -1)) {
		return -1;
	}	
	// set intr handler
	if ((sigemptyset(&newact.sa_mask) == -1) ||
	    (sigaction(SIGINT, &newact, NULL) == -1)) {
		return -1;
	}	
	return 0;
}

// initialize shared memory, return -1 on error
int initsharedmemory(int shmid) {
	// set up file i/o lock
	if (initelement(shmid, 0, 1) == -1) {
		return -1;
	}
	// set up child limiter
	if (initelement(shmid, 2, 19) == -1) {
		return -1;
	}
	return 0;
}

// initialize semaphores, return -1 on error
int initsemaphores(int semid) {
	// set up file i/o lock
	if (initelement(semid, 0, 1) == -1) {
		if (semctl(semid, 0, IPC_RMID) == -1)
			return -1;
		return -1;
	}
	// set up child limiter
	if (initelement(semid, 1, 19) == -1) {
		if (semctl(semid, 0, IPC_RMID) == -1)
			return -1;
		return -1;
	}
	return 0;
}

void printusage() {
	fprintf(stderr, "Usage: ");
	fprintf(stderr, "./oss [-s x] [-l filename] ");
	fprintf(stderr, "[-t z]\n\nx: max # slave pxs [1-19]\n");
	fprintf(stderr, "filename: of log\n");
	fprintf(stderr, "z: timeout (sec) [1-1000]\n\n");
}

