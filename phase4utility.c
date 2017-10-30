#include <usloss.h>
#include <usyscall.h>
#include <stdlib.h>
#include "phase1.h"
#include "phase2.h"
#include "phase4utility.h"
#include "devices.h"

extern process ProcTable[];
extern processPtr ClockDriverQueue;
extern processPtr DiskDriverQueue;
extern processPtr NextDiskRequest;

int compareRequests(diskRequest *, diskRequest *);

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

void diskQueueAdd(int op, void *memAddress, int numSectors, int startTrack, int startSector, int unit)
{
    processPtr proc = &ProcTable[getpid() % MAXPROC];
    diskRequest *request = &proc->diskRequest;

    request->op = op;
    request->memAddress = memAddress;
    request->numSectors = numSectors;
    request->startTrack = startTrack;
    request->startSector = startSector;
    request->unit = unit;

    if (DiskDriverQueue == NULL)
    {
        DiskDriverQueue = proc;
    }
    else if (compareRequests(&(DiskDriverQueue->diskRequest), &(proc->diskRequest)) > 0)
    {
        proc->nextDiskQueueProc = DiskDriverQueue;
        DiskDriverQueue = proc;
    }
    else
    {
        processPtr current = DiskDriverQueue;
        processPtr next = current->nextDiskQueueProc;
        while (next != NULL && compareRequests(&(proc->diskRequest), &(current->diskRequest)) > 0)
        {
            current = next;
            next = next->nextDiskQueueProc;
        }
        current->nextDiskQueueProc = proc;
        proc->nextDiskQueueProc = next;
    }
}

void clearProcRequest(processPtr proc)
{
    proc->diskRequest.op = EMPTY;
    proc->diskRequest.memAddress = NULL;
    proc->diskRequest.numSectors = EMPTY;
    proc->diskRequest.startTrack = EMPTY;
    proc->diskRequest.startSector = EMPTY;
    proc->diskRequest.unit = EMPTY;
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


/*
 * Returns a pointer to the next disk request to process. Cleans the next
 * pointer on the returned process. Removes the returned process from the queue
 */
processPtr dequeueDiskRequest()
{
    // Return null when the queue is empty
    if (DiskDriverQueue == NULL)
    {
        return NULL;
    }

    // Initialize next request on first call
    if (NextDiskRequest == NULL)
    {
        NextDiskRequest = DiskDriverQueue;
    }

    // Get the request to dequeue and update
    processPtr ret = NextDiskRequest;
    NextDiskRequest = NextDiskRequest->nextDiskQueueProc;
    if (NextDiskRequest == NULL)
    {
        NextDiskRequest = DiskDriverQueue;
    }

    // Search for the parent of the request to dequeue and remove
    if (ret != DiskDriverQueue)
    {
        processPtr parent = DiskDriverQueue;
        while (parent->nextDiskQueueProc != ret)
        {
            parent = parent->nextDiskQueueProc;
        }

        // Remove ret
        parent->nextDiskQueueProc = ret->nextDiskQueueProc;
    }
    else
    {
        DiskDriverQueue = ret->nextDiskQueueProc;
    }

    ret->nextDiskQueueProc = NULL;
    return ret;
}

void performDiskOp(processPtr proc, int opType)
{
    diskRequest request = proc->diskRequest;

    // Seek to the given track
    USLOSS_DeviceRequest uslossRequest;
    uslossRequest.opr = USLOSS_DISK_SEEK;
    uslossRequest.reg1 = (void *) ((long) request.startTrack);
    int result = USLOSS_DeviceOutput(USLOSS_DISK_DEV, request.unit, &uslossRequest);
    if (result != USLOSS_DEV_OK)
    {
        USLOSS_Console("readFromDisk(): Error in seeking.\n");
        USLOSS_Halt(1);
    }

    // Read from the given track
    for (int i = 0; i < request.numSectors; i++)
    {
        if (request.startSector + i > USLOSS_DISK_TRACK_SIZE)
        {
            USLOSS_Console("Error: sector overflow.\n");
        }
        uslossRequest.opr = opType;
        uslossRequest.reg1 = (void *) ((long) request.startSector + i);
        uslossRequest.reg2 = request.memAddress + USLOSS_DISK_SECTOR_SIZE * i;
        result = USLOSS_DeviceOutput(USLOSS_DISK_DEV, request.unit, &uslossRequest);
        if (result != USLOSS_DEV_OK)
        {
            USLOSS_Console("readFromDisk(): Error in reading/writing.\n");
            USLOSS_Halt(1);
        }
    }
}
