## Operating System Simulator 

### Austin Hester  
##### University of Missouri - St. Louis  
##### CS4760 - Operating Systems 
##### Sanjiv Bhatia  

To build and run:  
```  
make  
./oss  
```

Keeps track of an internal system clock in shared memory.  

Child processes use message queues to talk to master.  
Messages contain:

	* msec := seconds passed  
	* mnsec := nanoseconds passed 

Protects child's critical section (read from shared memory) with semaphores.  
Semaphores to control:  

	* Locking file I/O  
	* Limiting max # of processes   

Utilizes signal handlers to properly clean up when receiving `SIGINT` from `Ctrl^C` or `SIGALRM` on timeout.  
Child blocks signals during critical section, exits after with a trap.  

	* Child processes exit if not in critical section  
	* Removes messages in queue  
	* Removes semaphore sets  
	* Removes shared memory segments
	* Child in critical section exits when in remainder section  


