#ifndef PROCCNTL_H_
#define PROCCNTL_H_
#define MAXPROCESSES 18

#include "osstypes.h"

int initprocesscntlblock();
pxs_cb_t* getprocesscntlblock();
void prioritize();
int dispatchnextprocess();
int forknextprocess();
pxs_cb_t allocatenewprocess();
int putinblock(pxs_cb_t process);
int removefromblock(const pxs_cb_t process);
int findprocessindex(const pxs_cb_t process);
int findfreeblock();
int freeprocesscntlblock();

#endif
