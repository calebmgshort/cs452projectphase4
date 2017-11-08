#include <usloss.h>
#include <usyscall.h>
#include <stdlib.h>

#include "devices.h"
#include "phase1.h"
#include "phase2.h"
#include "phase4utility.h"
#include "phase4disk.h"

extern int debugflag4;
extern process ProcTable[];
extern int diskPIDs[];

// Pointers to the queue of disk operations
processPtr DiskDriverQueue;
processPtr NextDiskRequest;

// Disk sizes (number of tracks)
int DiskSizes[USLOSS_DISK_UNITS];

// TODO: Check syscall number in all syscall handlers
// TODO: Set to user mode in all syscall handlers
// TODO: Check that return values match spec

void diskRead(systemArgs *args)
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("diskRead(): called.\n");
    }

    // Check the syscall number
    if (args->number != SYS_DISKREAD)
    {
        USLOSS_Console("diskRead(): Called with wrong syscall number.\n");;
        USLOSS_Halt(1);
    }

    // Unpack the args
    void* memoryAddress = args->arg1;
    int numSectors = (int) ((long) args->arg2);
    int startDiskTrack = (int) ((long) args->arg3);
    int startDiskSector = (int) ((long) args->arg4);
    int unitNum = (int) ((long) args->arg5);

    // check for illegal input values
    if(numSectors < 0 || numSectors >= USLOSS_DISK_TRACK_SIZE)
    {
        if(DEBUG4 && debugflag4)
        {
            USLOSS_Console("diskRead(): invalid args.\n");
        }
        args->arg4 = (void*) -1;
        return;
    }
    else if(startDiskTrack < 0 || startDiskTrack >= DiskSizes[unitNum])
    {
        if(DEBUG4 && debugflag4)
        {
            USLOSS_Console("diskRead(): invalid args.\n");
        }
        args->arg4 = (void*) -1;
        return;
    }
    else if(startDiskSector < 0 || startDiskSector >= USLOSS_DISK_TRACK_SIZE)
    {
        if(DEBUG4 && debugflag4)
        {
            USLOSS_Console("diskRead(): invalid args.\n");
        }
        args->arg4 = (void*) -1;
        return;
    }
    else if(unitNum < 0 || unitNum >= USLOSS_DISK_UNITS)
    {
        if(DEBUG4 && debugflag4)
        {
            USLOSS_Console("diskRead(): invalid args.\n");
        }
        args->arg4 = (void*) -1;
        return;
    }
    int endingDiskTrack = startDiskSector + numSectors/USLOSS_DISK_TRACK_SIZE;
    if(endingDiskTrack >= DiskSizes[unitNum])
    {
        if(DEBUG4 && debugflag4)
        {
            USLOSS_Console("diskRead(): would have read past the end of the disk.\n");
        }
        args->arg4 = (void*) -1;
        return;
    }

    int result = diskReadReal(memoryAddress, numSectors, startDiskTrack,
                              startDiskSector, unitNum);

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

    // Put this into the disk driver queue and block
    diskQueueAdd(DISK_READ, memoryAddress, numSectorsToRead, startDiskTrack, startDiskSector, unitNumToRead);
    blockOnMbox();  // TODO: We still need to unblock this proc after it's finished on the queue
    processPtr proc = &ProcTable[getpid() % MAXPROC];
    int status = proc->diskRequest.resultStatus;
    clearProc(proc);
    return status;
}

void diskWrite(systemArgs *args)
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("diskWrite(): called.\n");
    }
    void* memoryAddress = args->arg1;
    int numSectors = (int) ((long) args->arg2);
    int startDiskTrack = (int) ((long) args->arg3);
    int startDiskSector = (int) ((long) args->arg4);
    int unitNum = (int) ((long) args->arg5);

    // check for illegal input values
    if(numSectors < 0 || numSectors >= USLOSS_DISK_TRACK_SIZE)
    {
        if(DEBUG4 && debugflag4)
        {
            USLOSS_Console("diskWrite(): invalid args.\n");
        }
        args->arg4 = (void*) -1;
        return;
    }
    else if(startDiskTrack < 0 || startDiskTrack >= DiskSizes[unitNum])
    {
        if(DEBUG4 && debugflag4)
        {
            USLOSS_Console("diskWrite(): invalid args.\n");
        }
        args->arg4 = (void*) -1;
        return;
    }
    else if(startDiskSector < 0 || startDiskSector >= USLOSS_DISK_TRACK_SIZE)
    {
        if(DEBUG4 && debugflag4)
        {
            USLOSS_Console("diskWrite(): invalid args.\n");
        }
        args->arg4 = (void*) -1;
        return;
    }
    else if(unitNum < 0 || unitNum >= USLOSS_DISK_UNITS)
    {
        if(DEBUG4 && debugflag4)
        {
            USLOSS_Console("diskWrite(): invalid args.\n");
        }
        args->arg4 = (void*) -1;
        return;
    }
    int endingDiskTrack = startDiskSector + numSectors/USLOSS_DISK_TRACK_SIZE;
    if(endingDiskTrack >= DiskSizes[unitNum])
    {
        if(DEBUG4 && debugflag4)
        {
            USLOSS_Console("diskWrite(): would have written past the end of the disk.\n");
        }
        args->arg4 = (void*) -1;
        return;
    }

    int result = diskWriteReal(memoryAddress, numSectors, startDiskTrack,
                              startDiskSector, unitNum);

    args->arg1 = (void*) ((long) result);
    args->arg4 = (void *) 0;
}

int diskWriteReal(void* memoryAddress, int numSectorsToRead, int startDiskTrack,
                 int startDiskSector, int unitNumToRead)
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("diskWriteReal(): called.\n");
    }
    // Put this into the disk driver queue and block
    diskQueueAdd(DISK_WRITE, memoryAddress, numSectorsToRead, startDiskTrack, startDiskSector, unitNumToRead);
    blockOnMbox();

    processPtr proc = &ProcTable[getpid() % MAXPROC];
    int status = proc->diskRequest.resultStatus;
    clearProc(proc);
    return status;
}

void diskSize(systemArgs *args)
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("diskSize(): called.\n");
    }

    int unit = (int) ((long) args->arg1);
    if(unit < 0 || unit >= USLOSS_DISK_UNITS)
    {
        args->arg4 = (void*) -1;
        return;
    }

    int sector = -1;
    int track = -1;
    int disk = -1;

    int result = diskSizeReal(unit, &sector, &track, &disk);

    args->arg1 = (void*) ((long) sector);
    args->arg2 = (void*) ((long) track);
    args->arg3 = (void*) ((long) disk);
    args->arg4 = (void*) ((long) result);

}

int diskSizeReal(int unit, int *sector, int *track, int *disk)
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("diskSizeReal(): called.\n");
    }

    // Check params
    if(unit < 0 || unit >= USLOSS_DISK_UNITS)
    {
        return -1;
    }

    // Set the track and sector sizes
    *track = USLOSS_DISK_TRACK_SIZE;
    *sector = USLOSS_DISK_SECTOR_SIZE;
    *disk = DiskSizes[unit];
    return 0;
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

    processPtr diskDriver = &ProcTable[diskPIDs[unit] % MAXPROC];
    unblockByMbox(diskDriver);
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

int performDiskOp(processPtr proc)
{
    diskRequest request = proc->diskRequest;

    // Seek to the given track
    int result = seekTrack(request.unit, request.startTrack);
    if (result != 0)
    {
        return result;
    }

    // Read/Write from the given track
    int currentTrack = request.startTrack;
    for (int i = 0; i < request.numSectors; i++)
    {
        // Assume sectors start from 0

        int sector = request.startSector + i;
        int overflow = sector / USLOSS_DISK_TRACK_SIZE;
        sector = sector % USLOSS_DISK_TRACK_SIZE;
        int track = request.startTrack + overflow;
        if (track != currentTrack)
        {
            result = seekTrack(request.unit, track);
            if (result != 0)
            {
                return result;
            }
            currentTrack = track;
        }

        // Send the disk a read/write request
        USLOSS_DeviceRequest uslossRequest;
        uslossRequest.opr = request.op;
        uslossRequest.reg1 = (void *) ((long) sector);
        uslossRequest.reg2 = request.memAddress + USLOSS_DISK_SECTOR_SIZE * i;
        result = USLOSS_DeviceOutput(USLOSS_DISK_DEV, request.unit, &uslossRequest);
        if (result != USLOSS_DEV_OK)
        {
            USLOSS_Console("readFromDisk(): Error in reading/writing.\n");
            USLOSS_Halt(1);
        }

        // Wait for the request to finish
        int status;
        result = waitDevice(USLOSS_DISK_DEV, request.unit, &status);
        if (result != 0)
        {
            return result;
        }

        if (status == USLOSS_DEV_ERROR)
        {
            // Inform the proc of the error
            proc->diskRequest.resultStatus = status;
            return 0;
        }
    }

    return 0;
}

int seekTrack(int unit, int track)
{
    // Send the disk a seek request
    USLOSS_DeviceRequest request;
    request.opr = USLOSS_DISK_SEEK;
    request.reg1 = (void *) ((long) track);
    int result = USLOSS_DeviceOutput(USLOSS_DISK_DEV, unit, &request);
    if (result != USLOSS_DEV_OK)
    {
        USLOSS_Console("seekTrack(): Error in seeking.\n");
        USLOSS_Halt(1);
    }

    // Wait for the seek to be finished
    int status;
    return waitDevice(USLOSS_DISK_DEV, unit, &status);
}
