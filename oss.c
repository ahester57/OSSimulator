/*
$Id: oss.c,v 1.3 2017/10/09 22:00:42 o1-hester Exp o1-hester $
$Date: 2017/10/09 22:00:42 $
$Revision: 1.3 $
$Log: oss.c,v $
Revision 1.3  2017/10/09 22:00:42  o1-hester
threading

Revision 1.2  2017/10/04 07:55:18  o1-hester
clock set up

Revision 1.1  2017/10/04 07:45:30  o1-hester
Initial revision

$Author: o1-hester $
*/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <pthread.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include "ipchelper.h"
#include "sighandler.h"
#include "filehelper.h"
#define FILEPERMS (O_WRONLY | O_TRUNC | O_CREAT)

// id and operations for semaphores
int semid;
struct sembuf mutex[2];

// clock
void updateclock(oss_clock_t* clock);
void* systemclock(void* args);
// spawning children
long spawnchild();
void* logchildcreate(void* args);
// message listener
void* msgthread(void* args);
// initiators
int initsighandlers();
oss_clock_t* initsharedmemory(int shmid);
int initsemaphores(int semid);
//print usage/error
void printusage();
void printopterr(char optopt);

// oss main. spawns a number of children that do some stuff
int
main (int argc, char** argv)
{
	/*************** Get options  *****************/
	// use getopt_long for long-named options
	// --quiet/silent
	// --help/version
	// --timeout/FILENAME
	// fatal error on malloc return NULL
	int maxslaves = 5;
	int timeout = 20;
	char* fname = "default.log";
	opterr = 0;
	char c;
	while ((c = getopt(argc, argv, "hs:l:t:")) != -1)
	{
		switch(c)
		{
		case 'h':
			printusage();
			return 0;
		case 's':
			maxslaves = atoi(optarg);	
			// increase this when you implement child limiter 
			if (maxslaves < 1 || maxslaves > 100) {
				char* m0 = "x must be from 1-100.";
				fprintf(stderr, "\t%s\n", m0);
				printusage();
				return 1;
			}
			break;
		case 'l':
			if (isalpha(optarg[0])) {
				fname = optarg;
				break;
			}
			char* m0 = "Filename must start with a letter.";
			fprintf(stderr, "\t%s\n", m0);
			printusage();
			return 1;	
		case 't':
			timeout	= atoi(optarg);
			if (timeout < 1 || timeout > 1000) {
				char* m0 = "z must be from 1-1000.";
				fprintf(stderr,"\t%s\n", m0);
				printusage();
				return 1;
			}
			break;
		case '?':
			printopterr(optopt);
			printusage();
		return 1;
		default:
			return 1;
		}
	}

	// get key from file
	key_t mkey, skey, shmkey;
	mkey = ftok(KEYPATH, MSG_ID);
	skey = ftok(KEYPATH, SEM_ID);
	shmkey = ftok(KEYPATH, SHM_ID);
	if ((mkey == -1) || (skey == -1) || (shmkey == -1)) {
		perror("Failed to retreive keys.");
		return 1;
	}

	/*************** Set up signal handler ********/
	if (initsighandlers() == -1) {
		perror("Failed to set up signal handlers.");
		return 1;
	}

	/*************** Set up semaphore *************/
	// semaphore contains 3 sems:
	// 0 = child file i/o lock
	// 1 = log file (oss) i/o lock 
	semid = getsemid(skey, 2);
	if (semid == -1) {
		perror("Failed to create semaphore.");
		return 1;
	}	
	if (initsemaphores(semid) == -1) {
		perror("Failed to init semaphores.");
		return 1;
	}
	setsembuf(mutex, 1, -1, 0);
	setsembuf(mutex+1, 1, 1, 0);

	/************** Set up message queue *********/
	// Initiate message queue	
	int msgid = getmsgid(mkey);
	if (msgid == -1) {
		perror("Failed to create message queue.");
		return 1;
	}

	// Open log file
	int logf = open(fname, FILEPERMS, PERM);
	if (logf == -1) {
		perror("Could not open log file:");
		return 1;
	}

	/*************** Set up shared memory *********/
	int shmid = getclockshmid(shmkey);
	oss_clock_t* clock;
	if (shmid == -1) {
		perror("Failed to create shared memory segment.");
		return 1;
	}	
	clock = initsharedmemory(shmid); 
	if (clock == (void*)-1) {
		perror("Failed to attack shared memory.");	
		return 1;
	}

	alarm(timeout);
	/****************** Spawn Children ***********/
	// make child
	long cpid;
	int childcount = 0;
	int term = (maxslaves < 19) ? maxslaves : 19;
	while (childcount < term)
	{
		// spawn new child
		// child executes child program
		cpid = spawnchild(logf);
		if (cpid == -1)	
			break; // failed to create child
		childcount++;
	}
	
	// If master fails at spawning children
	if (cpid == -1) {
		perror("Failed to create child.");
		if (removeshmem(msgid, semid, shmid, clock) == -1) {
			perror("failed to remove shared mem segment");
			return 1;
		}
	}

	
	/***************** Parent *****************/
	if (cpid > 0) {
		fprintf(stderr, "OSS: In parent code block.\n");
		dprintf(logf, "OSS: In parent code block.\n");
		int t_params[2];
		t_params[0] = msgid;
		t_params[1] = logf;
		// start message listener thread
		pthread_t tid;
		int e = pthread_create(&tid, NULL, msgthread, t_params);
		if (e) {
			fprintf(stderr, "Failed to create thread.\n");
			return 1;
		}

		// start system clock thread
		pthread_t ctid;
		e = pthread_create(&ctid, NULL, systemclock, clock);
		if (e) {
			fprintf(stderr, "Failed to create thread.\n");
			return 1;
		}

		// The PARENT LOOP
		// continuously adds children until:
		// * the clock has reached 2 s
		// * 100 children have been spawned
		while (clock->sec < 2 && childcount < 100)
		{
			if (childcount > 19) {
				// don't have too many kids at once
				wait(NULL);			
			}
			if (childcount < maxslaves) {
				//spawn new child
				cpid = spawnchild(logf);
				childcount++;
			}
		}
			
		// Waits for all children to be done
		while (wait(NULL))
		{
			if (errno == ECHILD)
				break;
		}
		fprintf(stderr, "All children accounted for\n");
		// wait until clock is finished to close everything
		// lets message thread deal with its queue
		while (clock->sec < 2) {};
		fprintf(stderr, "Internal clock reached 2s.\n");
		fprintf(stderr, "Filename for log: %s\n", fname);
		fprintf(stderr, "Message thread closed.\n");
		// close listening thread
		pthread_cancel(tid);
		close(logf);
		if (removeshmem(msgid, semid, shmid, clock) == -1) {
			// failed to remove shared mem segment
			perror("Failed to remove shared memory.");
			return 1;
		}
		
		fprintf(stderr, "Done. %ld\n", (long)getpgid(getpid()));
	}	
	return 0;	

}

// updates internal system clock
void
updateclock(oss_clock_t* clock)
{
	clock->nsec += 10;
	if (clock->nsec >= 1000000000) {
		clock->sec += 1;
		clock->nsec = 0;
	}
	//fprintf(stderr, "%d.%d\n", clock->sec, clock->nsec);
	return;
}

// thread for udpating system clock
void*
systemclock(void* args)
{
	oss_clock_t* clock = (oss_clock_t*)(args);
	while (clock->sec < 2)
	{
		updateclock(clock);			
	}
	return NULL;
}

// spawns a new child, return childpid, -1 on error
// takes logfile descripter
long
spawnchild(int logfile)
{
	long cpid = fork();
	/***************** Child ******************/
	if (cpid <= 0) {
		// execute child
		execl("./child", "child", (char*)NULL);
		perror("Exec failure.");
		return -1; // if error
	}
	// parent makes thread to write to file
	pthread_t ltid;
	long t_params[2];
	t_params[0] = cpid;
	t_params[1] = (long)logfile;
	// create thread, send t_params
	pthread_create(&ltid, NULL, logchildcreate, (void*)t_params);
	pthread_join(ltid, NULL); // w/o this shit gets crazy
	fprintf(stderr, "joined\n");
	return cpid;
}

// thread for logging child creation
// logfile protected by semaphores
// takes void* as arg
void*
logchildcreate(void* args)
{
	long childpid = *((long*)(args));
	long logf = *((long*)(args)+1);
	pthread_testcancel(); // testcancel sets possible cancellation pts

	// wait until your turn
	if (semop(semid, mutex, 1) == -1) {
		perror("Failed to lock semid.");
		pthread_exit(NULL);
	}

	// CRITICAL SECTION
	fprintf(stderr, "... child %ld spawned.\n", childpid);
	dprintf(logf, "... child %ld spawned.\n", childpid);
	fprintf(stderr, "T_SPAWN: %ld\t%ld\n", childpid, logf);

	// unlock file
	if (semop(semid, mutex+1, 1) == -1) { 		
		perror("Failed to unlock semid.");
		pthread_exit(NULL);	
	}

	pthread_testcancel();
	return NULL;
}

// thread to wait for and recieve messages from child
// logfile protected by semaphores
// takes a void* as arg
void*
msgthread(void* args)
{
	int msgid = *((int*)(args));
	int logf = *((int*)(args)+1);
	pthread_testcancel();
	while (69)
	{
		pthread_testcancel();
		// sleep for 3000 microseconds, gives spawnLOG more time
		usleep(3000);
		// wait until your turn
		if (semop(semid, mutex, 1) == -1) {
			perror("Failed to lock semid.");
			pthread_exit(NULL);
		}

		// CRITICAL SECTION
		mymsg_t msg;
		// retrieve message
		ssize_t sz = getmessage(msgid, &msg);
		if (sz != (ssize_t)-1) {
			char* m0 = "OSS: ";
			char* m1 = msg.mtext;
			dprintf(logf, "%s%s\n",m0,m1);
			fprintf(stderr, "%s%s\n",m0,m1);
		} 

		// unlock file
		if (semop(semid, mutex+1, 1) == -1) { 		
			perror("Failed to unlock semid.");
			pthread_exit(NULL);	
		}
		//wait(NULL);
		pthread_testcancel();
	}
	return NULL;	
}

// initialize signal handlers, return -1 on error
int
initsighandlers()
{
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
oss_clock_t*
initsharedmemory(int shmid)
{
	oss_clock_t* clock;
	clock = attachshmclock(shmid);
	if (clock == (void*)-1) {
		perror("Failed to attack shared memory.");	
		return (void*)-1;
	}
	clock->sec = 0;
	clock->nsec = 0;
	return clock;
}

// initialize semaphores, return -1 on error
int
initsemaphores(int semid)
{
	// set up file i/o lock
	if (initelement(semid, 0, 1) == -1) {
		if (semctl(semid, 0, IPC_RMID) == -1)
			return -1;
		return -1;
	}
	// set up child limiter
	if (initelement(semid, 1, 1) == -1) {
		if (semctl(semid, 0, IPC_RMID) == -1)
			return -1;
		return -1;
	}
	return 0;
}

// when the user dun goofs
void
printusage()
{
	fprintf(stderr, "\n\tUsage: ");
	fprintf(stderr, "./oss [-s x] [-l filename] ");
	fprintf(stderr, "[-t z]\n\n\tx: max # slave pxs [1-100]\n");
	fprintf(stderr, "\tfilename: of log\n");
	fprintf(stderr, "\tz: timeout (sec) [1-1000]\n\n");
	return;
}

// when the user dun goofs
void
printopterr(char optopt)
{
	if (optopt == 's' || optopt == 'l' || optopt == 't')
		fprintf(stderr, "\tOption -%c requires an argument.\n", optopt);
	else if (isprint(optopt))
		fprintf(stderr, "\tInvalid option %c\n", optopt);
	return;
}

