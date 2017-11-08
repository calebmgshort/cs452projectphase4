#include <usloss.h>
#include <usyscall.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase4.h>
#include <stdlib.h>
#include <stdio.h>

#include "phase1.h"
#include "phase2.h"
#include "devices.h"
#include "providedPrototypes.h"
#include "phase4utility.h"
#include "phase4clock.h"
#include "phase4disk.h"
#include "phase4term.h"

// Debugging flag
int debugflag4 = 0;

// Semaphore used to create drivers
semaphore running;

// Driver process functions
static int ClockDriver(char *);
static int DiskDriver(char *);
static int TermDriver(char *);

// Phase 4 proc table
process ProcTable[MAXPROC];
int diskPIDs[USLOSS_DISK_UNITS];

extern int DiskSizes[];

int start3(char *args)
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("start3(): started.\n");
    }

    char name[128];
    int clockPID;
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
        processPtr proc = &ProcTable[i % MAXPROC];
        clearProc(proc);
        proc->privateMboxID = MboxCreate(0, MAX_MESSAGE);
    }

    running = semcreateReal(0);

    // Create clock device driver
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
            USLOSS_Console("start3(): Can't create disk driver %d\n", i);
            USLOSS_Halt(1);
        }

        // Wait for the driver to start
        sempReal(running);
    }

    // May be other stuff to do here before going on to terminal drivers
    /*
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
    */
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

    // Zap the device drivers
    if (DEBUG4 && debugflag4)
    {
        USLOSS_Console("start3(): Zapping device drivers.\n");
    }
    zap(clockPID);
    for (int i = 0; i < USLOSS_DISK_UNITS; i++)
    {
        unblockProc(diskPIDs[i]);
        zap(diskPIDs[i]);
    }
    /*
    USLOSS_Console("start3(): Now zapping terms.\n");
    for (int i = 0; i < USLOSS_TERM_UNITS; i++)
    {
        if (0)
            USLOSS_Console("%d", termPIDs[i]);
        zap(termPIDs[i]);
    }
    */
    //USLOSS_Console("start3(): Finished zapping.\n");

    // Quit
    quit(0);
    return 0;
}

/*
 * Entry function for the clock driver process.
 */
static int ClockDriver(char *arg)
{
    if (DEBUG4 && debugflag4)
    {
        USLOSS_Console("ClockDriver(): called.\n");
    }

    // Let the parent know we are running and enable interrupts.
    semvReal(running);
    enableInterrupts();

    // Infinite loop until we are zap'd
    int status;
    while (!isZapped())
    {
        int result = waitDevice(USLOSS_CLOCK_DEV, 0, &status);
        if (result != 0)
        {
            // We were zapped while waiting
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

    // Find the unit number
    int unit = atoi(arg);

    // Request the disk size and store in DiskSizes
    USLOSS_DeviceRequest request;
    request.opr = USLOSS_DISK_TRACKS;
    request.reg1 = &DiskSizes[unit];
    int result = USLOSS_DeviceOutput(USLOSS_DISK_DEV, unit, &request);
    if (result != USLOSS_DEV_OK)
    {
        USLOSS_Console("diskSizeReal(): Could not get number of tracks.\n");
        USLOSS_Halt(1);
    }
    // Wait for the request to finish
    int status;
    result = waitDevice(USLOSS_DISK_DEV, unit, &status);
    if (result != 0)
    {
        USLOSS_Console("diskSizeReal(): error: result of waitDevice was not 0: %d.\n", result);
    }
    if (status == USLOSS_DEV_ERROR)
    {
        USLOSS_Console("diskSizeReal(): error: the status of waitDevice is USLOSS_DEV_ERROR");
    }


    // Enable interrupts and tell parent that we're running
    semvReal(running);
    enableInterrupts();

    while (!isZapped())
    {
        // Dequeue a disk request
        processPtr requestProc = dequeueDiskRequest();
        if (requestProc == NULL)
        {
            if(DEBUG4 && debugflag4)
            {
                USLOSS_Console("DiskDriver(): There is nothing to do, blocking.\n");
            }
            // Block and wait for a request to come in
            blockOnMbox();
            if(isZapped())
            {
                return 0;
            }

            // Once we've awoken, a request should exist
            requestProc = dequeueDiskRequest();
            if (requestProc == NULL)
            {
                USLOSS_Console("DiskDriver(): Awoken without a request.\n");
                USLOSS_Halt(1);
            }
        }

        // Perform the request
        performDiskOp(requestProc);

        // Unblock the process that requested the disk operation
        unblockByMbox(requestProc);
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

    // Enable interrupts and tell parent that we're running
    semvReal(running);
    enableInterrupts();

    // Get the unit number associated with this driver
    int unit = atoi(arg);

    while (!isZapped())
    {
        int status;
        int result = waitDevice(USLOSS_TERM_DEV, unit, &status);

        int recvStatus; // TODO unpack from status
        if (recvStatus == USLOSS_DEV_BUSY)
        {
            char character; // TODO unpack from status
            // TODO receive the char
        }
        else if (recvStatus == USLOSS_DEV_ERROR)
        {
            // An unspecified error has occured
            USLOSS_Console("TermDriver(): Error in terminal %d status register.\n", unit);
            USLOSS_Halt(1);
        }

        int xmitStatus; // TODO unpack from status
        if (xmitStatus == USLOSS_DEV_READY)
        {
            // Send a char
            unsigned int control = 0;
            // TODO set the character bits
            // TODO set the send char flag
            // TODO set the recv int enable flag
            // TODO determine if we should set the xmit int enable flag
        }
        else if (xmitStatus == USLOSS_DEV_ERROR)
        {
            // An unspecified error has occured
            USLOSS_Console("TermDriver(): Error in terminal %d status register.\n", unit);
            USLOSS_Halt(1);
        }
    }

    return 0;
}
