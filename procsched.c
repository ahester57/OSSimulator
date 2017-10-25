
// Austin Hester

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "procsched.h"

static pxs_cb_t** queue;
// repr. an open block
static pxs_cb_t openblock = {-1, -1, -1, -1, -1};

static int count = 0;

// initialize priority queues to all open blocks
int
initpriorityqueue()
{
	queue = (pxs_cb_t**)malloc(N_PRIORITIES*sizeof(pxs_cb_t*));
	if (queue == NULL)
		return -1;
	int i, j;
	for (i = 0; i < N_PRIORITIES; i++) {
		queue[i] = (pxs_cb_t*)malloc(MAXPROCESSES*sizeof(pxs_cb_t));
		if (queue[i] == NULL)
			return -1;
		for (j = 0; j < MAXPROCESSES; j++) {
			queue[i][j] = openblock;
		}
	}
	return 0;
}

// retreive priority queue
pxs_cb_t**
getpriorityqueue()
{
	if (queue != NULL)
		return queue;
	else
		return NULL;
}

// gets what it thinks the next process should be
pxs_cb_t
getnextscheduledproc()
{
	pxs_cb_t next;
	int num = count % 10;
	int qindex;
	if (num < 6)
		qindex = 0;
	else if (num < 9)
		qindex = 1;
	else
		qindex = 2;
	next = pop(qindex);
	count++;
	return next;
}

// determine priority queue based on wait/time
int
decidepriority(pxs_cb_t* process)
{
	int p;
	p = (int) rand() % 3;
	int q = 10000000;
	if (p == 1)
		q = q / 2;
	else if (p == 2)
		q = q / 4;
	process->priority = p;
	process->quantum = q;
	addtoqueue(process, p);
	return p;
}

// puts given process block in given queue, returns index or -1 on failure
int
addtoqueue(pxs_cb_t* process, int qnum)
{
	int index = findfreeblockqueue(qnum);
	if (index == -1)
		return -1;
	queue[qnum][index] = *process;
	return index;
}

// removes given process from queue, return -1 if not found
int
removefromqueue(const pxs_cb_t process)
{
	int qnum = process.priority;
	int index = findprocessindexqueue(process);
	if (index == -1)
		return -1;
	queue[qnum][index] = openblock;	
	return index;
}

// pops head of asked queue
pxs_cb_t
pop(int qnum)
{
	int i;
	pxs_cb_t temp = queue[qnum][0];
	for (i = 0; i < MAXPROCESSES-1; i++) {
		queue[qnum][i] = queue[qnum][i+1];
	}
	queue[qnum][MAXPROCESSES-1] = openblock;
	//removefromqueue(temp);
	return temp;
}

// returns index of given process, return -1 if not found
int
findprocessindexqueue(const pxs_cb_t process)
{
	int proc_id = process.proc_id;
	int priority = process.priority;
	int i;
	for (i = 0; i < MAXPROCESSES; i++) {
		// if process found
		if (queue[priority][i].proc_id == proc_id) {
			return i;
		}
	}
	return -1;
}

// returns 1st open block in q, return -1 if full
int
findfreeblockqueue(const int qnum)
{
	int i;
	for (i = 0; i < MAXPROCESSES; i++) {
		// if open block, return index
		if (queue[qnum][i].proc_id == -1) {
			return i;
		}
	}
	return -1;
}

// frees queue, call at end
int
freequeue()
{
	free(queue);
	if (errno)
		return errno;
	return 0;
}

