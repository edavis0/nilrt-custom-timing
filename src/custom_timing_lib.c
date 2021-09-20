// Refer to https://qnaplus.com/implement-periodic-timer-linux/ for more information

#define _GNU_SOURCE

#include <poll.h>
#include <sys/mman.h>
#include <limits.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/resource.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <sys/timerfd.h>
#include <time.h>
#include "../include/custom_timing.h"

#define MAX_TIMER_COUNT 1000
#define handle_error_en(en, msg) do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

struct timer_node
{
    int                 fd;
    time_handler        callback;
    void*               user_data;
    float               interval;
    t_timer             type;
    struct timer_node*  next;
};

static void* _timer_thread(int* cpuPtr);
static pthread_t g_thread_id;
static struct timer_node *g_head = NULL;

// Calculates and returns the difference between the thread's end time and start time
struct timespec diff(struct timespec start, struct timespec end) {

  struct timespec temp;
  if ((end.tv_nsec-start.tv_nsec)<0) {
    temp.tv_sec = end.tv_sec-start.tv_sec-1;
    temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
  } else {
    temp.tv_sec = end.tv_sec-start.tv_sec;
    temp.tv_nsec = end.tv_nsec-start.tv_nsec;
  }
  return temp;
}

// Creates an RT thread. For more information regarding CPU scheduling: https://man7.org/linux/man-pages/man7/sched.7.html
int initializeRT(int policyRT, int cpuRT)
{
    struct sched_param  param;
    pthread_attr_t      attr;
    pthread_t           thread;
    int                 ret, t;
    
      // Lock all of the calling process's virtual address space into RAM, preventing that memory from being paged to the swap area. https://man7.org/linux/man-pages/man2/mlock.2.html
      if(mlockall(MCL_CURRENT|MCL_FUTURE) == -1) {
        printf("mlockall failed: %m\n");
        exit(-2);
      }

      // Initialize pthread attributes to default values. https://man7.org/linux/man-pages/man3/pthread_attr_init.3.html
      ret = pthread_attr_init(&attr);
      if (ret) {
        printf("init pthread attributes failed\n");
        goto out;
      }

      // Use scheduling attributes of attr for newly created thread. https://man7.org/linux/man-pages/man3/pthread_attr_setinheritsched.3.html
      ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED); // Options: PTHREAD_INHERIT_SCHED, PTHREAD_EXPLICIT_SCHED
      if (ret) {
        printf("pthread setinheritsched failed\n");
        goto out;
      }

      // Set scheduling policy of the thread. https://man7.org/linux/man-pages/man3/pthread_attr_setschedpolicy.3.html
      ret = pthread_attr_setschedpolicy(&attr, policyRT);
      if (ret) {
        printf("pthread setschedpolicy failed\n");
        goto out;
      }

      // Set scheduling priority for this thread (in FIFO, tasks have a priority between 1 (low) and 99 (high)). https://man7.org/linux/man-pages/man3/pthread_attr_setschedparam.3.html
      param.sched_priority = 99;
      ret = pthread_attr_setschedparam(&attr, &param);
      if (ret) {
        printf("pthread setschedparam failed\n");
        goto out;
      }

      // Create the thread and start execution by invoking _timer_thread with &cpu as the sole argument. https://man7.org/linux/man-pages/man3/pthread_create.3.html
      ret = pthread_create(&g_thread_id, &attr,(void*)_timer_thread, &cpuRT);
      if (ret) {
        printf("create pthread failed\n");
        goto out;
      }

    out:
      return ret;
}

// Creates a standard priority thread. For more information regarding CPU scheduling: https://man7.org/linux/man-pages/man7/sched.7.html
int initializeStandard(void)
{
    struct sched_param  param;
    pthread_attr_t      attr;
    pthread_t           thread;
    int                 ret, t;

    // Initialize pthread attributes to default values. https://man7.org/linux/man-pages/man3/pthread_attr_init.3.html
    ret = pthread_attr_init(&attr);
    if (ret) {
      printf("init pthread attributes failed\n");
      goto out;
    }

    // Create the thread and start execution by invoking _timer_thread with no argument. https://man7.org/linux/man-pages/man3/pthread_create.3.html
    ret = pthread_create(&g_thread_id, &attr,(void*)_timer_thread, NULL);
    if (ret) {
      printf("create pthread failed\n");
      goto out;
    }

    out:
      return ret;
}

// Creates and starts timers
int start_timer(float interval, time_handler handler, t_timer type, void * user_data)
{
    struct timer_node*  new_node = NULL;
    struct itimerspec   new_value;

    // Allocates memory for the timer
    new_node = (struct timer_node *)malloc(sizeof(struct timer_node));

    if(new_node == NULL) return 0;

    // Store related data in new_node
    new_node->callback  = handler;
    new_node->user_data = user_data;
    new_node->interval  = interval;
    new_node->type      = type;

    // Creates a new timer object and assigns the description to new_node
    new_node->fd = timerfd_create(CLOCK_REALTIME, 0); // Options: CLOCK_REALTIME, CLOCK_MONOTONIC, CLOCK_BOOTTIME, CLOCK_REALTIME_ALARM, CLOCK_BOOTTIME_ALARM

    if (new_node->fd == -1)
    {
        free(new_node);
        return 0;
    }

    // Sets the time out interval
    new_value.it_value.tv_sec = interval / 1000;
    new_value.it_value.tv_nsec = interval * 1000000;

    // Creates timing interval, in the case of a periodic timer
    if (type == TIMER_PERIODIC)
    {
      new_value.it_interval.tv_sec= interval / 1000;
      new_value.it_interval.tv_nsec = interval * 1000000;
    }
    else
    {
      new_value.it_interval.tv_sec= 0;
      new_value.it_interval.tv_nsec = 0;
    }

    // Starts the timer
    timerfd_settime(new_node->fd, 0, &new_value, NULL);

    // Inserting the timer node into the list
    new_node->next = g_head;
    g_head = new_node;

    return (size_t)new_node;
}

// Stops the timer and removes timer data structure
void stop_timer(size_t timer_id)
{
    struct timer_node * tmp = NULL;
    struct timer_node * node = (struct timer_node *)timer_id;

    if (node == NULL) return;

    if(node == g_head)
    {
      g_head = g_head->next;
    } else {

      tmp = g_head;

      while(tmp && tmp->next != node) tmp = tmp->next;

      if(tmp)
      {
          /*tmp->next can not be NULL here*/
          tmp->next = tmp->next->next;
          close(node->fd);
          free(node);
      }
    }
}

// Stops all running timers and then stops the thread
void finalize()
{
    while(g_head) stop_timer((size_t)g_head);

    pthread_cancel(g_thread_id);
    pthread_join(g_thread_id, NULL);
}

// Obtains a reference to an in use timer from a file descriptor
struct timer_node * _get_timer_from_fd(int fd)
{
    struct timer_node * tmp = g_head;

    while(tmp)
    {
        if(tmp->fd == fd) return tmp;

        tmp = tmp->next;
    }
    return NULL;
}

// Checks if any timer's file descriptor is set and calls the timer's callback function if so
void* _timer_thread(int* cpuPtr)
{
    struct pollfd ufds[MAX_TIMER_COUNT] = {{0}};
    int iMaxCount = 0;
    struct timer_node * tmp = NULL;
    int read_fds = 0, i, s;
    uint64_t exp;
    
    // Set and get CPU core affinity if RT is enabled. https://man7.org/linux/man-pages/man3/CPU_SET.3.html
    if (cpuPtr != NULL) {
      cpu_set_t cpuset;
      pthread_t thread;
      thread = pthread_self();
      CPU_ZERO(&cpuset);
      CPU_SET(*cpuPtr, &cpuset);

      // Set the CPU affinity. https://man7.org/linux/man-pages/man3/pthread_setaffinity_np.3.html
      int ret = pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset);
      if (ret != 0) {
        printf("Set affinity failed\n");
        handle_error_en(s, "pthread_setaffinity_np");
      }

      // Check the CPU affinity mask assigned to the thread
      ret = pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
      if (ret != 0) {
        handle_error_en(s, "pthread_getaffinity_np");
      }

      // Print the CPU affinity to the console
      printf("CPU affinity mask: ");
      for (int j = 0; j < CPU_SETSIZE; j++) {
        if (CPU_ISSET(j, &cpuset)) {
          printf("CPU %d  ", j);
        } 
      }
    }

    // Get CPU core affinity if thread is standard priority
    else if (cpuPtr == NULL) {
      cpu_set_t cpuset;
      pthread_t thread;
      thread = pthread_self();

      // Check the CPU affinity mask assigned to the thread
      int ret = pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
      if (ret != 0) {
        handle_error_en(s, "pthread_getaffinity_np");
      }

      // Print the CPU affinity to the console
      printf("CPU affinity mask: ");
      for (int j = 0; j < CPU_SETSIZE; j++) {
        if (CPU_ISSET(j, &cpuset)) {
          printf("CPU %d  ", j);
        } 
      }
    }   
    
    while(1)
    {
			pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
			pthread_testcancel();
			pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

			iMaxCount = 0;
			tmp = g_head;

			memset(ufds, 0, sizeof(struct pollfd)*MAX_TIMER_COUNT);
			while(tmp)
			{
				ufds[iMaxCount].fd = tmp->fd;
				ufds[iMaxCount].events = POLLIN;
				iMaxCount++;

				tmp = tmp->next;
			}
			read_fds = poll(ufds, iMaxCount, 100);

			if (read_fds <= 0) continue;

			for (i = 0; i < iMaxCount; i++)
			{

				if (ufds[i].revents & POLLIN)
				{
					s = read(ufds[i].fd, &exp, sizeof(uint64_t));

					if (s != sizeof(uint64_t)) continue;

					tmp = _get_timer_from_fd(ufds[i].fd);

					if(tmp && tmp->callback) tmp->callback((size_t)tmp, tmp->user_data);
				}
			}
    }

    return NULL;
}