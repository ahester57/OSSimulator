#ifndef OSSTYPES_H_
#define OSSTYPES_H_

#define LINESIZE 256
#define BILLION 1000000000

// for message queues
typedef struct
{
	int mtype;
	unsigned int proc_id;
	char mtext[LINESIZE];
} mymsg_t;

// oss simulated clock
typedef struct
{
	unsigned int sec;
	unsigned int nsec;
} oss_clock_t;

// process control block
typedef struct
{
	unsigned int proc_id;
	unsigned int used_cpu_time;
	unsigned int system_total_time;
	unsigned int last_burst_time;
	unsigned int wait_time;
	unsigned int priority;
	unsigned int quantum;
	int done;
} pxs_cb_t;

#endif
