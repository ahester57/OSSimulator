
// Austin Hester

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "proccntl.h"
#include "procsched.h"

/********* Process Cntl Blocks ******/
static pxs_cb_t* pxscntlblock;
// block representing free(open) pxs_cntl_block
static pxs_cb_t openblock = {-1, -1, -1, -1, -1};

// for giving process ids
static unsigned int id_count = 10;
static unsigned int dispatch_count = 10;
static unsigned int prev_id = 10;

// initialize
int
initprocesscntlblock()
{
	pxscntlblock = (pxs_cb_t*)malloc(MAXPROCESSES*sizeof(pxs_cb_t));
	if (pxscntlblock == NULL)
		return -1;
	int i;
	for (i = 0; i < MAXPROCESSES; i++) {
		pxscntlblock[i] = openblock;
	}
	return 0;
}

// return process control block
pxs_cb_t*
getprocesscntlblock()
{
	if (pxscntlblock != NULL)
		return pxscntlblock;
	return NULL;
}

// assign priorities to each process
void
prioritize()
{
	int i;
	for (i = 0; i < MAXPROCESSES; i++) {
		if (pxscntlblock[i].proc_id == -1)
			continue;
		decidepriority(&pxscntlblock[i]);
	}
	return;
}

// dispatch process by setting dispatch process id
// gets next process id from process scheduler
int
dispatchnextprocess(pxs_cb_t* dispatch)
{
	if (dispatch == NULL)
		return -1;
	pxs_cb_t next;
	do
	{
		next = getnextscheduledproc();
	} while (next.proc_id == -1);	
	dispatch->proc_id = next.proc_id;	
	dispatch->quantum = next.quantum;
	//dispatch->proc_id = dispatch_count;	
	dispatch_count++;
	return 0;
}

// dispatch process by setting dispatch process id
int
dispatchprocess(pxs_cb_t* dispatch, int proc_id)
{
	if (dispatch == NULL)
		return -1;
	dispatch->proc_id = proc_id;	
	return 0;
}

// forks next process
int
forknextprocess()
{
	char* childpid = (char*)malloc(16*sizeof(char));
	if (childpid == NULL)
		return 1;
	sprintf(childpid, "%d", prev_id++);

	long cpid = fork();	
	if (cpid <= 0) {
		// execute child
		execl("./user", "user", childpid, (char*)NULL);
		perror("OSS: Exec failure.");
		return -1; // if error
	}
	return 0;
}

// initialize a new process
pxs_cb_t
makenewprocessblock()
{
	pxs_cb_t newpxs;
	newpxs.proc_id = id_count++;
	newpxs.used_cpu_time = 0;
	newpxs.system_total_time = 0;
	newpxs.last_burst_time = 0;
	newpxs.priority = 0;
	return newpxs;
}

// puts given process block in block, returns index or -1 on failure
int
addtoblock(pxs_cb_t process)
{
	int index = findfreeblock();
	if (index == -1)
		return -1;
	pxscntlblock[index] = process;
	return index;
}

// removes given process from block, return -1 if not found
int
removefromblock(const pxs_cb_t process)
{
	int index = findprocessindex(process);
	if (index == -1)
		return -1;
	pxscntlblock[index] = openblock;	
	return index;
}

// returns index of given process, return -1 if not found
int
findprocessindex(const pxs_cb_t process)
{
	int proc_id = process.proc_id;
	int i;
	for (i = 0; i < MAXPROCESSES; i++) {
		// if process found
		if (pxscntlblock[i].proc_id == proc_id) {
			return i;
		}
	}
	return -1;
}

// returns 1st open block, return -1 if full
int
findfreeblock()
{
	int i;
	for (i = 0; i < MAXPROCESSES; i++) {
		// if open block, return index
		if (pxscntlblock[i].proc_id == -1) {
			return i;
		}
	}
	return -1;
}

// call at end, 
int
freeprocesscntlblock()
{
	free(pxscntlblock);
	if (errno)
		return errno;
	return 0;
}
