// Refer to https://qnaplus.com/implement-periodic-timer-linux/ for more information

#ifndef TIME_H
#define TIME_H
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

typedef enum
{
        TIMER_SINGLE_SHOT = 0, // Single Shot Timer
        TIMER_PERIODIC         // Periodic Timer
} t_timer;

typedef void (*time_handler)(size_t timer_id, void* callbackData);

// Functions offered by the library
struct timespec diff(struct timespec start, struct timespec end); // Calculates the difference between the start and end times of the loop
int initializeRT(int policyRT, int cpuRT); // Initializes the RT thread. The user of this timer library has to call this function once before using any other function.
int initializeStandard(void); // Initializes the standard priority thread. The user of this timer library has to call this function once before using any other function.
int initialize(int policyRT, int cpuRT, bool enableRT); // Initializes the thread. The user of this timer library has to call this function once before using any other function.
int start_timer(float interval, time_handler handler, t_timer type, void * user_data); // Creates a timer and returns the id of the timer. The returned timer id would be used to stop the timer.
void stop_timer(size_t timer_id); // Stops a particular timer specified by the input timer id
void finalize(); // Stops (and deletes) all running timers.

#endif