/*
$Id: sighandler.h,v 1.1 2017/10/23 07:27:24 o1-hester Exp o1-hester $
$Date: 2017/10/23 07:27:24 $
$Revision: 1.1 $
$Log: sighandler.h,v $
Revision 1.1  2017/10/23 07:27:24  o1-hester
Initial revision

Revision 1.1  2017/10/04 07:45:49  o1-hester
Initial revision

$Author: o1-hester $
*/

#ifndef SIGHANDLER_H_
#define SIGHANDLER_H_

#include "ipchelper.h"
#include <signal.h>

void catchctrlc(int signo);
void handletimer(int signo);
void catchuserintr(int signo);

#endif
