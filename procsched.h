#ifndef PROCSCHED_H_
#define PROCSCHED_H_
#define MAXPROCESSES 18
#define N_PRIORITIES 3

#include "osstypes.h"

int initpriorityqueue();
pxs_cb_t** getpriorityqueue();
pxs_cb_t getnextscheduledproc();
int decidepriority(pxs_cb_t* process);
int isinpriorityqueue(int proc_id);
int addtopriorityqueue(pxs_cb_t process);
int removefrompriorityqueue(const pxs_cb_t process);
pxs_cb_t pop(int qnum);
int findprocessindexqueue(const pxs_cb_t process);
int findfreeblockqueue(const int qnum);
int freequeue();

#endif
