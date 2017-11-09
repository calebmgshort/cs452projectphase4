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

/*
 *  Acquires the mutex with the given id
 */
void getMutex(int id)
{
    //USLOSS_Console("getMutex(): receiving from mailbox %d\n", id);
    int result = MboxReceive(id, NULL, 0);
    if(result == -1)
    {
        USLOSS_Console("getMutex(): receive called with invalid arguments\n");
    }
}

/*
 *  Returns the mutex with the given id
 */
void returnMutex(int id)
{
    //USLOSS_Console("returnMutex(): sending to mailbox %d\n", id);
    int result = MboxSend(id, NULL, 0);
    if(result == -1)
    {
        USLOSS_Console("returnMutex(): send called with invalid arguments\n");
    }
}

/*
 * Receive a message from the current proc's private mailbox.
 */
void receivePrivateMessage(void *msg, int size)
{
    processPtr proc = &ProcTable[getpid() % MAXPROC];
    MboxReceive(proc->privateMboxID, msg, size);
}

/*
 * Conditionally receive a message from the current proc's private mailbox.
 */
int receivePrivateMessageCond(void *msg, int size)
{
    processPtr proc = &ProcTable[getpid() % MAXPROC];
    return MboxCondReceive(proc->privateMboxID, msg, size);
}

/*
 * Sends a message to the mailbox corresponding to the given pid.
 */
void sendPrivateMessage(int pid, void *msg, int size)
{
    processPtr proc = &ProcTable[pid % MAXPROC];
    MboxSend(proc->privateMboxID, msg, size);
}

/*
 * Conditionally send a message to the private mailbox of the given pid.
 */
int sendPrivateMessageCond(int pid, void *msg, int size)
{
    processPtr proc = &ProcTable[pid % MAXPROC];
    return MboxCondSend(proc->privateMboxID, msg, size);
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
    proc->pid = EMPTY;
    proc->nextProc = NULL;
    proc->blockStartTime = -1;
    proc->sleepTime = -1;
    proc->nextDiskQueueProc = NULL;

    clearProcRequest(proc);
}

void initProc()
{
    clearProc(getCurrentProc());
    getCurrentProc()->pid = getpid();
}

processPtr getCurrentProc()
{
    return &ProcTable[getpid() % MAXPROC];
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
