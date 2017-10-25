/*
$Id: user.c,v 1.1 2017/10/23 07:27:24 o1-hester Exp o1-hester $
$Date: 2017/10/23 07:27:24 $
$Revision: 1.1 $
$Log: user.c,v $
Revision 1.1  2017/10/23 07:27:24  o1-hester
Initial revision

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
//#include "filehelper.h"
#define PALIN "./palin.out"
#define NOPALIN "./nopalin.out"

// id and operations for semaphores
int semid;
struct sembuf mutex[2];
struct sembuf msgsignal[1];
struct sembuf dispatchsig[1];

oss_clock_t calcendtime(oss_clock_t clock, int quantum);
oss_clock_t calcusedtime(oss_clock_t start, oss_clock_t clock);
oss_clock_t* initsharedclock(const key_t shmkey);
pxs_cb_t* initshareddispatch(const key_t diskey);
int sem_wait();
int sem_signal();
int initsemaphores(const key_t skey);
int initsighandler();

// Child process spawn by oss 
// Each child is assigned a quantum
// It decides whether to use entire or partial quantum
// It then adds this to the current time
int
main (int argc, char** argv)
{
	unsigned int myid = atoi(argv[1]);
	// get keys from file
	key_t mkey, skey, shmkey, diskey;
	mkey = ftok(KEYPATH, MSG_ID);
	skey = ftok(KEYPATH, SEM_ID);
	shmkey = ftok(KEYPATH, SHM_ID);
	diskey = ftok(KEYPATH, SHM_ID*2);
	if (mkey == -1 || skey == -1 || shmkey == -1 || diskey == -1) {
		perror("USER: Failed to retreive keys.");
		return 1;
	}

	/*************** Set up signal handler ********/
	if (initsighandler() == -1) {
		perror("USER: Failed to set up signal handlers");
		return 1;
	}

	/***************** Set up shared memory *******/
	// Child has read-only permissions on shm
	oss_clock_t* clock = initsharedclock(shmkey);
	if (clock == (void*)-1) {
		perror("USER: Failed to attach shared memory.");	
		return 1;
	}
	pxs_cb_t* dispatch = initshareddispatch(diskey);
	if (dispatch == (void*)-1) {
		perror("USER: Failed to attach shared memory.");	
		return 1;
	}

	/***************** Set up semaphores ***********/
	if (initsemaphores(skey) == -1) {
		perror("USER: Failed to setup semaphores.");
		return 1;
	}

	/**************** Set up message queue *********/
	// for writing
	int msgid = msgget(mkey, PERM);
	if (msgid == -1) {
		if (errno == EIDRM) {
			perror("USER: Interrupted");
			return 1;
		}
		perror("USER: Failed to create message queue.");
		return 1;
	}

	/************** Wait until dispatched **********/
	oss_clock_t wait;
	wait.sec = clock->sec;
	wait.nsec = clock->nsec;
	while (myid != dispatch->proc_id)
	{
		if (clock == NULL || dispatch == NULL) {
			perror("USER: Shared memory corrupt.");
			return 1;
		}
	}
	wait = calcusedtime(wait, *clock);
	dispatch->wait_time += wait.nsec;

	// seed random 
	struct timespec tm;
	clock_gettime(CLOCK_MONOTONIC, &tm);
	srand((unsigned)(tm.tv_sec ^ tm.tv_nsec ^ (tm.tv_nsec >> 31)));

	/********* Get values from pxs cntl block ******/
	fprintf(stderr,"im on!\t%d\n", dispatch->proc_id);
	// get time quantum 
	int quantum = dispatch->quantum;
	// decide to use entire | partial quantum
	int entirequant = (int)(rand()) % 2;
	if (!entirequant) {
		int partial = (int)(rand()) % quantum;
		quantum = partial;
	}
	// calculate endtime based on quantum, reading from oss_clock 
	oss_clock_t start;
	start.sec = clock->sec;
	start.nsec = clock->nsec;
	oss_clock_t end = calcendtime(*clock, quantum);
	oss_clock_t usedtime;	// for later
	

	int expiry = 0; // flag for done
	long pid = (long)getpid();
	
	// output endtime to stderr
	fprintf(stderr,"USER:[%d] start:%d,%d\n",myid,start.sec,start.nsec);
	fprintf(stderr,"USER:[%d] priority:%d\n",myid,dispatch->priority);
	fprintf(stderr,"USER:[%d] runtime:%d\n",myid,quantum);
	fprintf(stderr,"USER:[%d] endtime:%d,%d\n",myid,end.sec,end.nsec);
	
	/**************** Child loop **************/
	// begin looping over critical section
	// where each child checks the clock in shmem
	while (expiry != 1)
	{	
	/************ Entry section ***************/	
	// wait until your turn
	// track wait time
	wait.sec = clock->sec;
	wait.nsec = clock->nsec;

	if (sem_wait() == -1) {
		// failed to lock shm
		return 1;
	}
	// need support for multiple waits
	wait = calcusedtime(wait, *clock);
	dispatch->wait_time += wait.nsec;

	// Block all signals during critical section
	sigset_t newmask, oldmask; 	
	if ((sigfillset(&newmask) == -1) ||
		(sigprocmask(SIG_BLOCK, &newmask, &oldmask) == -1)) {
		perror("CHILD: Failed setting signal mask.");
		return 1;
	} 
	/************ Critical section ***********/
	if ((end.sec <= clock->sec && end.nsec <= clock->nsec)
		|| (end.sec < clock->sec)) {
		// child's time is up
		expiry = 1;
		fprintf(stderr, "USER: Hey im done %d\n", myid);
		sendmessage(msgid, myid, end, *clock);
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

	// calculate and set used cpu time
	usedtime = calcusedtime(start, *clock);
	dispatch->used_cpu_time += usedtime.nsec;

	// detach shared memory
	if (shmdt(dispatch) == -1 || shmdt(clock) == -1) {
		perror("USER: Failed to detach shared memory.");
		return 1;
	}
	// signal parent to dispatch a new process
	if (semop(semid, dispatchsig, 1) == -1) {
		perror("USER: Failed to signal dispatcher.");
		return 1;
	}

 	if (errno != 0) {
		perror("CHILD: uncaught error:");
		return 1;
	}
	return 0;
}

// calculates the time at which child terminates
oss_clock_t
calcendtime(oss_clock_t clock, int quantum)
{
	int s = clock.sec;
	int ns = clock.nsec;
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

// calculate used cpu time
oss_clock_t
calcusedtime(oss_clock_t start, oss_clock_t clock)
{
	int s = clock.sec;
	int ns = clock.nsec;
	oss_clock_t usedtime;
	if (start.sec == s) {
		usedtime.sec = 0;
		usedtime.nsec = ns - start.nsec;
	} else {
		// add support for > 1 sec	
		usedtime.sec = 0;
		usedtime.nsec = (1000000000 - start.nsec) + clock.nsec;	
	}
	return usedtime;
}

oss_clock_t*
initsharedclock(const key_t shmkey)
{
	int shmid = getclockshmid_ro(shmkey);
	if (shmid == -1) {
		return (void*)-1;
	}
	return attachshmclock(shmid);
}

pxs_cb_t*
initshareddispatch(const key_t diskey)
{
	int shmid = getdispatchshmid(diskey);
	if (shmid == -1) {
		return (void*)-1;
	}
	return attachshmdispatch(shmid);
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

// initialize semaphore operations
int
initsemaphores(const key_t skey)
{
	semid = semget(skey, 4, PERM);
	if (semid == -1) {
		if (errno == EIDRM) {
			perror("CHILD: Interrupted");
			return -1;
		}
		perror("CHILD: Failed to set up semaphore.");
		return -1;
	}
	// mutex for reading from shared memory
	setsembuf(mutex, 0, -1, 0);
	setsembuf(mutex+1, 0, 1, 0);
	// msgsignal for letting parent know when message is available
	setsembuf(msgsignal, 2, 1, 0);
	// for signaling parent to dispatch another guy
	setsembuf(dispatchsig, 3, 1, 0);
	return 0;
}
// initialize signal handler, return -1 on error
int
initsighandler()
{
	struct sigaction act = {{0}};
	act.sa_handler = catchuserintr;
	act.sa_flags = 0;
	if ((sigemptyset(&act.sa_mask) == -1) ||
	    (sigaction(SIGINT, &act, NULL) == -1) ||
	    (sigaction(SIGALRM, &act, NULL) == -1) ||
	    (sigaction(SIGUSR1, &act, NULL) == -1)) {
		return -1;
	}	
	return 0;
}
