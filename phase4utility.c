#include <usloss.h>
#include <usyscall.h>
#include <stdlib.h>
#include "phase1.h"
#include "phase2.h"
#include "phase4utility.h"
#include "devices.h"

extern process ProcTable[];

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
void enableInterrupts()
{
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
    MboxCondSend(proc->privateMboxID, NULL, 0);
}

void clearProcRequest(processPtr proc)
{
    proc->diskRequest.op = EMPTY;
    proc->diskRequest.memAddress = NULL;
    proc->diskRequest.numSectors = EMPTY;
    proc->diskRequest.startTrack = EMPTY;
    proc->diskRequest.startSector = EMPTY;
    proc->diskRequest.unit = EMPTY;
    proc->diskRequest.resultStatus = 0;
}

void clearProc(processPtr proc)
{
    
}

int compareRequests(diskRequest *req1, diskRequest *req2)
{
    if (req1->startTrack > req2->startTrack)
    {
        return 1;
    }
    else if (req2->startTrack > req1->startTrack)
    {
        return -1;
    }
    else if (req1->startSector > req2->startSector)
    {
        return 1;
    }
    else if (req2->startSector > req1->startSector)
    {
        return -1;
    }
    return 0;
}
