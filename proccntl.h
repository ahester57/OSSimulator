#ifndef PROCCNTL_H_
#define PROCCNTL_H_
#define MAXPROCESSES 18

#include "osstypes.h"

int initprocesscntlblock();
pxs_cb_t* getprocesscntlblock();
pxs_cb_t allocatenewprocess();
int freeprocesscntlblock();

#endif
