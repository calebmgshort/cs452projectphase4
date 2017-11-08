#ifndef _DEVICES_H
#define _DEVICES_H

#include "phase4.h"

#define DEBUG4 1

typedef struct process process;
typedef struct process * processPtr;

typedef struct diskRequest diskRequest;

typedef struct termLine termLine;
typedef struct termInputBuffer termInputBuffer;

typedef int semaphore;
typedef USLOSS_Sysargs systemArgs;

struct diskRequest
{
    int op;
    void *memAddress;
    int numSectors;
    int startSector;
    int startTrack;
    int unit;
    int resultStatus;
};

struct process
{
    int pid;                          // The pid of this process
    int privateMboxID;                // The id of the private mailbox used to block this process

    // Clock fields
    processPtr nextProc;              // The next pointer for the clock driver queue
    int blockStartTime;               // The time at which this process was first blocked due to sleep
    int sleepTime;                    // The amount of time this process should sleep for

    // Disk fields
    processPtr nextDiskQueueProc;     // The next proc in the disk queue
    diskRequest diskRequest;          // Holds information on the request to the disk
};

struct termLine
{
    char characters[MAXLINE];         // The characters in the line
    int indexToModify;                // The index of the first empty spot in the array.
};

struct termInputBuffer
{
    termLine lines[10];               // The 10 line buffer
    int lineToModify;                 // The line that new chars should be written to
    int lineToRead;                   // The line to read when reading from this buffer
};

#define EMPTY -1
#define DISK_READ  USLOSS_DISK_READ 
#define DISK_WRITE USLOSS_DISK_WRITE

#endif
