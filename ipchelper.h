/*
$Id: ipchelper.h,v 1.2 2017/10/10 20:18:25 o1-hester Exp o1-hester $
$Date: 2017/10/10 20:18:25 $
$Revision: 1.2 $
$Log: ipchelper.h,v $
Revision 1.2  2017/10/10 20:18:25  o1-hester
bug fix

Revision 1.1  2017/10/04 07:45:11  o1-hester
Initial revision

$Author: o1-hester $
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
#include "osstypes.h"
#define PERM (S_IRUSR | S_IWUSR)
#define RPERM (S_IRUSR)
#define KEYPATH "./ipchelper.h"
#define SHM_ID 7452542
#define MSG_ID 5879785 
#define SEM_ID 3490943 
#define LINESIZE 256

int initelement(int semid, int semnum, int semval);
int getsemid(const key_t skey, const int nsems);
int getmsgid(const key_t mkey);
int getclockshmid(const key_t shmclockkey);
int getdispatchshmid(const key_t shmkey);
int getclockshmid_ro(const key_t shmkey);
int getdispatchshmid_ro(const key_t shmkey);
oss_clock_t* attachshmclock(const int shmid);
pxs_id_t* attachshmdispatch(const int shmid);
void setsembuf(struct sembuf *s, int n, int op, int flg);
int sendmessage(const int msgid, const long pid,
		const oss_clock_t endtime, oss_clock_t* clock);
ssize_t getmessage(const int msgid, mymsg_t* msg);
int removemsgqueue(const int msgid);
int removeshmem(int msgid, int semid, int shmid, void* shmaddr);
int detachandremove(const int shmid, void* shmaddr);

#endif
