## Operating System Simulator 

### Austin Hester  
##### University of Missouri - St. Louis  
##### CS4760 - Operating Systems 
##### Sanjiv Bhatia  

To build and run:  
```  
make  
./oss [-s x] [-l filename] [-t z]
```
where  
`x` is the max number of child processes,  
`filename` is the name of log file,  
`z` is timeout (seconds).

Keeps track of an internal system clock in shared memory.  

Uses threads to:  

	* Increment system clock
	* Receive message from child
	* Log messasge from child
	* Log the spawning of children

Child processes use message queues to talk to master.  
Messages contain:

	* mtype := message type (1 for success) 
	* mtext := contains time of termination and time it expired

Protects child's critical section (read from shared memory) with semaphores.  
Semaphores to control:  

	* Locking file I/O  
	* Child read from shared memory 

Utilizes signal handlers to properly clean up when receiving `SIGINT` from `Ctrl^C` or `SIGALRM` on timeout.  
Child blocks signals during critical section, exits after with a trap.  

	* Child processes exit if not in critical section  
	* Removes messages in queue  
	* Removes semaphore sets  
	* Removes shared memory segments
	* Child in critical section exits when in remainder section  

`https://github.com/ahester57/OSSimulator`
