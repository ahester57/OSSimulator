## Operating System Simulator 

### Austin Hester  
##### University of Missouri - St. Louis  
##### CS4760 - Operating Systems 
##### Sanjiv Bhatia  

This project is to familiarize yourself with semaphores,  
however, I have used it to familiarize myself with threads.

Since we were to increment the clock in a loop in which we  
are also supposed to receive messages and spawn new children,  
I thought it a good idea to put the system clock in a thread.  
 
I also use two other threads, one of which happens multiple times.  
The first other thread is the child spawn log thread. Since  
I use `sem_wait` (blocking) to ensure mutual exclusion on the  
log file, I do not want my process to hold up waiting for access.  

The other other thread was actually the reason I started using   
threads. It is the Message Listener thread. I do not use a single  
shared memory space for the message child sends, but I use a message  
queue. The message queue allows the expired child to terminate   
immediately instead of waiting for the parent to deal with the   
previous child. Think of this guy as a babysitter.  

To build and run:  
```  
make  
./oss [-h] [-s x] [-l filename] [-t z]
```
where  
`h` prints this usage info,  
`x` is the max number of child processes,  
`filename` is the name of log file,  
`z` is timeout (seconds).

Keeps track of an internal system clock in shared memory (POSIX).  

Uses threads to:  

	* Increment system clock
	* Receive message from child
	* Log messages from child
	* Log the spawning of children

Child processes use message queues (POSIX) to talk to master.  
Messages contain:

	* mtype := message type (1 for success) 
	* mtext := contains time of termination and time it expired

Protects child's critical section (read from shared memory) with semaphores (POSIX).   
Semaphores to control:  

	* Locking logfile I/O  
	* Child read from shared memory 
	* Child signaling message listener about new message

Utilizes signal handlers to properly clean up when receiving `SIGINT` from `Ctrl^C` or `SIGALRM` on timeout.  
Child blocks signals during critical section, exits after with a trap.  

	* Child processes exit if not in critical section  
	* Exits open threads
	* Removes messages in queue  
	* Removes semaphore sets  
	* Removes shared memory segments
	* Child in critical section exits when in remainder section  

Version Control:  
`https://github.com/ahester57/OSSimulator`  
and RCS a little.  
