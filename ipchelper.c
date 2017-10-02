/*
$Id$
$Date$
$Revision$
$Log$
$Author$
*/

#include "ipchelper.h"

static int sem_id;
static int msg_id;
static int shm_clock_id;
static oss_clock_t* shm_addr;

// Initializes semaphore with id, num, and value
// returns -1 on failure
int initelement(int semid, int semnum, int semval) {
	union semun {
		int val;
		struct semid_ds *buf;
		unsigned short *array;
	} arg;
	arg.val = semval;
	return semctl(semid, semnum, SETVAL, arg);
}

// creates semaphores, returns -1 on error and semid on success
int getsemid(key_t skey, int nsems) {
	int semid;
	if ((semid = semget(skey, nsems, PERM | IPC_CREAT)) == -1) {
		return -1;
	}
	sem_id = semid;
	return sem_id;
}

// creates message queue, returns -1 on error and msgid on success
int getmsgid(key_t mkey) {
	int msgid;
	if ((msgid = msgget(mkey, PERM | IPC_CREAT)) == -1) {
		return -1;
	}
	msg_id = msgid;
	return msg_id;
}

// creates shared memory, returns -1 on error and shmid on success
int getclockshmid(key_t shmkey) {
	int shmid;
	if ((shmid = shmget(shmkey, 2*sizeof(int), PERM | IPC_CREAT)) == -1) {
		return -1;
	}
	shm_clock_id = shmid;
	return shm_clock_id;
}

// attach shared memory segment
oss_clock_t* attachshmclock(int shmid) {
	oss_clock_t* clock;
	if ((clock = (oss_clock_t*)shmat(shmid, NULL, 0)) == (void*)-1) {
		return (void*)-1;
	}
	shm_addr = clock;
	return clock;
}

//set up a semaphore operation
void setsembuf(struct sembuf *s, int n, int op, int flg) {
	s->sem_num = (short)n;
	s->sem_op = (short)op;
	s->sem_flg = (short)flg;
	return;
}


// send messages to msgqueue, return -1 on error
int sendmessages(int msgid, char** mylist, int lines) {
	int j;
	mymsg_t* mymsg;
	for (j = 0; j < lines; j++) {
		if ((mymsg = (mymsg_t*) malloc(sizeof(mymsg_t))) == NULL) {
			return -1;
		}
		// mType is index of string
		// mText is string
		// child finds string using mType
		mymsg->mType = j+1;	// because mType cannot be 0
		memcpy(mymsg->mText, mylist[j], LINESIZE);
		if (msgsnd(msgid, mymsg, LINESIZE, 0) == -1) {
			return -1;
		}	
	}
	return 0;
}

// destroy message queue segment
int removeMsgQueue(int msgid) {
	return msgctl(msgid, IPC_RMID, NULL);
}

// Remove shared memory segments
int removeshmem(int msgid, int semid, int shmid, void* shmaddr) {
	int error = 0;
	if (msgid == -1)
		msgid = msg_id;
	if (semid == -1)
		semid = sem_id;
	if (shmid == -1)
		shmid = shm_clock_id;
	if (shmaddr == (void*)-1)
		shmaddr = shm_addr;
	// Kill message queue
	char* msg = "Killing msgqueue.\n";
	write(STDERR_FILENO, msg, 18);
	if (removeMsgQueue(msgid) == -1) {
		error = errno;
	}
	// kill semaphore set
	msg = "Killing semaphore set.\n";
	write(STDERR_FILENO, msg, 23);
	if (semctl(semid, 0, IPC_RMID) == -1) {
		error = errno;
	}
	if (detachandremove(shmid, shmaddr) == -1) {
		error = errno;
	}
	if (error != 0)
		return -1;
	return 0;
}

// Detaches and removes shared memory
int detachandremove(int shmid, void* shmaddr) {
	 int error = 0;
	if (shmdt(shmaddr) == -1)
		error = errno;
	if ((shmctl(shmid, IPC_RMID, NULL) == -1) && !error)
		error = errno;
	if (!error)
		return 0;
	errno = error;
	return -1;
}

