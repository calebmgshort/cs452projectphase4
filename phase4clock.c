#include <stdlib.h>
#include <usloss.h>
#include <usyscall.h>
#include "phase1.h"
#include "phase2.h"
#include "phase4utility.h"
#include "phase4clock.h"
#include "devices.h"

extern int debugflag4;
extern process ProcTable[];

// List of processes waiting on the clock driver
processPtr ClockDriverQueue = NULL;

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

/*
 * Adds a process to the back of the ClockDriverQueue.
 */
void addProcToClockQueue(processPtr proc)
{
    if (ClockDriverQueue == NULL)
    {
        ClockDriverQueue = proc;
    }
    else
    {
        processPtr parent = ClockDriverQueue;
        while (parent->nextProc != NULL)
        {
            parent = parent->nextProc;
        }
        parent->nextProc = proc;
    }
}

/*
 * Returns the head of the ClockDriverQueue.
 */
processPtr nextClockQueueProc()
{
    return ClockDriverQueue;
}

/*
 * Removes one process from the ClockDriverQueue. If there are no processes in
 * the queue, then no action is taken.
 */
void removeClockQueueProc()
{
    if (ClockDriverQueue == NULL)
    {
        return;
    }
    ClockDriverQueue = ClockDriverQueue->nextProc;
}

void checkClockQueue(int clockStatus)
{
    // Iterate over the queue and check for procs to unblock
    processPtr proc = ClockDriverQueue;
    processPtr parent = NULL;
    while (proc != NULL)
    {
        if (proc->blockStartTime == -1)
        {
            proc->blockStartTime = clockStatus;
        }
        int mcsPassed = (clockStatus - proc->blockStartTime);
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
