/*
$Id: child.c,v 1.2 2017/10/04 07:55:28 o1-hester Exp o1-hester $
$Date: 2017/10/04 07:55:28 $
$Revision: 1.2 $
$Log: child.c,v $
Revision 1.2  2017/10/04 07:55:28  o1-hester
clock set up

Revision 1.1  2017/10/04 07:44:46  o1-hester
Initial revision

$Author: o1-hester $
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "ipchelper.h"
#include "sighandler.h"
#include "filehelper.h"
#define PALIN "./palin.out"
#define NOPALIN "./nopalin.out"

int semid;
struct sembuf mutex[2];

oss_clock_t calcendtime(oss_clock_t* clock, int quantum);
int initsighandler();

// explain
int
main (int argc, char** argv)
{
	
	// get key from file
	key_t mkey, skey, shmkey;
	if (((mkey = ftok(KEYPATH, MSG_ID)) == -1) ||
		((skey = ftok(KEYPATH, SEM_ID)) == -1) ||
		((shmkey = ftok(KEYPATH, SHM_ID)) == -1)) {
		perror("Failed to retreive keys.");
		return 1;
	}
	/*************** Set up signal handler ********/
	if (initsighandler() == -1) {
		perror("Failed to set up signal handlers");
		return 1;
	}

	/***************** Set up shared memory *******/
	int shmid;
	oss_clock_t* clock;
	if ((shmid = getclockshmidreadonly(shmkey)) == -1) {
		perror("Failed to retreive shared memvory segment.");
		return 1;
	}	
	if ((clock = attachshmclock(shmid)) == (void*)-1) {
		perror("Failed to attack shared memory.");	
		return 1;
	}

	/***************** Set up semaphore ************/
	//int semid;
	if ((semid = semget(skey, 2, PERM)) == -1) {
		if (errno == EIDRM) {
			perror("I cannot go on like this :(\n");
			return 1;
		}
		perror("Failed to set up semaphore.");
		return 1;
	}
	//struct sembuf mutex[2];
	struct sembuf signalDad[2];
	setsembuf(mutex, 0, -1, 0);
	setsembuf(mutex+1, 0, 1, 0);
	setsembuf(signalDad, 1, -1, 0);

	/**************** Set up message queue *********/
	//mymsg_t mymsg;	
	int msgid;
	if ((msgid = msgget(mkey, PERM)) == -1) {
		if (errno == EIDRM) {
			perror("I cannot go on like this :(\n");
			return 1;
		}
		perror("Failed to create message queue.");
		return 1;
	}

	
	// seed random 
	struct timespec tm;
	clock_gettime(CLOCK_MONOTONIC, &tm);
	srand((unsigned)(tm.tv_sec ^ tm.tv_nsec ^ (tm.tv_nsec >> 31)));

	// initialize all the following stuff
	long pid = (long)getpid();
	int quantum = rand() % 1000000 + 1;
	oss_clock_t endtime = calcendtime(clock, quantum);
	int expiry = 0; // flag for done
	fprintf(stderr, "%ld endtime:%d, %d\n", pid, endtime.sec, endtime.nsec);
	
	// begin looping over critical section
	// where each child checks the clock in shmem
	while (!expiry)
	{	
	/************ Entry section ***************/	
	//fprintf(stderr, "im in entry section\n.");
	// wait until your turn
	if (semop(semid, mutex, 1) == -1) {
		if (errno == EIDRM) {
			fprintf(stderr, "child interrupted.\n");
			return 1;
		}
		perror("Failed to lock semid.");
		return 1;	
	}
	// Block all signals during critical section
	sigset_t newmask, oldmask; 	
	if ((sigfillset(&newmask) == -1) ||
		(sigprocmask(SIG_BLOCK, &newmask, &oldmask) == -1)) {
		perror("Failed setting signal mask.");
		return 1;
	} 
	/************ Critical section ***********/
	//fprintf(stderr, "child %ld in crit sec @ %s", pid, tme); 
	if ((endtime.sec <= clock->sec && endtime.nsec <= clock->nsec)
		|| (endtime.sec < clock->sec)) {
		// child's time is up
		//fprintf(stderr, "child %ld expired.\n", pid); 
		expiry = 1;
		sendmessage(msgid, pid, endtime, clock);
	}
	//fprintf(stderr, "%d\t%d\n", clock->sec, clock->nsec);
	/*********** Exit section **************/
	// unlock file
	if (semop(semid, mutex+1, 1) == -1) { 		
		if (errno == EINVAL) {
			char* msg = "finished critical section after signal";
			fprintf(stderr, "child %ld %s\n", pid, msg);
			return 1;
		}
		perror("Failed to unlock semid.");
		return 1;
	}
	// Unblock signals after critical sections
	if ((sigprocmask(SIG_BLOCK, &newmask, &oldmask) == -1)) {
		perror("Failed setting signal mask.");
		return 1;
	} 
	} // end while

 	if (errno != 0) {
		perror("palin uncaught error:");
		return 1;
	}
	return 0;
}

// calculates the time at which child terminates
oss_clock_t
calcendtime(oss_clock_t* clock, int quantum)
{
	int s = clock->sec;
	int ns = clock->nsec;
	ns += quantum;
	if (ns >= 1000000000) {
		s++;
		ns = ns % 1000000000;
	}
	oss_clock_t endtime;
	endtime.sec = s;
	endtime.nsec = ns;
	return endtime;	
}

// initialize signal handler, return -1 on error
int
initsighandler()
{
	struct sigaction act = {{0}};
	act.sa_handler = catchchildintr;
	act.sa_flags = 0;
	if ((sigemptyset(&act.sa_mask) == -1) ||
	    (sigaction(SIGINT, &act, NULL) == -1) ||
	    (sigaction(SIGALRM, &act, NULL) == -1)) {
		return -1;
	}	
	return 0;
}
