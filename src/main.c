#define _GNU_SOURCE

#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/mman.h>
#include <time.h>
#include <assert.h>
#include "../include/custom_timing.h"

void time_handler1(size_t timer_id, void* callbackData);
void PrintData(void);

extern unsigned int sleep(unsigned int __seconds);

/******************************/
/* Enter values for constants */
/******************************/
const int policyRT = SCHED_FIFO; // Sets the scheduling policy attribute of the RT enabled thread. Options: SCHED_FIFO, SCHED_RR, and SCHED_OTHER
const int cpuRT = 1; // Sets the CPU core affinity for the RT enabled thread. In a 2 core cRIO the options are 0 and 1
const double cycleTime = 500; // Cycle time in Hz
const int runSec = 15; // Sets the time window for the loops to be run within
const double upperLimit = 1.1; // Sets the upper limit of the cycle time to evaluate the maximum time
const double lowerLimit = 0.9; // Sets the lower limit of the cycle time to evaluate the minimum time

// Initialize global variables
char 		errBuff[2048] = { '\0' };
int 		timerCount;
double 		totalTime, avgTime, maxTime, minTime;
struct 		timespec prevTime = {0}, startTime = {0};

int main() {

	int 		i, timerRT, timerStandard;
	double		actualCycleTime, expectedTimePerLoop = 1/cycleTime*1000;

	// Print input parameters to console
	printf("\n********* Input Parameters *********\n");
	if (policyRT == 0) {
		printf("RT thread policy: Other\n");
	}
	else if (policyRT == 1){
		printf("RT thread policy: FIFO\n");
	}
	else if (policyRT == 2){
		printf("RT thread policy: Round-robin\n");
	}
	printf("RT thread CPU core: %d\n", cpuRT);
	printf("Cycle time: %5.2f Hz\n", cycleTime);
	printf("Run time per thread: %d sec\n", runSec);

	// **************************************************
	// RT priority loop thread
	// **************************************************
	// Initialize the timer module with RT priority
	if (initializeRT(policyRT, cpuRT)) {
		printf("create pthread failed\n");
		return 1;
	}

	// Assign initial values to previously initialized variables
	minTime = 10000;
	maxTime = 0;
	totalTime = 0;
	timerCount= 0;

	// Get current value of clock CLOCK_MONOTONIC and stores it in StartTime
	clock_gettime(CLOCK_MONOTONIC, &startTime);

	// Create a timer and assigns its file descriptor to timer1
	printf("\n********* RT Priority Thread *********\n");
	timerRT = start_timer(expectedTimePerLoop, time_handler1, TIMER_PERIODIC, NULL);

	// Wait before stopping the timer
	sleep(runSec);

	// Stop the timer, delete it, and print timer data to the console
	stop_timer(timerRT);
	finalize();
	PrintData();
	
	// **************************************************
	// Standard priority thread
	// **************************************************
	// Initialize the timer module with standard priority
	if (initializeStandard()) {
		printf("create pthread failed\n");
		return 1;
	}

	// Reassign initial values to previously initialized variables
	minTime = 10000;
	maxTime = 0;
	totalTime = 0;
	timerCount= 0;

	// Get current value of clock CLOCK_MONOTONIC and stores it in StartTime
	clock_gettime(CLOCK_MONOTONIC, &startTime);

	// Creates a timer and assigns its file descriptor to timer1
	printf("********* Standard Priority Thread *********\n");
	timerStandard = start_timer(expectedTimePerLoop, time_handler1, TIMER_PERIODIC, NULL);

	// Wait before stopping the timer
	sleep(runSec);

	// Stop the timer, delete it, and print timer data to the console
	stop_timer(timerStandard);
	finalize();
	PrintData();

	return 0;
}

// Creates a timer and calculates its stats for minimum, maximum, and average time
void time_handler1(size_t timer_id, void* callbackData) {

	struct 		timespec currentTime, tempTimeUsed;
	double 		timeUsed;
	double		expectedTimePerLoop = 1/cycleTime*1000;
	double 		upperInterval = upperLimit*expectedTimePerLoop;
	double 		lowerInterval = lowerLimit*expectedTimePerLoop;

	// Get current value of clock CLOCK_MONOTONIC and stores it in currentTime
	clock_gettime(CLOCK_MONOTONIC, &currentTime);
	if (timerCount == 0) {
		prevTime = startTime;
		printf("\n");
	}

	// Calculate the time used
	tempTimeUsed = diff(prevTime, currentTime);
	timeUsed = tempTimeUsed.tv_sec + (double) tempTimeUsed.tv_nsec / 1000000.0;
	prevTime = currentTime;

	if (timerCount > 1) {

		// Calculate average time
		totalTime = totalTime + timeUsed;
		avgTime = totalTime / timerCount;

		// Save the minimum time and print to console
		if (timeUsed < minTime) {
			minTime = timeUsed;
		}

		// Save the maximum time and print to console 
		if (timeUsed > maxTime){
			maxTime = timeUsed;
		}
	}
	timerCount++;
}

// Prints timer data to the console
void PrintData(void) {

	float actualCycleTime;
	printf("Minimum Time: %2.3f ms\n", minTime);
	printf("Maximum Time: %2.3f ms\n", maxTime);
	printf("Average Time: %2.3f ms\n", avgTime);
	actualCycleTime = (float)(timerCount + 1) / runSec;
	printf("Frequency: %5.3f Hz\n\n", actualCycleTime);

}
