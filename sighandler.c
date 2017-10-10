/*
$Id: sighandler.c,v 1.1 2017/10/04 07:45:49 o1-hester Exp o1-hester $
$Date: 2017/10/04 07:45:49 $
$Revision: 1.1 $
$Log: sighandler.c,v $
Revision 1.1  2017/10/04 07:45:49  o1-hester
Initial revision

$Author: o1-hester $
*/

#include <unistd.h>
#include <sys/wait.h>
#include "ipchelper.h"
#include "sighandler.h"

// whether the alarm occured, for outputting why its over
static int alarmhappened = 0;

/************************ Signal Handler ********************/
// Handler for SIGINT
void
catchctrlc(int signo)
{
	alarm(0); // cancel alarm
	if (alarmhappened == 0) {
		char* msg = "Ctrl^C pressed, killing children.\n";
		write(STDERR_FILENO, msg, 36);
	}
	removeshmem(-1, -1, -1, (void*)-1);
	pid_t pgid = getpgid(getpid());
	while(wait(NULL))
	{
		if (errno == ECHILD)
			break;
	}

	kill(pgid, SIGKILL);
	//sleep(2);
}

// Handler for SIGALRM
void
handletimer(int signo)
{
	alarmhappened = 1;
	char* msg = "Alarm occured. Time to kill children.\n";
	write(STDERR_FILENO, msg, 39);
	pid_t pgid = getpgid(getpid());

	kill(pgid, SIGINT);
}

// handler for palin SIGINT
void
catchchildintr(int signo)
{
	char msg[] = "Child interrupted. Goodbye.\n";
	write(STDERR_FILENO, msg, 32);
	exit(1);
}
