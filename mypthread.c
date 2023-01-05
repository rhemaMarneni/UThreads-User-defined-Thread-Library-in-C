// File:	mypthread.c

// Group members: Victor Sankar Ghosh, , Rhema Keren Marneni
// iLab machine tested on: ilab3.cs.rutgers.edu, pwd.cs.rutgers.edu
#include <stdio.h>
#include "mypthread.h"

#define dbgF(...) if(DEBUG) printf(__VA_ARGS__);

mypthread_t threadIDCounter = 0;
mypthread_t currentThread;

int mutexIDCounter = 0;
int justExited = 0;
int lastExited = -1;

static scheduler_type sched = MLFQ;

// MLFQ defs begin
#define LEVELS 4
const int LEVEL_QUANTUM[] = {20, 40, 100, 180};
// const int LEVELS = sizeof(LEVEL_QUANTUM) / sizeof(LEVEL_QUANTUM[0]);;

typedef struct node {
  mypthread_t id;
  struct node* next;
} queue_node;

typedef struct { // circular linked list.
  struct node* tail;
  unsigned int size;
} queue;

typedef struct {
  queue qs[LEVELS];
  queue fcfs;
  tcb* last_picked;
  int last_picked_level;
} Mlfq;

static Mlfq mlfq;

static void init_MLFQ(Mlfq* mlfq);
void enqueue(queue* q, mypthread_t tid);
// MLFQ defs end

/* create a new thread */
int mypthread_create(mypthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg) {
	dbgF("Creating a new thread\n");
    if(threadIDCounter == 0){
		
    	// setup array of threads
    	int i = 0;
    	for(i = 0; i < 300; i++){
    		runningQueue[i].threadID = -5;
    		runningQueue[i].threadStatus = NEW;
    		runningQueue[i].waitingOn = -5;
    		runningQueue[i].beingWaitedOnBy = -5;
    		runningQueue[i].waitingOnMutex = -5;
    		runningQueue[i].quantumsElapsed = -5;
			runningQueue[i].returnValue = NULL;
    	}

		atexit(cleanup);

    	// initialize main thread
    	runningQueue[0].threadID = threadIDCounter;
    	runningQueue[0].threadStatus = READY;
    	runningQueue[0].waitingOn = -1;
    	runningQueue[0].beingWaitedOnBy = -1;
    	runningQueue[0].waitingOnMutex = -1;
    	runningQueue[0].quantumsElapsed = 0;
		runningQueue[0].returnValue = NULL;
    	
    	currentThread = threadIDCounter;
    	
    	threadIDCounter++;
		
		// setup timer for schedule
		struct sigaction sa;
		memset (&sa, 0, sizeof (sa));
		sa.sa_handler = &swapToScheduler;  /* which function to call when timer signal happens */
		sigaction (SIGPROF, &sa, NULL);
		// resetting the timer for each ms
		timer.it_interval.tv_usec = QUANTUM; //1000 microsecs = 1 ms
		timer.it_interval.tv_sec = 0;
		// setup current timer
		timer.it_value.tv_usec = QUANTUM;
		timer.it_value.tv_sec = 0;
		setitimer(ITIMER_PROF, &timer, NULL);

		init_MLFQ(&mlfq);
    	enqueue(&mlfq.qs[0], 0);
    }
    
    runningQueue[threadIDCounter].threadID = threadIDCounter;
    runningQueue[threadIDCounter].threadStatus = READY;
    runningQueue[threadIDCounter].waitingOn = -1;
    runningQueue[threadIDCounter].beingWaitedOnBy = -1;
    runningQueue[threadIDCounter].waitingOnMutex = -1;
    runningQueue[threadIDCounter].quantumsElapsed = 0;
	runningQueue[threadIDCounter].returnValue = NULL;    
    runningQueue[threadIDCounter].threadStack = malloc(STACK_SIZE);	
    
    getcontext(&runningQueue[threadIDCounter].threadContext);

    runningQueue[threadIDCounter].threadContext.uc_stack.ss_sp = runningQueue[threadIDCounter].threadStack;
    runningQueue[threadIDCounter].threadContext.uc_stack.ss_size = STACK_SIZE;
    runningQueue[threadIDCounter].threadContext.uc_stack.ss_flags = 0;
    runningQueue[threadIDCounter].threadContext.uc_link = NULL;
    makecontext(&runningQueue[threadIDCounter].threadContext, (void*)function, 1, arg);
    dbgF("made thread %d\n", threadIDCounter);
	enqueue(&mlfq.qs[0], threadIDCounter);
    *thread = threadIDCounter;
    threadIDCounter++;
    if(threadIDCounter == 2){
    	getcontext(&runningQueue[0].threadContext);
    }

    return 0;
};

/* give CPU possession to other user-level threads voluntarily */
int mypthread_yield() {

	// change thread state from Running to Ready
	// save context of this thread to its thread control block
	// switch from thread context to scheduler context
	justExited = 0;
	schedule();
	
	return 0;
};

/* terminate a thread */
void mypthread_exit(void *value_ptr) {
	// Deallocated any dynamic memory created when starting this thread
	pauseTimer();
	runningQueue[currentThread].threadStatus = DONE;
	if(runningQueue[currentThread].beingWaitedOnBy != -1){
		runningQueue[runningQueue[currentThread].beingWaitedOnBy].waitingOn = -1;
		runningQueue[runningQueue[currentThread].beingWaitedOnBy].threadStatus = READY;
		dbgF("thread %d exit, %d can now proceed\n", currentThread, runningQueue[currentThread].beingWaitedOnBy);
	}
	if(value_ptr != NULL){ // return value stored in tcb
		runningQueue[currentThread].returnValue = value_ptr;
	}
	if(DEBUG) printf("thread %d exit\n", currentThread);
	justExited = 1;
	resumeTimer();
	schedule();
};


/* Wait for thread termination */
int mypthread_join(mypthread_t thread, void **value_ptr) {
	// wait for a specific thread to terminate
	// de-allocate any dynamic memory created by the joining thread
	if(runningQueue[thread].threadStatus == DONE) {
		justExited = 0;
		schedule();
		return 0;
	}
	if(value_ptr != NULL){
		value_ptr = runningQueue[thread].returnValue;
	}
	runningQueue[thread].beingWaitedOnBy = currentThread;
	runningQueue[currentThread].threadStatus = WAITING;
	runningQueue[currentThread].waitingOn = thread;
	justExited = 0;
	if(DEBUG) printf("thread %d waiting on %d\n", currentThread, thread);
	schedule();
	return 0;
};

/* initialize the mutex lock */
int mypthread_mutex_init(mypthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr) {
	//initialize data structures for this mutex
	*mutex = (mypthread_mutex_t){
		.mutexId = mutexIDCounter,
		.lockState = UNLOCKED,
	};
	mutexIDCounter++;
	return 0;
};

/* aquire the mutex lock */
int mypthread_mutex_lock(mypthread_mutex_t *mutex) {
  	  while(__atomic_test_and_set((volatile void*)&mutex->lockState, __ATOMIC_RELAXED)){ //loop until mutex no longer locked
       	runningQueue[currentThread].threadStatus = WAITING;
       	runningQueue[currentThread].waitingOnMutex = mutex->mutexId;
        justExited = 0;
        schedule(); //choose something else to do
      }
      //made it, time to lock it for personal use  
      if(mutex->lockState == UNLOCKED) mutex->lockState = LOCKED;
      return 0;
};

/* release the mutex lock */
int mypthread_mutex_unlock(mypthread_mutex_t *mutex) {
	mutex->lockState = UNLOCKED;
	int i = 0;
	while(runningQueue[i].threadStatus != NEW){
		if(runningQueue[i].waitingOnMutex == mutex->mutexId){
			runningQueue[i].waitingOnMutex = -1;
			runningQueue[i].threadStatus = READY;
		}
		i++;
	}
	return 0;
};

/* destroy the mutex */
int mypthread_mutex_destroy(mypthread_mutex_t *mutex) {
	// Deallocate dynamic memory created in mypthread_mutex_init
	mutex->lockState = UNLOCKED;
	int i = 0;
	while(runningQueue[i].threadStatus != NEW){
		if(runningQueue[i].waitingOnMutex == mutex->mutexId){
			runningQueue[i].waitingOnMutex = -1;
			runningQueue[i].threadStatus = READY;
		}
		i++;
	}
	return 0;
};

/* scheduler */
static void schedule() {
	// Every time when timer interrup happens, your thread library should be contexted switched from thread context to this schedule function
	// Invoke different actual scheduling algorithms according to policy (PSJF or RR or MLFQ)

	pauseTimer();

	if (sched == PSJF)
			sched_PSJF();
	else if (sched == RR)
			sched_RR();
	else if (sched == MLFQ)
			sched_MLFQ();

	// schedule policy
}

/* --- Thread Scheduling Helper Functions --- */
void swapToScheduler(){
	pauseTimer();
	runningQueue[currentThread].quantumsElapsed = runningQueue[currentThread].quantumsElapsed + 1;
	runningQueue[currentThread].threadStatus = READY;
	justExited = 0;
	schedule();
}

void pauseTimer(){
	struct itimerval zero_timer = {0};
	setitimer(ITIMER_PROF, &zero_timer, NULL);
}

void resumeTimer(){
	setitimer(ITIMER_PROF, &timer, NULL);
}

void cleanup(){
	int i = 1;
	while(runningQueue[i].threadStatus == DONE){
		free(runningQueue[i].threadStack);
		i++;
	}
}

/* --- Scheduler Helper Functions ---*/
/* Fn. to check if thread is scheduleable */
int isScheduleable(tcb *t){
	return t->waitingOn == -1 && t->waitingOnMutex == -1 && t->threadStatus != DONE && t->threadStatus != WAITING;
}

/* Debugger Fn. to check for thread scheduler state */
char state(mypthread_t tid) {
  return isScheduleable(&runningQueue[tid]) ? 'S' : 'X';
}

/* Fn. to switch to the next thread if current thread has completed its execution */
void set_next_thread(mypthread_t tid) {
  dbgF("Scheduler to go from thread %d(%dms)%c to %d(%dms)%c\n", currentThread, runningQueue[currentThread].quantumsElapsed, state(tid), tid, runningQueue[tid].quantumsElapsed, state(tid));
  currentThread = tid;
  resumeTimer();
  setcontext(&runningQueue[tid].threadContext);
}

/* Fn. to context switch to the next scheduleable thread */
void swap_thread(mypthread_t x, mypthread_t y) {
  dbgF("Scheduler to swap from thread %d(%dms)%c to %d(%dms)%c\n", x, runningQueue[x].quantumsElapsed, state(x), y, runningQueue[y].quantumsElapsed, state(y));
  currentThread = y;
  resumeTimer();
  swapcontext(&runningQueue[x].threadContext, &runningQueue[y].threadContext);
}

/* Round-Robin Helper Fn. to select next scheduleable thread */
mypthread_t RR_find_next_thread_id(mypthread_t tid) {
  const int MAX_THREADS = sizeof(runningQueue) / sizeof(runningQueue[0]);

  for (mypthread_t i = tid + 1; i < 2 * MAX_THREADS; ++i) {
    unsigned int j = i % MAX_THREADS;
    if (runningQueue[j].threadStatus == NEW) {
      continue;
    }
    if (isScheduleable(&runningQueue[j])) {
      dbgF("Next chosen : %d(%dms)%c\n", j, runningQueue[j].quantumsElapsed, state(j));
      return j;
    }
  }
  dbgF("Instance Beyond MAX_THREADS \n");
  return -1;

}

/* --- Scheduler Policy Functions --- */
/* Round Robin scheduling algorithm */
static void sched_RR() {
  const int ALLOWED_NUMBER_OF_QUANTUMS = 40;

  int currentThreadLifetime = runningQueue[currentThread].quantumsElapsed;
  int currentThreadJustStarted = currentThreadLifetime == 0;
  int yetToFinishQuantum = (currentThreadLifetime % ALLOWED_NUMBER_OF_QUANTUMS) != 0;
  int currentThreadIsOk = justExited == 0 && isScheduleable(&runningQueue[currentThread]) && (currentThreadJustStarted || yetToFinishQuantum);
  if (currentThreadIsOk) {
    dbgF("Not switching. Thread %d elapsed at time %d\n", currentThread, currentThreadLifetime);
    resumeTimer();
    return;
  }
  
  mypthread_t oldRunner = currentThread;
  mypthread_t nextThreadToRun = RR_find_next_thread_id(currentThread);  
  if (justExited == 1) {
    dbgF("tid=%d exited \n", currentThread);
    set_next_thread(nextThreadToRun);
    return;
  }
  if (justExited == 0) {
    swap_thread(oldRunner, nextThreadToRun);
  }
}

/* Preemptive PSJF (STCF) scheduling algorithm */
static void sched_PSJF() {
  int i = 0;
  int lowestQuants = 69420;
  int nextThreadToRun = -1;
  int oldRunner = currentThread;
  while (runningQueue[i].threadStatus != NEW) {
    if (isScheduleable(&runningQueue[i]) && runningQueue[i].quantumsElapsed < lowestQuants) {
      nextThreadToRun = i;
      lowestQuants = runningQueue[i].quantumsElapsed;
    }
    i++;
  }
  if (justExited == 1 && lowestQuants != 69420) {
    set_next_thread(nextThreadToRun);
  } else if (justExited == 0 && lowestQuants != 69420) {
    swap_thread(oldRunner, nextThreadToRun);
  }
}

/* --- Utility Functions for MLFQ --- */
/* --- Queue Functions for MLFQ   --- */
// Enqueue Function to push thread into queue
void enqueue(queue* q, mypthread_t tid) {
  queue_node* node = (queue_node*) malloc(sizeof(queue_node));
  node->id = tid;
  q->size++;
  if (q->tail == NULL) {
    node->next = node;
    q->tail = node;
    return;
  }
  node->next = q->tail->next;
  q->tail->next = node;
  q->tail = node;
}

// Dequeue thread from queue
mypthread_t dequeue(queue* q) {
  q->size--;
  queue_node* head = q->tail->next;
  mypthread_t detached_value = head->id;
  if (head == q->tail) {
    q->tail = NULL;
  } else {
    q->tail->next = head->next;
  }
  free(head);
  return detached_value;
}

// Push a thread from one level queue to the next level queue
mypthread_t dequeue_next_schedulable(queue* q) {
  unsigned int current_size = q->size;
  for (int attempt = 0; attempt < current_size; ++attempt) {
    mypthread_t tid = dequeue(q);
    if (isScheduleable(&runningQueue[tid])) {
      return tid;
    }
    enqueue(q, tid);
  }
  return -1;
}

/* --- MLFQ Helper Functions --- */
// Initializes queue for MLFQ 
static void init_MLFQ(Mlfq* mlfq) {
  for (int i = 0; i < LEVELS; ++i) {
    mlfq->qs[i].size = 0;
    mlfq->qs[i].tail = NULL;
  }
  mlfq->fcfs.size = 0;
  mlfq->fcfs.tail = NULL;
}

// Initializes last picked thread and its queue level
static void pickThreadFromMlfq(int level, mypthread_t next_tid) {
  mlfq.last_picked = &runningQueue[next_tid];
  mlfq.last_picked_level = 0 <= level && level < LEVELS ? level : -1;
}

// Fn. to check if the queue level has at least one scheduleable thread
static int hasAtLeastOneScheduleable(queue* q) {
  queue_node* p = q->tail->next;
  for (int i = 0; i < q->size; ++i) {
    if (isScheduleable(&runningQueue[p->id])) {
      return 1;
    }
    p = p->next;
  }
  return 0;
}

// Fn. to find a scheduleable queue level
static int findAvailableQueueLevel() {
  for (int i = 0; i < LEVELS; ++i) {
    if (mlfq.qs[i].size > 0 && hasAtLeastOneScheduleable(&mlfq.qs[i])) {
      return i;
    }
  }
  return -1;
}

/* Preemptive MLFQ scheduling algorithm */
// Fn for the last level in MLFQ i.e. FCFS
static void sched_MLFQ_FCFS() {
  int hasAThread = mlfq.last_picked != NULL;
  int existingThreadFinished = hasAThread && justExited == 1;
  int existingThreadNotFinished = hasAThread && justExited == 0;
  int existingThreadBlocked = existingThreadNotFinished && !isScheduleable(mlfq.last_picked);
  int existingThreadRunning = existingThreadNotFinished && isScheduleable(mlfq.last_picked);

  if (existingThreadRunning) {
    resumeTimer();
    return;
  }

  mypthread_t next_tid = dequeue_next_schedulable(&mlfq.fcfs);

  dbgF("Next tid=%d at l=%d\n", next_tid, -1);
  if (next_tid == -1) {
    dbgF("Skipping FCFS\n")
    resumeTimer();
    return;
  }

  if (!hasAThread) {
    enqueue(&mlfq.qs[0], currentThread);
    pickThreadFromMlfq(-1, next_tid);
    resumeTimer();
    swap_thread(currentThread, next_tid);
    return;
  }

  if (existingThreadFinished) {
    pickThreadFromMlfq(-1, next_tid);
    resumeTimer();
    set_next_thread(next_tid);
    return;
  }

  if (existingThreadBlocked) {
    enqueue(&mlfq.fcfs, mlfq.last_picked->threadID);
    pickThreadFromMlfq(-1, next_tid);
    resumeTimer();
    swap_thread(currentThread, next_tid);
    return;
  }  
  enqueue(&mlfq.fcfs, next_tid);
  resumeTimer();
}

// Fn for regular MLFQ, that is, move thread from lower level queue to higher level queue
static void sched_MLFQ_regular(int level) {
  int hasAThread = mlfq.last_picked != NULL;
  int existingThreadFinished = hasAThread && justExited == 1;
  int existingThreadNotFinished = hasAThread && justExited == 0;
  int existingThreadBlocked = existingThreadNotFinished && !isScheduleable(mlfq.last_picked);
  int existingThreadRunning = existingThreadNotFinished && isScheduleable(mlfq.last_picked);
  int existingThreadExhaustedQuota = hasAThread &&  mlfq.last_picked->quantumsElapsed > LEVEL_QUANTUM[mlfq.last_picked_level >= 0 ? mlfq.last_picked_level : 0];

  if (existingThreadRunning && !existingThreadExhaustedQuota) {
    resumeTimer();
    return;
  }
  mypthread_t next_tid = dequeue_next_schedulable(&mlfq.qs[level]);

  dbgF("Next tid=%d at l=%d\n", next_tid, level);
  if (next_tid == -1) {
    dbgF("Skipping level=%d\n", level)
    resumeTimer();
    return;
  }
  if (!hasAThread) {
    enqueue(&mlfq.qs[0], currentThread);
    pickThreadFromMlfq(level, next_tid);
    resumeTimer();
    swap_thread(currentThread, next_tid);
    return;
  }

  if (existingThreadFinished) {
    pickThreadFromMlfq(level, next_tid);
    resumeTimer();
    set_next_thread(next_tid);
    return;
  }

  if (existingThreadBlocked && !existingThreadExhaustedQuota) {
    queue* nextQ = &mlfq.qs[mlfq.last_picked_level];
    enqueue(nextQ, mlfq.last_picked->threadID);
    pickThreadFromMlfq(level, next_tid);
    resumeTimer();
    swap_thread(currentThread, next_tid);
    return;
  }

  if (existingThreadBlocked && existingThreadExhaustedQuota) {
    queue* nextQ = mlfq.last_picked_level < LEVELS ? &mlfq.qs[mlfq.last_picked_level + 1] : &mlfq.fcfs;
    enqueue(nextQ, mlfq.last_picked->threadID);
    pickThreadFromMlfq(level, next_tid);
    resumeTimer();
    swap_thread(currentThread, next_tid);
    return;
  }

  enqueue(&mlfq.qs[level], next_tid);
  resumeTimer();
}

// Main MLFQ Function
static void sched_MLFQ() {
  int firstNonEmptyLevel = findAvailableQueueLevel();
  if (firstNonEmptyLevel == -1) {
    sched_MLFQ_FCFS();
    return;
  }
  sched_MLFQ_regular(firstNonEmptyLevel);
}