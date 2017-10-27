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
int debugflag4 = 1;

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
void diskReadReal();
void diskWriteReal();
void diskSizeReal();
void termReadReal();
void termWriteReal();

// Phase 4 proc table
process ProcTable[MAXPROC];
processPtr ClockDriverQueue = NULL;

int start3(char *args)
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("start3(): started");
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
    int result = USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
    if (result != USLOSS_DEV_OK)
    {
        USLOSS_Console("ClockDriver(): Bug in enable interrupts.\n");
        USLOSS_Halt(1);
    }

    // Infinite loop until we are zap'd
    int status;
    while(!isZapped())
    {
        result = waitDevice(USLOSS_CLOCK_DEV, 0, &status);
        if (result != 0)
        {
            return 0;
        }

        // Iterate over the queue and check for procs to unblock
        processPtr proc = ClockDriverQueue;
        processPtr parent = NULL;
        while (proc != NULL)
        {
            if (proc->blockStartTime == -1)
            {
                proc->blockStartTime = status;
            }
            int mcsPassed = (status - proc->blockStartTime);
            if (mcsPassed > 1000000 * proc->sleepTime)
            {
                // Enough time has elapsed; unblock proc
                proc->sleepTime = -1;
                proc->blockStartTime = -1;
                if (parent == NULL)
                {
                    ClockDriverQueue = proc->nextProc;
                }
                else
                {
                    parent->nextProc = proc->nextProc;
                }
                proc->nextProc = NULL;
                unblockByMbox(proc);
            }
            // Step through the queue
            parent = proc;
            proc = proc->nextProc;
        }
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
    semvReal(running);
    return 0; // TODO
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
}

void diskReadReal()
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("diskReadReal(): called.\n");
    }
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

void diskSizeReal()
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("diskSizeReal(): called.\n");
    }
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
