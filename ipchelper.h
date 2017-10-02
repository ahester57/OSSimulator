/*
$Id$
$Date$
$Revision$
$Log$
$Author$
*/

#ifndef IPCHELPER_H_
#define IPCHELPER_H_

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/stat.h>
#define PERM (S_IRUSR | S_IWUSR)
#define KEYPATH "./ipchelper.h"
#define SHM_ID 7452542
#define MSG_ID 5879785 
#define SEM_ID 3490943 
#define LINESIZE 256

// for message queues
typedef struct {
	long mType;
	char mText[LINESIZE];
} mymsg_t;

typedef struct {
	int sec;
	int nsec;
} oss_clock_t;

int initelement(int semid, int semnum, int semval);
int getsemid(key_t skey, int nsems);
int getmsgid(key_t mkey);
int getclockshmid(key_t shmclockkey);
oss_clock_t* attachshmclock(int shmid);
void setsembuf(struct sembuf *s, int n, int op, int flg);
int sendmessages(int msgid, char** mylist, int lines);
int removeMsgQueue(int msgid);
void setmsgid(int msgid);
int removeshmem(int msgid, int semid, int shmid, void* shmaddr);
int detachandremove(int shmid, void* shmaddr);

#endif
