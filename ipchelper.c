/*
$Id: ipchelper.c,v 1.1 2017/10/23 07:27:24 o1-hester Exp o1-hester $
$Date: 2017/10/23 07:27:24 $
$Revision: 1.1 $
$Log: ipchelper.c,v $
Revision 1.1  2017/10/23 07:27:24  o1-hester
Initial revision

Revision 1.3  2017/10/11 20:32:12  o1-hester
turnin

Revision 1.2  2017/10/10 20:18:18  o1-hester
bug fix

Revision 1.1  2017/10/04 07:45:11  o1-hester
Initial revision

$Author: o1-hester $
*/

#include "ipchelper.h"

// id's and addr of shared memory and whatnot
static int sem_id;
static int msg_id;
static int shm_clock_id;
static int shm_dispatch_id;
static oss_clock_t* shm_clock_addr;
static pxs_cb_t* shm_dispatch_addr;

// Initializes semaphore with id, num, and value
// returns -1 on failure
int
initelement(int semid, int semnum, int semval)
{
	union semun {
		int val;
		struct semid_ds *buf;
		unsigned short *array;
	} arg;
	arg.val = semval;
	return semctl(semid, semnum, SETVAL, arg);
}

// creates semaphore set, returns -1 on error and semid on success
int
getsemid(const key_t skey, const int nsems)
{
	sem_id = semget(skey, nsems, PERM | IPC_CREAT);
	// seems pointless, but what if i wanna do something on failure?
	if (sem_id == -1) {
		return -1;
	}
	return sem_id;
}

// creates message queue, returns -1 on error and msgid on success
int
getmsgid(const key_t mkey)
{
	msg_id = msgget(mkey, PERM | IPC_CREAT);
	if (msg_id == -1) {
		return -1;
	}
	return msg_id;
}

// creates shared memory, returns -1 on error and shmid on success
int
getclockshmid(const key_t shmkey)
{
	shm_clock_id = shmget(shmkey, sizeof(oss_clock_t), PERM|IPC_CREAT);
	if (shm_clock_id == -1) {
		return -1;
	}
	return shm_clock_id;
}

// creates shared memory, returns -1 on error and shmid on success
int
getdispatchshmid(const key_t shmkey)
{
	shm_dispatch_id = shmget(shmkey, sizeof(pxs_cb_t), PERM|IPC_CREAT);
	if (shm_dispatch_id == -1) {
		return -1;
	}
	return shm_dispatch_id;
}

// gets shared memory (read only), returns -1 on error and shmid on success
int
getclockshmid_ro(const key_t shmkey)
{
	return shmget(shmkey, sizeof(oss_clock_t), RPERM);
}

/*
// gets shared memory (read only), returns -1 on error and shmid on success
int
getdispatchshmid_ro(const key_t shmkey)
{
	return shmget(shmkey, sizeof(pxs_cb_t), RPERM);
}
*/

// attach system clock to shared memory segment, return -1 on error
oss_clock_t*
attachshmclock(const int shmid)
{
	shm_clock_addr = (oss_clock_t*)shmat(shmid, NULL, 0);
	if (shm_clock_addr == (void*)-1) {
		return (void*)-1;
	}
	return shm_clock_addr;
}

// attach dispatch to shared memory segment, return -1 on error
pxs_cb_t*
attachshmdispatch(const int shmid)
{
	shm_dispatch_addr = (pxs_cb_t*)shmat(shmid, NULL, 0);
	if (shm_dispatch_addr == (void*)-1) {
		return (void*)-1;
	}
	return shm_dispatch_addr;
}

//set up a semaphore operation
void
setsembuf(struct sembuf *s, int n, int op, int flg)
{
	s->sem_num = (short)n;
	s->sem_op = (short)op;
	s->sem_flg = (short)flg;
	return;
}


// send messages to msgqueue, return -1 on error
int
sendmessage(const int msgid, const long pid,
		const oss_clock_t endtime, const oss_clock_t clock)
{
	mymsg_t* mymsg = (mymsg_t*) malloc(sizeof(mymsg_t));
	if (mymsg == NULL) {
		return -1;
	}
	// ti is current time
	// eti is child expiry time 
	int t0 = clock.sec;
	int t1 = clock.nsec;
	int et0 = endtime.sec;
	int et1 = endtime.nsec;
	// strings and what not
	char* m0 = "Child ";
	char* m1 = " terminating at ";
	char* m2 = " because it reached ";
	char* t_m = (char*)malloc(LINESIZE*sizeof(char));
	if (t_m == NULL) {
		return -1;
	}
	// This message holds: child pid, termination time, and expiry time
	sprintf(t_m,"%s%ld%s%d.%d%s%d.%d",m0,pid,m1,t0,t1,m2,et0,et1);
	mymsg->mtype = 1;
	mymsg->proc_id = pid;
	memcpy(mymsg->mtext, t_m, LINESIZE);
	if (msgsnd(msgid, mymsg, sizeof(mymsg_t), 0) == -1) {
		free(t_m);
		return -1;
	}	
	free(t_m);
	return 0;
}

// gets next message in queue, returns size or -1 on failure
ssize_t
getmessage(const int msgid, mymsg_t* msg)
{
	ssize_t sz;
	sz = msgrcv(msgid, msg, sizeof(mymsg_t),0,IPC_NOWAIT);
	if (sz == (ssize_t)-1 && errno != ENOMSG) {
		return (ssize_t)-1;
	}	
	return sz;
}

// destroy message queue segment, return -1 on error
int
removemsgqueue(const int msgid)
{
	return msgctl(msgid, IPC_RMID, NULL);
}

// Remove shared memory segments, return -1 on error
int
removeshmem(int msgid, int semid, int shmid, void* shmaddr)
{
	int error = 0;
	if (msgid == -1)
		msgid = msg_id;
	if (semid == -1)
		semid = sem_id;
	if (shmid == -1)
		shmid = shm_clock_id;
	if (shmaddr == (void*)-1)
		shmaddr = shm_clock_addr;
	// Kill message queue
	char* msg = "IPC: Killing msgqueue.\n";
	write(STDERR_FILENO, msg, 23);
	if (removemsgqueue(msgid) == -1) {
		error = errno;
	}
	// kill semaphore set
	msg = "IPC: Killing semaphore set.\n";
	write(STDERR_FILENO, msg, 28);
	if (semctl(semid, 0, IPC_RMID) == -1) {
		error = errno;
	}
	// kill semaphore set
	msg = "IPC: Killing shared memory segments.\n";
	write(STDERR_FILENO, msg, 37);
	if (detachandremove(shm_dispatch_id, shm_dispatch_addr) == -1) {
		error = errno;
	}
	if (detachandremove(shmid, shmaddr) == -1) {
		error = errno;
	}
	if (error != 0)
		return -1;
	return 0;
}

// Detaches and removes shared memory, return -1 on error
int
detachandremove(const int shmid, void* shmaddr)
{
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

