#ifndef PROCCNTL_H_
#define PROCCNTL_H_
#define MAXPROCESSES 18

#include "osstypes.h"

int initprocesscntlblock();
pxs_cb_t* getprocesscntlblock();
int dispatchnextprocess();
pxs_cb_t allocatenewprocess();
int putinblock(pxs_cb_t process);
int removefromblock(pxs_cb_t process);
int findprocessindex(pxs_cb_t process);
int findfreeblock();
int freeprocesscntlblock();

#endif
