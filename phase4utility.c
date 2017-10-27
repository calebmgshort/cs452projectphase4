#include <usloss.h>
#include <usyscall.h>
#include <stdlib.h>
#include "phase1.h"
#include "phase2.h"
#include "phase4utility.h"
#include "devices.h"

extern process ProcTable[];
extern processPtr ClockDriverQueue;

/*
 * Checks the current mode of the current status and halts if the process is in
 * user mode.
 */
void checkMode(char *funcName)
{
    unsigned int psr = USLOSS_PsrGet();
    unsigned int kernelMode = psr & USLOSS_PSR_CURRENT_MODE;
    if (!kernelMode)
    {
        USLOSS_Console("%s(): Called from user mode.  Halting...\n", funcName);
        USLOSS_Halt(1);
    }
}

/*
 * Enable interrupts
 */
int enableInterrupts(){
    int result = USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
    if (result != USLOSS_DEV_OK)
    {
        USLOSS_Console("ClockDriver(): Bug in enable interrupts.\n");
        USLOSS_Halt(1);
    }
}

/*
 * Sets the current process into user mode. Requires the process to currently
 * be in kernel mode. Also enables interrupts.
 */
void setToUserMode()
{
    unsigned int psr = USLOSS_PsrGet();
    if (!(psr & USLOSS_PSR_CURRENT_MODE))
    {
        USLOSS_Console("setToUserMode(): Called from user mode.  Halting...\n");
        USLOSS_Halt(1);
    }
    unsigned int newpsr = (psr & ~USLOSS_PSR_CURRENT_MODE) | USLOSS_PSR_CURRENT_INT;
    int result = USLOSS_PsrSet(newpsr);
    if (result != USLOSS_DEV_OK)
    {
        USLOSS_Console("setToUserMode(): Bug in psr set.  Halting...\n");
        USLOSS_Halt(1);
    }
}

/*
 * Blocks the current process by receiving from its private mailbox.
 */
void blockOnMbox()
{
    processPtr proc = &ProcTable[getpid() % MAXPROC];
    MboxReceive(proc->privateMboxID, NULL, 0);
}

/*
 * Unblocks the given process by sending to its private mailbox.
 */
void unblockByMbox(processPtr proc)
{
    MboxSend(proc->privateMboxID, NULL, 0);
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
