/*
 *  File: phase4.c
 *  Course: CSC 452, Fall 2017
 *  Authors: Taylor Heimbichner and Caleb Short
 *  Purpose: This file is phase 4 in the operating system, which handles requests
 *    to the devices. This file specifically handles the device drivers and start3,
 *    the entry and exit point to this phase.
 */

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
int debugflag4 = 0;

// Semaphore used to create drivers
semaphore running;

// Semaphore for the disks
semaphore diskSem[USLOSS_DISK_UNITS];

// Mutex for accessing the disk queue
int diskMutex[USLOSS_DISK_UNITS];

// Disk Queue stuff
extern processPtr DiskDriverQueue[USLOSS_DISK_UNITS];
extern processPtr NextDiskRequest[USLOSS_DISK_UNITS];

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
extern termInputBuffer TermReadBuffers[];
extern semaphore TermReadBufferLocks[];
extern int TermReadBufferWaitMbox[];
extern int TermWriteWaitMbox[];
extern int TermWriteMessageMbox[];


/*
 *  The entry function for phase4. Starts and closes everything at the phase4 level
 */
int start3(char *args)
{
    if (DEBUG4 && debugflag4)
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

    // Create the running semaphore
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
        sprintf(name, "DiskDriver %d", i);
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
        semvReal(diskSem[i]);
        zap(diskPIDs[i]);
    }
    for (int i = 0; i < USLOSS_TERM_UNITS; i++)
    {
        if (DEBUG4 && debugflag4)
        {
            USLOSS_Console("start3(): Zapping term driver %d.\n", i);
        }

        // Wake up the terminal, in case it's blocked on waitdevice
        awakenTerminal(i);
        
        // zap it
        zap(termPIDs[i]);


        if (DEBUG4 && debugflag4)
        {
            USLOSS_Console("start3(): Zapping term reader %d.\n", i);
        }

        // Unblock the reader, if it's waiting for a character
        char null = '\0';
        sendPrivateMessageCond(termReaderPIDs[i], &null, sizeof(char));

        // zap it
        zap(termReaderPIDs[i]);


        if (DEBUG4 && debugflag4)
        {
            USLOSS_Console("start3(): Zapping term writer %d.\n", i);
        }

        // Unblock the writer, if it's waiting for input
        MboxCondSend(TermWriteMessageMbox[i], NULL, 0);

        // zap it
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

    // Find the unit number
    int unit = atoi(arg);

    // Request the disk size and store in DiskSizes
    USLOSS_DeviceRequest request;
    request.opr = USLOSS_DISK_TRACKS;
    request.reg1 = &DiskSizes[unit];
    int result = USLOSS_DeviceOutput(USLOSS_DISK_DEV, unit, &request);
    if (result != USLOSS_DEV_OK)
    {
        USLOSS_Console("diskDriver(%d): Could not get number of tracks.\n", unit);
        USLOSS_Halt(1);
    }
    // Wait for the request to finish
    int status;
    result = waitDevice(USLOSS_DISK_DEV, unit, &status);
    if (result != 0)
    {
        USLOSS_Console("diskDriver(%d): error: result of waitDevice was not 0: %d.\n", unit, result);
    }
    if (status == USLOSS_DEV_ERROR)
    {
        USLOSS_Console("diskDriver(%d): error: the status of waitDevice is USLOSS_DEV_ERROR\n", unit);
    }

    // Initialize the disk semaphore
    diskSem[unit] = semcreateReal(0);

    // Create the mutex for the disk queue
    diskMutex[unit] = MboxCreate(1, 0);
    if(diskMutex[unit] < 0){
      USLOSS_Console("DiskDriver(%d): Failed to create the diskMutex.\n", unit);
    }
    returnMutex(diskMutex[unit]);

    // Initialize the disk queue stuff
    DiskDriverQueue[unit] = NULL;
    NextDiskRequest[unit] = NULL;

    // Enable interrupts and tell parent that we're running
    semvReal(running);
    enableInterrupts();

    while (!isZapped())
    {
        if (DEBUG4 && debugflag4)
        {
            USLOSS_Console("DiskDriver(%d): Now looking for another request to fulfill.\n", unit);
        }

        // Make sure there is something on the queue
        if (DEBUG4 && debugflag4)
        {
            USLOSS_Console("DiskDriver(%d): Calling semP on diskSem[%d]\n", unit, unit);
        }
        sempReal(diskSem[unit]);

        if(isZapped())
        {
            break;
        }

        // Dequeue a disk request
        if (DEBUG4 && debugflag4)
        {
            USLOSS_Console("DiskDriver(%d): Now dequeueing a request\n", unit);
        }
        processPtr requestProc = dequeueDiskRequest(unit);
        if (requestProc == NULL)
        {
            USLOSS_Console("DiskDriver(%d): Awoken without a request.\n", unit);
            USLOSS_Halt(1);
        }

        if(DEBUG4 && debugflag4)
        {
            USLOSS_Console("DiskDriver(%d): Performing disk operation.\n", unit);
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

    // Do an initial device output to get the terminals started
    unsigned long control = 0;
    control = USLOSS_TERM_CTRL_RECV_INT(control);
    control = USLOSS_TERM_CTRL_XMIT_INT(control);
    int result = USLOSS_DeviceOutput(USLOSS_TERM_DEV, unit, (void *) control);
    if (result != USLOSS_DEV_OK)
    {
        USLOSS_Console("TermDriver(): Failed to set control register.\n");
        USLOSS_Halt(1);
    }

    while (!isZapped())
    {
        // Retrieve the status register
        int status;
        result = waitDevice(USLOSS_TERM_DEV, unit, &status);
        if (result != 0)
        {
            return 0;
        }

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
            // Try to send a char
            unsigned long control = 0;

            // Get the char to send
            char output;
            result = receivePrivateMessageCond(&output, sizeof(char));

            if (result != -2)
            {
                // Set the character bits
                control = USLOSS_TERM_CTRL_CHAR(control, output);

                // Set the send char flag
                control = USLOSS_TERM_CTRL_XMIT_CHAR(control);

                // Set the xmit flag
                control = USLOSS_TERM_CTRL_XMIT_INT(control);
            }

            // Always set recv flag
            control = USLOSS_TERM_CTRL_RECV_INT(control);

            // Send the char
            result = USLOSS_DeviceOutput(USLOSS_TERM_DEV, unit, (void *) control);
            if (result != USLOSS_DEV_OK)
            {
                USLOSS_Console("TermDriver(): Failed to send output to terminal %d.\n", unit);
                USLOSS_Halt(1);
            }
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

/*
 *  Process that handles reading from the terminal
 */
static int TermReader(char *args)
{
    if (DEBUG4 && debugflag4)
    {
        USLOSS_Console("TermRead(): Called.\n");
    }

    // Get the associated unit number
    int unit = atoi(args);

    // Initialize the buffers and semaphores
    clearBuffer(&TermReadBuffers[unit]);
    TermReadBufferLocks[unit] = semcreateReal(1);
    TermReadBufferWaitMbox[unit] = MboxCreate(0, MAX_MESSAGE);

    // Enable interrupts and tell parent we're running
    enableInterrupts();
    semvReal(running);

    while (!isZapped())
    {
        // Try to receive a char from the driver
        char input;
        receivePrivateMessage(&input, sizeof(char));

        // Store the char in a semaphore protected buffer
        storeChar(unit, input);
    }
    return 0;
}

/*
 *  Process that handles writing to the terminal
 */
static int TermWriter(char *args)
{
    if (DEBUG4 && debugflag4)
    {
        USLOSS_Console("TermWrite(): Called.\n");
    }

    // Get the associated unit number
    int unit = atoi(args);

    // Initialize the mailboxes
    TermWriteWaitMbox[unit] = MboxCreate(0, MAX_MESSAGE);
    TermWriteMessageMbox[unit] = MboxCreate(0, MAX_MESSAGE);

    // Enable interrupts and tell parent we're running
    enableInterrupts();
    semvReal(running);

    while (!isZapped())
    {
        // A buffer for the written message
        char buffer[MAXLINE];

        // Receive a message from the mailbox
        int size = MboxReceive(TermWriteMessageMbox[unit], buffer, MAXLINE);
        if (size == -3)
        {
            // We were zapped while waiting for a message
            return 0;
        }

        for (int i = 0; i < size; i++)
        {
            if (DEBUG4 && debugflag4)
            {
                // USLOSS_Console("TermWriter(): %d: About to send character %c to driver.\n", unit, buffer[i]);
            }

            // Send chars to the driver
            sendPrivateMessage(termPIDs[unit], buffer + i, sizeof(char));

            if (DEBUG4 && debugflag4)
            {
                // USLOSS_Console("TermWriter(): %d: Send completed.\n", unit);
            }

            // Stop prematurely if we're zapped
            if (isZapped())
            {
                return 0;
            }
        }

        // Unblock a proc once we've completed the line
        if (DEBUG4 && debugflag4)
        {
            USLOSS_Console("TermWriter(): Write completed to term %d. Starting to unblock writer.\n", unit);
        }
        int result = MboxCondSend(TermWriteWaitMbox[unit], NULL, 0);
        if (result == -2)
        {
            USLOSS_Console("TermWriter(): Process that wrote to terminal was not blocked.\n");
            USLOSS_Halt(1);
        }
        else if (result == 0)
        {
            if (DEBUG4 && debugflag4)
            {
                USLOSS_Console("TermWriter(): Process unblocked.\n", unit);
            }
        }
    }

    return 0;
}
