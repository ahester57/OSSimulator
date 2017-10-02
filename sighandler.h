/*
$Id$
$Date$
$Revision$
$Log$
$Author$
*/

#ifndef SIGHANDLER_H_
#define SIGHANDLER_H_

#include "ipchelper.h"
#include <signal.h>

void catchctrlc(int signo);
void handletimer(int signo);
void catchchildintr(int signo);

#endif
