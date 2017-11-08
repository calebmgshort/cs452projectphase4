#include <usloss.h>
#include <usyscall.h>
#include <stdlib.h>
#include <stdio.h>

#include "phase1.h"
#include "phase2.h"
#include "phase3.h"
#include "phase4.h"
#include "devices.h"
#include "providedPrototypes.h"
#include "phase4utility.h"
#include "phase4clock.h"
#include "phase4disk.h"
#include "phase4term.h"

// Debugging flag
int debugflag4 = 1;

// Semaphore used to create drivers
semaphore running;

// Driver process functions
static int ClockDriver(char *);
static int DiskDriver(char *);
static int TermDriver(char *);
static int TermReader(char *);
static int TermWriter(char *);

// Phase 4 proc table
process ProcTable[MAXPROC];
int diskPIDs[USLOSS_DISK_UNITS];
int termPIDs[USLOSS_TERM_UNITS];
int termReaderPIDs[USLOSS_TERM_UNITS];
int termWriterPIDs[USLOSS_TERM_UNITS];

// Driver data structures
extern int DiskSizes[];
extern termInputBuffer buffers[];
extern semaphore bufferLocks[];
extern int bufferWaitMbox[];

int start3(char *args)
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("start3(): started.\n");
    }

    char name[128];
    int clockPID;
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
        sprintf(name, "DiskDriver %d", i);
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

    // Create terminal device processes
    for (int i = 0; i < USLOSS_TERM_UNITS; i++)
    {
        // Create the driver
        if (DEBUG4 && debugflag4)
        {
            USLOSS_Console("start3(): Creating terminal device driver %d.\n", i);
        }
        sprintf(buf, "%d", i);
        sprintf(name, "TermDriver %d", i);
        int pid = fork1(name, TermDriver, buf, USLOSS_MIN_STACK, 2);
        termPIDs[i] = pid;
        if (pid < 0)
        {
            USLOSS_Console("start3(): Can't create term driver %d.\n", i);
            USLOSS_Halt(1);
        }

        // Wait for the driver to start
        sempReal(running);

        // Create the reader
        if (DEBUG4 && debugflag4)
        {
            USLOSS_Console("start3(): Creating terminal reader %d.\n", i);
        }
        sprintf(name, "TermReader %d", i);
        pid = fork1(name, TermReader, buf, USLOSS_MIN_STACK, 2);
        termReaderPIDs[i] = pid;
        if (pid < 0)
        {
            USLOSS_Console("start3(): Can't create term reader %d.\n", i);
            USLOSS_Halt(1);
        }

        // Wait for the reader to start
        sempReal(running);

        // Create the writer
        if (DEBUG4 && debugflag4)
        {
            USLOSS_Console("start3(); Creating terminal writer %d.\n", i);
        }
        sprintf(name, "TermWriter %d", i);
        pid = fork1(name, TermWriter, buf, USLOSS_MIN_STACK, 2);
        termWriterPIDs[i] = pid;
        if (pid < 0)
        {
            USLOSS_Console("start3(): Can't create term writer %d.\n", i);
            USLOSS_Halt(1);
        }
        
        // Wait for the writer to start
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

    // Zap the device drivers
    if (DEBUG4 && debugflag4)
    {
        USLOSS_Console("start3(): Zapping device drivers.\n");
    }
    zap(clockPID);
    for (int i = 0; i < USLOSS_DISK_UNITS; i++)
    {
        zap(diskPIDs[i]);
    }
    for (int i = 0; i < USLOSS_TERM_UNITS; i++)
    {
        zap(termPIDs[i]);
        zap(termReaderPIDs[i]);
        zap(termWriterPIDs[i]);
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
    if (DEBUG4 && debugflag4)
    {
        USLOSS_Console("DiskDriver(): called.\n");
    }

    // Enable interrupts and tell parent that we're running
    semvReal(running);
    enableInterrupts();

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
            if(DEBUG4 && debugflag4)
            {
                USLOSS_Console("DiskDriver(): Unblocked in the if loop.\n");
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
    if (DEBUG4 && debugflag4)
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
        // Retrieve the status register
        int status;
        int result = waitDevice(USLOSS_TERM_DEV, unit, &status);

        // Check if we can receive a character
        int recvStatus = USLOSS_TERM_STAT_RECV(status);
        if (recvStatus == USLOSS_DEV_BUSY)
        {
            // Send the character to the reader
            char character = USLOSS_TERM_STAT_CHAR(status);
            sendPrivateMessage(termReaderPIDs[unit], &character, sizeof(char));
        }
        else if (recvStatus == USLOSS_DEV_ERROR)
        {
            // An unspecified error has occured
            USLOSS_Console("TermDriver(): Error in terminal %d status register.\n", unit);
            USLOSS_Halt(1);
        }

        // Check if we can transmit a character
        int xmitStatus = USLOSS_TERM_STAT_XMIT(status);
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

static int TermReader(char *args)
{
    if (DEBUG4 && debugflag4)
    {
        USLOSS_Console("TermRead(): Called.\n");
    }

    // Enable interrupts and tell parent we're running
    enableInterrupts();
    semvReal(running);

    // Get the associated unit number
    int unit = atoi(args);

    // Initialize the buffers and semaphores
    clearBuffer(&buffers[unit]);
    bufferLocks[unit] = semcreateReal(1);
    bufferWaitMbox[unit] = MboxCreate(0, MAX_MESSAGE);

    while (!isZapped())
    {
        // Try to receive a char
        char input;
        receivePrivateMessage(&input, sizeof(char));
        storeChar(unit, input);
    }
    return 0;
}

static int TermWriter(char *args)
{
    if (DEBUG4 && debugflag4)
    {
        USLOSS_Console("TermWrite(): Called.\n");
    }

    // Enable interrupts and tell parent we're running
    enableInterrupts();
    semvReal(running);

    // Get the associated unit number
    int unit = atoi(args);
    
    return 0;
}
