#include <usloss.h>
#include <usyscall.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase4.h>
#include <stdlib.h>
#include <stdio.h>

#include "devices.h"
#include "providedPrototypes.h"
#include "phase4utility.h"

// Debugging flag
int debugflag4 = 0;

// Semaphore used to create drivers
semaphore running;

// Driver process functions
static int ClockDriver(char *);
static int DiskDriver(char *);
static int TermDriver(char *);

// Syscall handler prototypes
void sleep(systemArgs *);
void diskRead();
void diskWrite();
void diskSize();
void termRead();
void termWrite();

int sleepReal(int);
int diskReadReal(void*, int, int, int, int);
void diskWriteReal();
void diskSizeReal();
void termReadReal();
void termWriteReal();

// Phase 4 proc table
process ProcTable[MAXPROC];

// Driver queues
processPtr ClockDriverQueue = NULL;
processPtr DiskDriverQueue = NULL;

// Disk sizes
int DiskSize[USLOSS_DISK_UNITS];

int start3(char *args)
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("start3(): started.\n");
    }

    char name[128];
    // char termbuf[10];
    int clockPID;
    int diskPIDs[USLOSS_DISK_UNITS];
    int termPIDs[USLOSS_TERM_UNITS];
    int	status;
    char buf[10];

    // Check kernel mode
    checkMode("start3");

    // Initialize the system call vector
    if (DEBUG4 && debugflag4)
    {
        USLOSS_Console("start3(): Initializing syscall vector.\n");
    }
    systemCallVec[SYS_SLEEP] = sleep;
    systemCallVec[SYS_DISKREAD] = diskRead;
    systemCallVec[SYS_DISKWRITE] = diskWrite;
    systemCallVec[SYS_DISKSIZE] = diskSize;
    systemCallVec[SYS_TERMREAD] = termRead;
    systemCallVec[SYS_TERMWRITE] = termWrite;

    // Initialize the ProcTable
    if (DEBUG4 && debugflag4)
    {
        USLOSS_Console("start3(): Initializing ProcTable.\n");
    }
    for (int i = 0; i < MAXPROC; i++)
    {
        ProcTable[i].pid = EMPTY;
        ProcTable[i].privateMboxID = MboxCreate(0, MAX_MESSAGE);
        ProcTable[i].nextProc = NULL;
        ProcTable[i].blockStartTime = -1;
        ProcTable[i].sleepTime = -1;
    }

    // Create clock device driver
    running = semcreateReal(0);
    if (DEBUG4 && debugflag4)
    {
        USLOSS_Console("start3(): Creating clock device driver.\n");
    }
    clockPID = fork1("Clock driver", ClockDriver, NULL, USLOSS_MIN_STACK, 2);
    if (clockPID < 0)
    {
        USLOSS_Console("start3(): Can't create clock driver\n");
        USLOSS_Halt(1);
    }

    // Wait for the clock driver to start. ClockDriver will V running once it starts.
    sempReal(running);

    // Create the disk device drivers
    for (int i = 0; i < USLOSS_DISK_UNITS; i++)
    {
        if (DEBUG4 && debugflag4)
        {
            USLOSS_Console("start3(): Creating disk device driver %d.\n", i);
        }
        sprintf(buf, "%d", i);
        int pid = fork1(name, DiskDriver, buf, USLOSS_MIN_STACK, 2);
        diskPIDs[i] = pid;
        if (pid < 0)
        {
            USLOSS_Console("start3(): Can't create term driver %d\n", i);
            USLOSS_Halt(1);
        }

        // Wait for the driver to start
        sempReal(running);
    }

    // May be other stuff to do here before going on to terminal drivers

    // Create terminal device drivers
    for (int i = 0; i < USLOSS_TERM_UNITS; i++)
    {
        if (DEBUG4 && debugflag4)
        {
            USLOSS_Console("start3(): Creating terminal device driver %d.\n", i);
        }
        sprintf(buf, "%d", i);
        int pid = fork1(name, TermDriver, buf, USLOSS_MIN_STACK, 2);
        termPIDs[i] = pid;
        if (pid < 0)
        {
            USLOSS_Console("start3(): Can't create term driver %d\n", i);
            USLOSS_Halt(1);
        }

        // Wait for the driver to start
        sempReal(running);
    }

    // Create first user-level process and wait for it to finish.
    if (DEBUG4 && debugflag4)
    {
        USLOSS_Console("start3(): Spawning start4.\n");
    }
    int pid = spawnReal("start4", start4, NULL, 4 * USLOSS_MIN_STACK, 3);
    if (pid < 0)
    {
        USLOSS_Console("start3(): Can't create start4.\n");
        USLOSS_Halt(1);
    }
    pid = waitReal(&status);

    for (int i = 0; i < 6; i++)
    {
        waitReal(&status);
    }

    // Zap the device drivers
    if (DEBUG4 && debugflag4)
    {
        USLOSS_Console("start3(): Zapping device drivers.\n");
    }
    zap(clockPID);
    for (int i = 0; i < USLOSS_DISK_UNITS; i++)
    {
        if (0)
            USLOSS_Console("%d", diskPIDs[i]);
        // zap(diskPIDs[i]);
    }
    for (int i = 0; i < USLOSS_TERM_UNITS; i++)
    {
        if (0)
            USLOSS_Console("%d", termPIDs[i]);
        // zap(termPIDs[i]);
    }

    // Quit
    quit(0);
    return 0;
}

/*
 * Entry function for the clock driver process.
 */
static int ClockDriver(char *arg)
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("ClockDriver(): called.\n");
    }
    // Let the parent know we are running and enable interrupts.
    semvReal(running);
    enableInterrupts();

    // Infinite loop until we are zap'd
    int status;
    while(!isZapped())
    {
        int result = waitDevice(USLOSS_CLOCK_DEV, 0, &status);
        if (result != 0)
        {
            return 0;
        }

        // Check the queue and unblock procs
        checkClockQueue(status);
    }
    return 0;
}

/*
 * Entry function for the disk driver process.
 */
static int DiskDriver(char *arg)
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("DiskDriver(): called.\n");
    }

    // Enable interrupts and tell parent that we're running
    semvReal(running);
    enableInterrupts();

    // Find the size of t
    int unit = atoi(arg);

    // Request the disk size
    USLOSS_DeviceRequest request;
    request.opr = USLOSS_DISK_TRACKS;
    request.reg1 = &DiskSizes[unit];
    int result = USLOSS_DeviceOutput(USLOSS_DISK_DEV, unit, &request);
    if (result != USLOSS_DEV_OK)
    {
        USLOSS_Console("diskSizeReal(): Could not get number of tracks.\n");
        USLOSS_Halt(1);
    }

    while (!isZapped())
    {
    }
    return 0;
}

/*
 * Entry function for the term driver process.
 */
static int TermDriver(char *arg)
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("TermDriver(): called.\n");
    }
    semvReal(running);
    return 0; // TODO
}

void sleep(systemArgs *args)
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("sleep(): called.\n");
    }
    // Unpack args
    int secs = (int) ((long) args->arg1);

    // Defer to sleepReal
    long result = sleepReal(secs);

    // Put return values in args
    args->arg4 = (void *) result;

    // Set to user mode
    setToUserMode();
}

int sleepReal(int secs)
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("sleepReal(): called.\n");
    }
    // Check args
    if (secs < 0)
    {
        return -1;
    }

    // Put an entry in the clock driver queue
    addProcToClockQueue(&ProcTable[getpid() % MAXPROC]);

    // Set the sleep time in the process table
    ProcTable[getpid() % MAXPROC].sleepTime = secs;

    if (DEBUG4 && debugflag4)
    {
        USLOSS_Console("sleepReal(): About to block process %d.\n", getpid());
    }

    // Block this process
    blockOnMbox();

    // The clock driver will unblock us when appropriate

    // Return the result
    return 0;
}

void diskRead(systemArgs *args)
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("diskRead(): called.\n");
    }
    void* memoryAddress = args->arg1;
    int numSectorsToRead = (int) ((long) args->arg2);
    int startDiskTrack = (int) ((long) args->arg3);
    int startDiskSector = (int) ((long) args->arg4);
    int unitNumToRead = (int) ((long) args->arg5);

    // check for illegal input values
    if(numSectorsToRead < 0 || numSectorsToRead >= DiskSizes[unitNumToRead])
    {
        args->arg4 = (void*) -1;
        return;
    }
    else if(startDiskTrack < 0 || startDiskTrack >= DiskSizes[unitNumToRead])
    {
        args->arg4 = (void*) -1;
        return;
    }
    else if(startDiskSector < 0 || startDiskSector >= USLOSS_DISK_TRACK_SIZE)
    {
        args->arg4 = (void*) -1;
        return;
    }
    else if(unitNumToRead < 0 || unitNumToRead > USLOSS_DISK_UNITS)
    {
        args->arg4 = (void*) -1;
        return;
    }

    int result = diskReadReal(memoryAddress, numSectorsToRead, startDiskTrack,
                              startDiskSector, unitNumToRead);

    args->arg1 = (void*) ((long) result);
    args->arg4 = (void *) 0;
}

int diskReadReal(void* memoryAddress, int numSectorsToRead, int startDiskTrack,
                 int startDiskSector, int unitNumToRead)
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("diskReadReal(): called.\n");
    }

    // TODO: put this into the disk driver queue and block
    diskQueueAdd(DISK_READ, memoryAddress, numSectorsToRead, startKistTrack, startDiskSector, unitNumToRead); 
    blockOnMbox();
    return 0;
}

void diskWrite(systemArgs *args)
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("diskWrite(): called.\n");
    }
}

void diskWriteReal()
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("diskWriteReal(): called.\n");
    }
}

void diskSize(systemArgs *args)
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("diskSize(): called.\n");
    }
}

int diskSizeReal(int unit, int *sector, int *track, int *disk)
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("diskSizeReal(): called.\n");
    }

    // Check params
    if (unit != 0 && unit != 1)
    {
        return -1;
    }

    // Set the track and sector sizes
    *track = USLOSS_DISK_TRACK_SIZE;
    *sector = USLOSS_DISK_SECTOR_SIZE;
    *disk = DiskSizes[unit];
    return 0;
}

void termRead(systemArgs *args)
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("termRead(): called.\n");
    }
}

void termReadReal()
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("termReadReal(): called.\n");
    }
}

void termWrite(systemArgs *args)
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("termWrite(): called.\n");
    }
}

void termWriteReal()
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("termWriteReal(): called.\n");
    }
}
