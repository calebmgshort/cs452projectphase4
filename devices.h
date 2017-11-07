#ifndef _DEVICES_H
#define _DEVICES_H

#define DEBUG4 1

typedef struct process process;
typedef struct process * processPtr;

typedef struct diskRequest diskRequest;

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

#define EMPTY -1
#define DISK_READ  USLOSS_DISK_READ
#define DISK_WRITE USLOSS_DISK_WRITE
#define TRUE 1
#define FALSE 0

#endif
