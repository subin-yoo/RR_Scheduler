/*
	 Code is written by Seehwan Yoo (2014)
	The code is public. 
	However, re-distribution is strictly prohibited 
	except for education purpose.
	Do not modify this code segment.
*/
/*
	Code is modified by Subin Yoo.
	10 processes are executed according to round robin sceduling.
	Assume that 10 processes are only CPU-bound jobs.
*/
/**
	scheduler simulation
	create 10 procs. 
	schedule thru round-robin policy
	send ipcs among parent and children (signal)
*/

/* code snippet for OS howmeork */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>

#include <sys/ipc.h>
#include <sys/msg.h>

#define MY_MSGQ_KEY	2627

#include <signal.h>
#include <errno.h>

/* Seehwan Yoo, sched. simulation */

/* timer signal handler */
void time_tick(int);

int global_tick = 0;


//define process state
enum proc_state{
	READY,
	WAIT,
};

//Struct Definitions
typedef struct{
	int pid;
	enum proc_state state;
	int remaining_cpu_time;
	int remaining_io_time;
}pcb;


typedef struct{
	int mtype;
	int pid;
	int time_quantum;
}message;

//Function Declarations

int schedule();

/* child process: the function never returns */
void do_child();

/* child signal recv'd */
void child_process(int signo);

/* msgq_id */
int * msgq_id;

//Struct Definitions
typedef struct qNode
{
	pcb* data;
	struct qNode *next;

}QUEUE_NODE;

typedef struct
{
	QUEUE_NODE *front;
	QUEUE_NODE *rear;
	int count;
} QUEUE;

//Function Definitions (about Queue)
QUEUE *createQueue ()
{
	QUEUE *queue;
	queue = (QUEUE*)malloc(sizeof(QUEUE));
	if (queue)
	{
		queue->front = NULL;
		queue->rear = NULL;
		queue->count = 0;
	}
	return queue;
}

int enqueue (QUEUE *queue, pcb* item)
{
	QUEUE_NODE *newPtr;

	if (!(newPtr = (QUEUE_NODE*)malloc(sizeof(QUEUE_NODE))))
		return 0;

	newPtr->data = item;
	newPtr->next = NULL;

	if (queue->count == 0)
		queue->front = newPtr;
	else
		queue->rear->next = newPtr;

	(queue->count)++;
	queue->rear = newPtr;
	return 1;
}

pcb* dequeue (QUEUE *queue) //, pcb* item)
{
	QUEUE_NODE *deleteLoc;

	if (!queue->count)
		return NULL;

	pcb* item = queue->front->data;
	
	deleteLoc = queue->front;
	
	if (queue->count == 1)
		queue->rear = queue->front = NULL;
	else
		queue->front = queue->front->next;

	(queue->count)--;
	free (deleteLoc);
	
	return item;
}

int emptyQueue (QUEUE *queue)
{
	return (queue->count == 0);
}

int fullQueue (QUEUE *queue)
{
	QUEUE_NODE *temp;

	if ((temp = (QUEUE_NODE*)malloc(sizeof(QUEUE_NODE))))
	{
		free(temp);
		return 0;
	}
	return 1;
}

int queueCount (QUEUE *queue)
{
	return queue->count;
}

QUEUE *destroyQueue (QUEUE *queue)
{
	QUEUE_NODE *deletePtr;

	if (queue)
	{
		while (queue->front != NULL)
		{
			deletePtr = queue->front;
			queue->front = queue->front->next;
			free(deletePtr);
		}
		free(queue);
	}
	return NULL;
}

//Global variables
QUEUE* run_q;
QUEUE* retired_q;

pcb* current = NULL;

void time_tick(int signo) 
{
	int run_pid;
	message msg;

	if (global_tick > 600000) {
		pcb* t;
		
		//Kill all child processes in run queue and retired queue 
		while(!emptyQueue(run_q)){		
			t = dequeue(run_q);
			kill(t->pid,SIGKILL);
		}
		while(!emptyQueue(retired_q)){
			t = dequeue(retired_q);
			if(t !=NULL) kill(t->pid,SIGKILL);
		}
		kill(getpid(), SIGKILL);//Kill the parent process
		return;
	}

	/* wakeup some procs */
	/* update wait time of all procs in the waitq */
	/* decrease io_time */
	/* if wait time reaches to zero,
		proc[i] is put back into runq */

	/* invoke main scheduler */
		
		if((global_tick%10) == 0)/*If a process consumes its own time quantum,
									parent select the other process by scheduler
									time quantum = 0.1s x 10 = 1s*/
		{							
			run_pid = schedule(); //run_pid represents selected process by scheduler
			if(run_pid != -1)
			{ 		
		    	msg.mtype = 0;
			    msg.pid = run_pid;
				msg.time_quantum = 10;
			    msgsnd(*msgq_id, &msg, sizeof(msg), 0); //send the process to be excecuted message
			}
		}

	global_tick++;
	return ;
}


int main (int argc, char *argv[])
{
	run_q = createQueue(); // Create run queue
	retired_q = createQueue();// Create retired queue

	int i = 0;
	int pid = -1;
	struct sigaction old_sighandler, new_sighandler;
	struct itimerval itimer, old_timer;
	int ret;
	message msg;
	pcb* proc;
	srand(10);//change seed

	//Create message Queue
	 msgq_id = (int *)malloc(sizeof(int));
     ret = msgget(MY_MSGQ_KEY, IPC_CREAT | 0644);
     if (ret == -1) {
         printf("msgq creation error ! (p) ");
         printf("errno: %d\n", errno);
         *msgq_id = -1;
     }
     else {
         *msgq_id = ret;
         printf("msgq (key:0x%x, id:0x%x) created.\n", MY_MSGQ_KEY, ret);
     }


	for (i = 0 ; i < 10 ; i++) {
		if ((pid = fork()) > 0){//parent process
			printf("parent: pid[%d] = %d\n", i, pid);
			proc = (pcb*)malloc(sizeof(pcb));
			memset(proc, 0, sizeof(pcb)); //Initialize struct pcb to 0
			proc->pid = pid;
			proc->state = READY;
			proc->remaining_cpu_time = rand()%100;
			proc->remaining_io_time = rand()%100;
			printf("cpu time get : %d\n" , proc->remaining_cpu_time);
			enqueue(run_q, proc); // Initialize PCB of each child process to proper value, then insert it in the run queue 

		} else if (pid < 0) {
			//error!
			printf("fork failed \n");
			return 0;
		} else if (pid == 0) {
			//child
			printf("child: getpid() = %d\n", getpid());
			do_child();
			// not reach here
			return 0;
		}
	}

	// register timer signal : If parent process receive SIGALRM, then 'time_tick' function is excuted 
	memset(&new_sighandler, 0, sizeof(new_sighandler));
	new_sighandler.sa_handler = &time_tick;
	sigaction(SIGALRM, &new_sighandler, &old_sighandler);
	

	// setitimer
	itimer.it_interval.tv_sec = 0;
	itimer.it_interval.tv_usec = 100000; //100000 = 10^5 * 10^(-6) = 0.1 s = 100ms
	itimer.it_value.tv_sec = 0;
	itimer.it_value.tv_usec = 100000;
	global_tick = 0;
	setitimer(ITIMER_REAL, &itimer, &old_timer);//SIGALRM signal is sent the process by ITIMER_REAL

	while (1) {
		// endless while loop
/*		if (msgq_id > 0) {
			ret = msgrcv(*msgq_id, &msg, sizeof(msg), 0, 0);
			if (ret > 0) {
				int io_pid;
				pcb* procp;
				io_pid = msg.pid;
				while(!emptyQueue(run_q))
				{
					// change state
					procp = dequeue(run_q);
					if (procp->pid == io_pid) 
					{
						procp->state = WAIT;
						procp->remaining_io_time = 1;
						procp->remaining_cpu_time = 0;
						printf("now (%d) proc (%d) request io for (%d) ticks, and sleep \n",
								global_tick, procp->pid, procp->remaining_io_time);
					}//if
					// remove proc[i] from runquque
				}//while
			} //if
			else if (ret == 0) {
				//just skip
			}//else if 
			else {
				//printf("parent msgrcv error error: %d!\n", errno);
			}//else
		}//if*/
	}//while

	return 0;
}

/* global_var of child */
int remaining_cpu_burst=10;
int remaining_io_burst = 1;

void do_io() {

	message msg;
	int ret;
	key_t key;

	if (*msgq_id == -1) {
		ret = msgget(MY_MSGQ_KEY, IPC_CREAT | 0644);
		if (ret == -1) {
			printf("msgq creation error !  (c) ");
			printf("errno: %d\n", errno);
		}
		*msgq_id = ret;
	}

	msg.mtype = 0;
	msg.pid = getpid();
	msg.time_quantum = 0;
	msgsnd(*msgq_id, &msg, sizeof(msg), 0);
}

void child_process(int signo)
{

	remaining_cpu_burst--; //decerase remaining cpu burst by 1 when child proces receives time tick

	if (remaining_cpu_burst < 0){
		return;
		}

	return;
}

void do_child(){
	int ret;
	struct sigaction old_sighandler, new_sighandler;
	message msg;
	// register timer signal : If parent process receive the message, then 'child_process' function is excuted

	if((ret = msgrcv(*msgq_id, &msg, sizeof(msg), 0, 0))<0)
	{
		perror("msgrcv:");
		return ;
	}

	if(msg.pid==getpid()) //If the process get the message, then consume its own time quantum
	{
		memset (&new_sighandler, 0, sizeof (new_sighandler));
		new_sighandler.sa_handler = &child_process;
		sigaction(SIGALRM, &new_sighandler, &old_sighandler); 

	}

	while (1) {
	}
}

int schedule() {
	// find proc from runq, according to the rr policy
	pcb *temp,*next;
	remaining_cpu_burst = 10;

	if(current == NULL)
	{
		next = dequeue(run_q); // Select the first process from run queue
		current = next;//set cucrrent to next
	}//if
	else
	{
		current->remaining_cpu_time = current->remaining_cpu_time-10;
		if(!emptyQueue(run_q)){
			/* If there is at least one process in the run queue,
				dequeue from the run queue and set current to next.*/
			if(current->remaining_cpu_time <=0)
			{
					next = dequeue(run_q);
					printf("pid : %d , remaining cpu time : %d \n", current->pid,0);
					kill(current->pid,SIGKILL);
					current= next;
			}//If remaining cpu burst of current process is less than or equal to 0, kill the process 
			else
			{
				next = dequeue(run_q);
				printf("pid : %d , remaining cpu time : %d\n", current->pid,current->remaining_cpu_time);
				enqueue(retired_q, current);
				current = next;
			}//If remaining cpu burst of current proess is greater than 0, put the process into retired queue
		}//if	
		else //If run queue is empty
		{
			if(emptyQueue(retired_q)&&current==NULL)
			{	
				kill(getpid(),SIGKILL);//If run queue and retired queue are empty, then kill the parent process
			}
 			if(current->remaining_cpu_time<=0)
			{
				printf("pid : %d , remaining cpu time : %d \n", current->pid,0);
				kill(current->pid, SIGKILL); //If remaining cpu burst of current process is less than or equal to 0, kill the process
				current = NULL;
				if(emptyQueue(retired_q))
					kill(getpid(),SIGKILL);//If retired queue is empty, Kill the parent process
			}
			else
			{
				printf("pid : %d , remaining cpu time : %d \n", current->pid, current->remaining_cpu_time);
				enqueue(retired_q,current);
			}
			while(!emptyQueue(retired_q))//If retired queue is not empty
			 /* If runq doesn't have any other processes,
				 dequeue all pcb of processes from retired queue then put it into the run queue*/
			{
				temp = dequeue(retired_q);
				enqueue(run_q, temp);	
			}
				next = dequeue(run_q);
				current = next;
		}//else
	}//else
	if(next == NULL) return -1;
	return next->pid;
}
