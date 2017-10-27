#ifndef _DEVICES_H
#define _DEVICES_H

#define DEBUG4 1

typedef struct process process;
typedef struct process * processPtr;

typedef int semaphore;

struct process
{
    int pid;                          // The pid of this process
    int privateMboxID;                // The id of the private mailbox used to block this process
    processPtr nextProc;              // The next pointer for the clock driver queue
    int blockStartTime;               // The time at which this process was first blocked due to sleep
    int sleepTime;                    // The amount of time this process should sleep for
};

#define EMPTY -1;

#endif
