/*
$Id: oss.c,v 1.2 2017/10/04 07:55:18 o1-hester Exp o1-hester $
$Date: 2017/10/04 07:55:18 $
$Revision: 1.2 $
$Log: oss.c,v $
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

long spawnchild();
void updateclock(oss_clock_t* clock);
void* msgthread(void*);
int initsighandlers();
oss_clock_t* initsharedmemory(int shmid);
int initsemaphores(int semid);
void logchildcreate(int logf, long childpid);
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
	while ((c = getopt(argc, argv, "hs:l:t:")) != -1) {
		switch(c) {
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
	alarm(timeout);

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
	if ((clock = initsharedmemory(shmid)) == (void*)-1) {
		perror("Failed to attack shared memory.");	
		return 1;
	}

	/*************** Set up semaphore *************/
	// semaphore contains 3 sems:
	// 0 = file i/o lock
	// 1 = for limiting to 19 children at one time
	int semid;
	if ((semid = getsemid(skey, 2)) == -1) {
		perror("Failed to create semaphore.");
		return 1;
	}	
	struct sembuf limitchildren[1];
	setsembuf(limitchildren, 1, -1, 0);
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

	// Open log file
	int logf = open(fname, (O_WRONLY | O_CREAT), PERM);
	if (logf == -1) {
		perror("Could not open log file:");
		return 1;
	}

	/****************** Spawn Children ***********/
	// make child
	long cpid;
	int childcount = 0;
	int term = (maxslaves < 19) ? maxslaves : 19;
	while (childcount < term) {
		// spawn new child
		// child executes child program
		cpid = spawnchild();
		if (cpid == -1)	
			break; // failed to create child
		// log child creation
		logchildcreate(logf, cpid);
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

	
	int numlivechild = childcount;
	/***************** Parent *****************/
	if (cpid > 0) {
		fprintf(stderr, "OSS: In parent code block.\n");
		dprintf(logf, "OSS: In parent code block.\n");
		int t_params[2];
		t_params[0] = msgid;
		t_params[1] = logf;
		pthread_t tid;
		int e = pthread_create(&tid, NULL, msgthread, t_params);
		if (e) {
			fprintf(stderr, "Failed to create thread.\n");
			return 1;
		}
		// enter clock increment loop
		while (clock->sec < 2) {
			// update the system clock
			updateclock(clock);			
			// rc, mcount get number of messages in queue
			struct msqid_ds buf;
			int rc = msgctl(msgid, IPC_STAT, &buf);	
			if (rc == -1) {
				perror("Message failure.");
				return 1;
			}
			int mcount = (int)(buf.msg_qnum);
			if (mcount > 0) {
				if (childcount > 19)
					wait(NULL);
				numlivechild--;
				if (childcount < maxslaves) {
					//spawn new child
					cpid = spawnchild();
					logchildcreate(logf, cpid);
					childcount++;
					numlivechild++;
				}
			}
			
		}
		fprintf(stderr, "Internal clock reached 2s.\n");
		fprintf(stderr, "Filename for log: %s\n", fname);
		pthread_cancel(tid);
		fprintf(stderr, "Thread closed: %ld\n", (long)tid);
		//pthread_join(tid, NULL);
		close(logf);
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

// spawns a new child, return childpid, -1 on error
long
spawnchild()
{
	long childpid = fork();
	/***************** Child ******************/
	if (childpid <= 0) {
		// execute child
		execl("./child", "child", (char*)NULL);
		perror("Exec failure.");
		return -1; // if error
	}
	// parent
	return childpid;
}


// updates internal system clock
void
updateclock(oss_clock_t* clock)
{
	clock->nsec += 100;
	if (clock->nsec >= 1000000000) {
		clock->sec += 1;
		clock->nsec = 0;
	}
	//fprintf(stderr, "%d.%d\n", clock->sec, clock->nsec);
	return;
}

// executed in thread to wait for and recieve messages from child
// takes a void* as arg
void*
msgthread(void* arg)
{
	int msgid = *( (int*)(arg));
	int logf = *( (int*)(arg + 1));
	while (1)
	{
		//usleep(1000);
		pthread_testcancel();
		//fprintf(stderr, "fuck trump%d\n", msgid);
		// rc, mcount get number of messages in queue
		struct msqid_ds buf;
		int rc = msgctl(msgid, IPC_STAT, &buf);	
		if (rc == -1) {
			perror("THREAD: Message failure.");
		}
		int mcount = (int)(buf.msg_qnum);
		pthread_testcancel();
		if (mcount > 0) {
			mymsg_t msg;
			ssize_t sz = getmessage(msgid, &msg);
			if (sz != (ssize_t)-1) {
				char* m0 = "OSS: ";
				char* m1 = msg.mtext;
				dprintf(logf, "%s%s\n",m0,m1);
				fprintf(stderr, "THREAD: %s%s\n",m0,m1);
			}
		}
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
	if ((clock = attachshmclock(shmid)) == (void*)-1) {
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
	if (initelement(semid, 1, 19) == -1) {
		if (semctl(semid, 0, IPC_RMID) == -1)
			return -1;
		return -1;
	}
	return 0;
}

// log child creation
void
logchildcreate(int logf, long childpid)
{
	fprintf(stderr, "... child %ld spawned.\n", childpid);
	dprintf(logf, "... child %ld spawned.\n", childpid);
	return;
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

