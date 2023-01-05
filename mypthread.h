// File:	mypthread.h

// List all group members' names: Victor Sankar Ghosh, Rhema Keren Marneni
// iLab machine tested on: ilab3.cs.rutgers.edu, pwd.cs.rutgers.edu

#ifndef MYTHREAD_T_H
#define MYTHREAD_T_H

#define _GNU_SOURCE

/* To use Linux pthread Library in Benchmark, you have to comment the USE_MYTHREAD macro */
#define USE_MYTHREAD 1

/* include lib header files that you need here: */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <signal.h>
#include <string.h>
//#include <valgrind/valgrind.h>

#define STACK_SIZE SIGSTKSZ
#define QUANTUM 10000 //milliseconds
#define DEBUG 0

typedef uint mypthread_t;

/* Thread State enum definition */
enum pthread_State{
	RUNNING = 0,
	READY = 1,
	DONE = 2,
	WAITING = 3,
	NEW = 4	
};

/* Mutex State enum definition */
enum mutex_State{
	UNLOCKED = 0,
	LOCKED = 1
};

/* Scheduler Type enum definition */
typedef enum scheduler_type{
	RR = 0,
	PSJF = 1,
	MLFQ = 2
} scheduler_type;

struct itimerval timer;
ucontext_t schedContext;

/* TCB struct definition */
typedef struct threadControlBlock {	
	mypthread_t threadID;

	enum pthread_State threadStatus;

	int waitingOn;
	int beingWaitedOnBy;
	int waitingOnMutex;
	int quantumsElapsed;
	
	ucontext_t threadContext;

	void* threadStack;
	void* returnValue;	
} tcb;

tcb runningQueue[300];

/* mutex struct definition */
typedef struct mypthread_mutex_t {
	int mutexId;
	int lockState;
} mypthread_mutex_t;


/* Function Declarations: */
void swapToScheduler();
void pauseTimer();
void resumeTimer();
void cleanup();

static void sched_RR();
static void sched_PSJF();
static void sched_MLFQ();
static void schedule();

/* create a new thread */
int mypthread_create(mypthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg);

/* give CPU pocession to other user level threads voluntarily */
int mypthread_yield();

/* terminate a thread */
void mypthread_exit(void *value_ptr);

/* wait for thread termination */
int mypthread_join(mypthread_t thread, void **value_ptr);

/* initial the mutex lock */
int mypthread_mutex_init(mypthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr);

/* aquire the mutex lock */
int mypthread_mutex_lock(mypthread_mutex_t *mutex);

/* release the mutex lock */
int mypthread_mutex_unlock(mypthread_mutex_t *mutex);

/* destroy the mutex */
int mypthread_mutex_destroy(mypthread_mutex_t *mutex);

#ifdef USE_MYTHREAD 
#define pthread_t mypthread_t
#define pthread_mutex_t mypthread_mutex_t
#define pthread_create mypthread_create
#define pthread_exit mypthread_exit
#define pthread_join mypthread_join
#define pthread_mutex_init mypthread_mutex_init
#define pthread_mutex_lock mypthread_mutex_lock
#define pthread_mutex_unlock mypthread_mutex_unlock
#define pthread_mutex_destroy mypthread_mutex_destroy
#endif

#endif