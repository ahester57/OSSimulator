/*
$Id: sighandler.c,v 1.1 2017/10/23 07:27:24 o1-hester Exp o1-hester $
$Date: 2017/10/23 07:27:24 $
$Revision: 1.1 $
$Log: sighandler.c,v $
Revision 1.1  2017/10/23 07:27:24  o1-hester
Initial revision

Revision 1.3  2017/10/11 20:32:12  o1-hester
turnin

Revision 1.2  2017/10/10 20:18:50  o1-hester
bug fix

Revision 1.1  2017/10/04 07:45:49  o1-hester
Initial revision

$Author: o1-hester $
*/

#include <unistd.h>
#include <pthread.h>
#include <sys/wait.h>
#include "osstypes.h"
#include "ipchelper.h"
#include "proccntl.h"
#include "procsched.h"
#include "sighandler.h"

// whether the alarm occured, for outputting why its over
static int alarmhappened = 0;

/************************ Signal Handler ********************/
// Handler for SIGINT
void
catchctrlc(int signo)
{
	pid_t pgid = getpgid(getpid());
	alarm(0); // cancel alarm
	if (alarmhappened == 0) {
		char* msg = "Ctrl^C pressed, killing children.\n";
		write(STDERR_FILENO, msg, 34);
	}
	//kill(pgid, SIGINT);
	removeshmem(-1, -1, -1, (void*)-1);
	while(wait(NULL))
	{
		if (errno == ECHILD)
			break;
	}
	// KILL 'EM ALL, no whammies
	kill(pgid, SIGKILL);
	//pthread_exit(NULL);
	exit(1);
}

// Handler for SIGALRM
void
handletimer(int signo)
{
	alarmhappened = 1;
	char* msg = "Alarm occured. Time to kill children.\nPress Ctrl^C\n";
	write(STDERR_FILENO, msg, 51);

	pid_t pgid = getpgid(getpid());
	while(wait(NULL))
	{
		if (errno == ECHILD)
			break;
	}
	removeshmem(-1, -1, -1, (void*)-1);
	// KILL 'EM ALL, no whammies
	kill(pgid, SIGKILL);
	pthread_exit(NULL);
	exit(1);
}

// handler for user SIGINT
void
catchuserintr(int signo)
{
	char msg[] = "Child interrupted. Goodbye.\n";
	write(STDERR_FILENO, msg, 28);
	exit(1);
}
