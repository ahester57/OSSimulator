
// Austin Hester

#include <stdio.h>
#include <stdlib.h>
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

// initialize a new process
pxs_cb_t
allocatenewprocess()
{
	pxs_cb_t newpxs;
	newpxs.proc_id = 14;
	newpxs.used_cpu_time = 0;
	newpxs.system_total_time = 0;
	newpxs.last_burst_time = 0;
	newpxs.priority = 0;
	return newpxs;
}

int
freeprocesscntlblock()
{
	free(pxscntlblock);
	if (errno)
		return errno;
	return 0;
}
