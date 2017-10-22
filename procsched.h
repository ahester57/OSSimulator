#ifndef PROCSCHED_H_
#define PROCSCHED_H_
#define MAXPROCESSES 18
#define N_PRIORITIES 3

#include "osstypes.h"

int initpriorityqueue();
pxs_cb_t** getpriorityqueue();
int decidequeue(pxs_cb_t process);
int putinqueue(pxs_cb_t process, int qnum);
int removefromqueue(pxs_cb_t process);
int findprocessindexqueue(pxs_cb_t process);
int findfreeblockqueue(int qnum);
int freequeue();

#endif
