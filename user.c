/*
$Id: child.c,v 1.5 2017/10/11 20:32:12 o1-hester Exp o1-hester $
$Date: 2017/10/11 20:32:12 $
$Revision: 1.5 $
$Log: child.c,v $
Revision 1.5  2017/10/11 20:32:12  o1-hester
turnin

Revision 1.4  2017/10/10 20:19:24  o1-hester
cleanup

Revision 1.3  2017/10/09 22:00:49  o1-hester
*** empty log message ***

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
#include "osstypes.h"
#include "ipchelper.h"
#include "sighandler.h"
#include "filehelper.h"
#define PALIN "./palin.out"
#define NOPALIN "./nopalin.out"

// id and operations for semaphores
int semid;
struct sembuf mutex[2];
struct sembuf msgsignal[1];

oss_clock_t calcendtime(oss_clock_t* clock, int quantum);
int sem_wait();
int sem_signal();
int initsighandler();

// Child process spawn by oss 
// Each child picks a number between 1-1000000
// It adds this to the current time
int
main (int argc, char** argv)
{
	unsigned int myid = atoi(argv[1]);
	// get keys from file
	key_t mkey, skey, shmkey;
	mkey = ftok(KEYPATH, MSG_ID);
	skey = ftok(KEYPATH, SEM_ID);
	shmkey = ftok(KEYPATH, SHM_ID);
	if ((mkey == -1) || (skey == -1) || (shmkey == -1)) {
		perror("CHILD: Failed to retreive keys.");
		return 1;
	}

	/*************** Set up signal handler ********/
	if (initsighandler() == -1) {
		perror("CHILD: Failed to set up signal handlers");
		return 1;
	}

	/***************** Set up shared memory *******/
	// Child has read-only permissions on shm
	int shmid = getclockshmidreadonly(shmkey);
	oss_clock_t* clock;
	if (shmid == -1) {
		perror("CHILD: Failed to retreive shared memvory segment.");
		return 1;
	}
	clock = attachshmclock(shmid);
	if (clock == (void*)-1) {
		perror("CHILD: Failed to attack shared memory.");	
		return 1;
	}

	/***************** Set up semaphores ***********/
	semid = semget(skey, 2, PERM);
	if (semid == -1) {
		if (errno == EIDRM) {
			perror("CHILD: Interrupted");
			return 1;
		}
		perror("CHILD: Failed to set up semaphore.");
		return 1;
	}
	// mutex for reading from shared memory
	setsembuf(mutex, 0, -1, 0);
	setsembuf(mutex+1, 0, 1, 0);
	// msgsignal for letting parent know when message is available
	setsembuf(msgsignal, 2, 1, 0);

	/**************** Set up message queue *********/
	// for writing
	int msgid = msgget(mkey, PERM);
	if (msgid == -1) {
		if (errno == EIDRM) {
			perror("CHILD: Interrupted");
			return 1;
		}
		perror("CHILD: Failed to create message queue.");
		return 1;
	}
	
	// seed random 
	struct timespec tm;
	clock_gettime(CLOCK_MONOTONIC, &tm);
	srand((unsigned)(tm.tv_sec ^ tm.tv_nsec ^ (tm.tv_nsec >> 31)));

	int expiry = 0; // flag for done
	long pid = (long)getpid();
	
	// initialize endtime 
	int quantum = rand() % 1000000 + 1;

	// get exclusive access while reading
	if (sem_wait() == -1) {
		// failed to lock shm
		return 1;
	}

	// calculate endtime, reading from shared memory
	oss_clock_t endt = calcendtime(clock, quantum);

	// let others read from shared memory
	if (sem_signal() == -1) { 		
		return 1;
	}

	// output endtime to stderr
	fprintf(stderr,"USER: %d endtime:%d,%d\n",myid,endt.sec,endt.nsec);
	
	/**************** Child loop **************/
	// begin looping over critical section
	// where each child checks the clock in shmem
	while (!expiry)
	{	
	/************ Entry section ***************/	
	// wait until your turn
	if (sem_wait() == -1) {
		// failed to lock shm
		return 1;
	}
	// Block all signals during critical section
	sigset_t newmask, oldmask; 	
	if ((sigfillset(&newmask) == -1) ||
		(sigprocmask(SIG_BLOCK, &newmask, &oldmask) == -1)) {
		perror("CHILD: Failed setting signal mask.");
		return 1;
	} 
	/************ Critical section ***********/
	if ((endt.sec <= clock->sec && endt.nsec <= clock->nsec)
		|| (endt.sec < clock->sec)) {
		// child's time is up
		expiry = 1;
		sendmessage(msgid, myid, endt, clock);
		// signal parent that a new message is available
		if (semop(semid, msgsignal, 1) == -1) {
			perror("CHILD: Failed to signal parent.");
			return 1;	
		}
	}
	/*********** Exit section **************/
	// unlock shared memory read 
	if (sem_signal() == -1) { 		
		if (errno == EINVAL) {
			char* m0 = "finished critical section after signal";
			fprintf(stderr, "CHILD: %ld %s\n", pid, m0);
			return 1;
		}
		return 1;
	}
	// Unblock signals after critical sections
	if ((sigprocmask(SIG_BLOCK, &newmask, &oldmask) == -1)) {
		perror("CHILD: Failed setting signal mask.");
		return 1;
	} 
	} // end while

 	if (errno != 0) {
		perror("CHILD: uncaught error:");
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

/***** NOTE about semaphores *****/
/* I do NOT log every time sem is waited on
 * and acquired. That would be ridiculous. Thanks. */ 

// Semaphore blocking wait, return -1 on error
int
sem_wait()
{
	if (semop(semid, mutex, 1) == -1) {
		if (errno == EIDRM) {
			perror("CHILD: Interrupted");
			return -1;
		}
		perror("CHILD: Failed to lock shared memory.");
		return -1;	
	}
	return 0;
}

// Semaphore signal, return -1 on error
int
sem_signal()
{
	if (semop(semid, mutex+1, 1) == -1) { 		
		perror("CHILD: Failed to unlock semid.");
		return -1;
	}
	return 0;
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