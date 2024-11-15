# UThreads-User-defined-Thread-Library-in-C
Created a user-defined thread library to perform multi-threading operations - creating, yielding, joining, destroying, mutex-locking, and unlocking threads. Scheduled the threads using 3 scheduling policies: Round Robin, Pre-emptive SJF and Multilevel Feedback Queues.

# Thread Functions

### $${\color{red}THREAD\hspace{2mm}CREATION}$$
### int mypthread_create(mypthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg);

A function to create a thread. Returns 0 on success. Before creating a new
thread, a counter threadIDCounter checks if it is the first ever thread to be created
by the program. For value 0, the first thread is created and all the Thread Control
Blocks in runningQueue are set to uninitialized, status is NEW and return values are
set to NULL. The first thread will be the main thread and the threadIDCounter is
incremented. Here, the timer is also starts. When the counter increases to 1 and further,
a new TCB is created at that index of runningQueue, and a stack will be malloc’d to
the thread. This is done using makecontext(). Then the counter increment to give a
unique tid for the next thread. After the main thread exits, all the heap data must be
freed. So, cleanup() is called during atexit(). Whenever the timer has to go off
after QUANTUM amount of time (i.e., 10 milliseconds), the signal is handled by calling
swapToScheduler() function.

### $${\color{red}YIELD\hspace{2mm}THREAD}$$
### void mypthread_yield();
A function to let other threads get CPU execution. It takes no arguments. The
current thread is taken away from the processor and sent to Ready state. As a result, a
context switch occurs, where in, the context of the thread will be saved in its Thread
Control Block and changed to the scheduler context. A variable justExited is set to
0, to let the scheduler know that the last thread did not finish exiting, but just changed
its state.

### $${\color{red}THREAD\hspace{2mm}JOIN}$$
### int mypthread_join(mypthread_t thread, void **value_ptr);
A function to let the threads waiting on a calling thread, get execution time after
the calling thread finishes execution. Firstly, we check if thread exited. In case it did,
scheduler runs and justExited will be set to 0, letting the scheduler know it has not
exited, so that the threads waiting on it can be executed. If **value_ptr is not NULL,
it will be set to thread’s returnValue attribute. Its beingWaitedOnBy attribute is set
to the id of calling thread (referred to as currentThread) which will be contacted
once thread exits. Calling thread is set to WAITING status with waitingOn set to tid
of thread so that it will not be scheduled until the thread has exited. Next, scheduler
will run and justExited is set to 0.

### $${\color{red}INITIATE\hspace{2mm}MUTEX\hspace{2mm}LOCK}$$
### int mypthread_mutex_init(mypthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr);

A function to initialize a new mutex. Each mutex must have a distinct id. To
maintain that, we use a counter variable mutexIDCounter. The mutex id is set to this
value and its state is unlocked as it has not been used yet. The counter will then
increment for the next mutex created to have a unique id.

### $${\color{red}UNLOCK\hspace{2mm}MUTEX\hspace{2mm}LOCK}$$
### int mypthread_mutex_lock(mypthread_mutex_t *mutex);
A function to acquire a mutex. We use __atomic_test_and_set() for the
calling thread to keep checking the state of the mutex in a while loop. As long as its
status is LOCKED, the status will be WAITING and the waitingOnMutex attribute
indicates that it is waiting. So, it cannot be scheduled. The scheduler runs and
justExited is set to 0. If the loop breaks, that is the mutex is no longer locked, the
calling thread changes the lockState to LOCKED so that it alone can use it.

### $${\color{red}ACQUIRE\hspace{2mm}MUTEX\hspace{2mm}LOCK}$$
### int mypthread_mutex_unlock(mypthread_mutex_t *mutex);
A function to release the mutex. Firstly, set the state to UNLOCKED. Loop
over runningQueue to check if any other threads are waiting for the mutex lock. Such
threads, if any, will have their waitingOnMutex attribute indicate that they are
waiting, and be sent to READY state for the scheduler to pick them up.

### $${\color{red}DESTROY\hspace{2mm}MUTEX\hspace{2mm}LOCK}$$
### int mypthread_mutex_destroy(mypthread_mutex_t *mutex);
A function to destroy a mutex. Destroying works the same as the mutex unlock
function, in that, we change the state to UNLOCKED and check for any threads
waiting on the mutex. That itself indicates destroying the mutex as there was no
dynamic memory allocation in the process of initializing a mutex.

# Scheduling Functions

### $${\color{green}ROUND\hspace{2mm}ROBIN}$$
### static void sched_RR()
Follows the concept of Round Robin Scheduling Algorithm. Each thread is
allotted an interval of time i.e., ALLOWED_NUMBER_OF_QUANTUMS, to execute after
which, it is context-switched out for another thread that does the same. The quanta
chosen in this project is 25, 40 and 50 milliseconds. Each time, we set the time
quantum manually. We are careful to not perform context switch if the thread is
schedulable, is actively running and still has some time left to finish. If the execution
is done, set justExited to 1 and look for next thread. Otherwise, set justExited to
0 and context switch to next thread.

### $${\color{green}PREEMPTIVE\hspace{2mm}SHORTEST\hspace{2mm}JOB\hspace{2mm}FIRST}$$
### static void sched_PSJF()
To implement preemptive based SJF, foreknowledge of burst times of threads
is required. However, as that is not possible, the scheduler here gives priority to the
thread that has the least elapsed time. The thread that has executed for the least number
of quantums gets more priority. The function loops over the threads and execute them
as long as they are schedulable, and not waiting for another thread or mutex.
LowestQuant holds the value of the thread that has executed for the least amount of
time. To make it preemptive, justExited is set to 1 if the job is fully executed,
otherwise, justExited is set to 0, the thread is preempted out and the next priority
thread swaps in.

### $${\color{green}MULTILEVEL\hspace{2mm}FEEDBACK\hspace{2mm}QUEUES}$$
### static void sched_MLFQ()
Implements Multilevel Feedback Queues Scheduling Algorithm. Each round
uses a Circular Linked List with a dedicated time quantum. The threads that do not
finish execution in one level, are moved to the next queue level (with a higher
quantum) to complete their execution. We considered four quantum levels of 20, 40,
100 and 180 in our function. One function for the first three levels, and one for the last
case, where the execution ends up in FCFS policy.

# Outcome

Our code shows that MLFQ performs better as it takes lesser runtime. Round Robin and PSJF
policies almost perform similarly for these benchmarks.

<img width="610" alt="image" src="https://github.com/user-attachments/assets/2ab103a9-75a0-44c0-bba0-0d852bd74211">
