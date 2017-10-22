
// Austin Hester

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "proccntl.h"
#include "procsched.h"

/********* Process Cntl Blocks ******/
static pxs_cb_t* pxscntlblock;
static pxs_id_t currentpxs[MAXPROCESSES];
// block representing free(open) pxs_cntl_block
static pxs_cb_t openblock = {-1, -1, -1, -1, -1};
// repr. an open id block
static pxs_id_t openpxsid = {-1, -1};

// for giving process ids
static unsigned int id_count = 10;
static unsigned int prev_id = 10;

int
initprocesscntlblock()
{
	pxscntlblock = (pxs_cb_t*)malloc(MAXPROCESSES*sizeof(pxs_cb_t));
	if (pxscntlblock == NULL)
		return -1;
	int i;
	for (i = 0; i < MAXPROCESSES; i++) {
		pxscntlblock[i] = openblock;
		currentpxs[i] = openpxsid;
	}
	return 0;
}

pxs_cb_t*
getprocesscntlblock()
{
	if (pxscntlblock != NULL)
		return pxscntlblock;
	return NULL;
}

int
dispatchnextprocess()
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
allocatenewprocess()
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
putinblock(pxs_cb_t process)
{
	int index = findfreeblock();
	if (index == -1)
		return -1;
	pxscntlblock[index] = process;
	return index;
}

// removes given process from block, return -1 if not found
int
removefromblock(pxs_cb_t process)
{
	int index = findprocessindex(process);
	if (index == -1)
		return -1;
	pxscntlblock[index] = openblock;	
	return index;
}

// returns index of given process, return -1 if not found
int
findprocessindex(pxs_cb_t process)
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

int
freeprocesscntlblock()
{
	free(pxscntlblock);
	if (errno)
		return errno;
	return 0;
}
