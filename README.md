# nilrt-custom-timing

## Overview
This repository contains an example program that demonstrates the timing performance of a thread given real-time priority to a thread given standard priority. Additionally, the core affinity of the RT thread is set in the program. The RT thread functionality is primarily achieved by utilizing the pthread.h, time.h, and mman.h header files. The custom_timing_lib .c and .h files were built based on Srikanta's [post][1] in QnA Plus.

## Code Output
When executed, the program displays the input parameters, performance of the RT thread, and performance of the standard priority thread.
  
![Console output](https://github.com/edavis0/nilrt-custom-timing/blob/main/ConsoleOutImage.png)

## Key Functions Utilized
In the custom_timing_lib.c library file, two functions are utilized to initialize the threads: *initializeRT* and *initializeStandard*. The *initializeRT* function creates a thread and sets the thread attributes, such as the policy and priority. The *initializeStandard* function creates a thread with standard priority. When the threads are created using *pthread_create*, the *_timer_thread* function is called. If it is an RT thread, the CPU affinity is set within *_timer_thread*. The *_timer_thread* function will return the CPU affinity mask to the console using *pthread_getaffinity_np*.

In main.c, the threads are created, measured, and stopped using the following programming flow:
1. Initalize the thread using *initializeRT* or *initializeStandard*.
2. Start the timer using *start_timer*.
3. Sleep for a pre-defined amount of time using *sleep*.
4. Stop the timer using *stop_timer*.
5. Cancel the thread using *finalize*.
6. Print data to the console using *PrintData*.

## Building for NI Linux Real-Time OS
To compile the source code on your host machine, you must install the GNU C/C++ Compile Tools for [x64 Linux][2] or [ARMv7 Linux][3]. Make sure to include the CMakeLists.txt file and .vscode directories when building the binary (included in /build and /.vscode, respectively).

It is recommended you first learn how to cross-compile code and deploy to the NI Linux RTOS using Microsoft VSCode by visiting this [NI Forum Post][4].

[1]: https://qnaplus.com/implement-periodic-timer-linux/ "How to Implement Periodic Timer in Linux? post"
[2]: https://www.ni.com/en-us/support/downloads/software-products/download.gnu-c---c---compile-tools-x64.html#338442 "x64 Linux Toolchain download" 
[3]: https://www.ni.com/en-us/support/downloads/software-products/download.gnu-c---c---compile-tools-for-armv7.html#338448 "ARMv7 Linux Toolchain download"
[4]: https://forums.ni.com/t5/NI-Linux-Real-Time-Documents/NI-Linux-Real-Time-Cross-Compiling-Using-the-NI-Linux-Real-Time/ta-p/4026449 "NI forum post"
