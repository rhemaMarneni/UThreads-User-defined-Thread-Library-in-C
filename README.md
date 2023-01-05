# UThreads-User-defined-Thread-Library-in-C
Creating user-defined thread library for the creating, joining, deleting and scheduling threads.

**1.0 SCHEDULING POLICIES**<br>
Round Robin Scheduling (RR)<br>
A pool of threads are scheduled to run in a fair manner. In that, each thread gets to 
run for a specified interval of time, referred to as ‘time quantum’. After each interval, 
the thread is preempted out and the next scheduled thread is given execution time. 
Eventually, all threads run to completion. We chose to test with three different quanta
i.e., 25, 40, 50 milliseconds.<br><br>
3.2 Preemptive Shortest Job First Scheduling (PSJF)<br>
Preemptive Shortest Job First Scheduling allows the thread with the least burst time 
to execute first. Shortest jobs are given more priority. However, we would typically 
require foreknowledge of the burst times of each thread, which is impossible. So, we 
implement this by giving priority to the thread that has run for the least time. Jobs 
with the shortest elapsed time are given more priority.<br><br>
3.3 Multilevel Feedback Queue Scheduling (MLFQ)<br>
Schedule a job in a queue that uses a scheduling algorithm appropriate to the job and 
its behavior. It swap jobs based on their priority and performance. Can allow jobs to 
be classified and run using a scheduling algorithm that is well-tuned for that particular 
job's characteristics. We implement this by picking different time quanta. If a thread 
did not run to completion in one level, it is sent to the next level with a higher 
quantum. This continues until it completes its execution. At the highest quantum 
level, the remaining threads run on FCFS.

