/*
 *  File: phase4disk.c
 *  Purpose: This file holds functions and global variables that deal with the
 *  disk devices
 */

#include <usloss.h>
#include <usyscall.h>
#include <stdlib.h>

#include "devices.h"
#include "phase1.h"
#include "phase2.h"
#include "providedPrototypes.h"
#include "phase4utility.h"
#include "phase4disk.h"

extern int debugflag4;
extern process ProcTable[];
extern int diskPIDs[USLOSS_DISK_UNITS];
extern int diskMutex[USLOSS_DISK_UNITS];

void printQueue(int);

extern semaphore diskSem[USLOSS_DISK_UNITS];

// Pointers to the queue of disk operations
processPtr DiskDriverQueue[USLOSS_DISK_UNITS];
processPtr NextDiskRequest[USLOSS_DISK_UNITS];

// Disk sizes (number of tracks)
int DiskSizes[USLOSS_DISK_UNITS];

/*
 *  System call for user function DiskRead. Serves as a bridge between DiskRead
 *  and diskReadReal
 */
void diskRead(systemArgs *args)
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("diskRead(): called.\n");
    }

    initProc();

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

    int result = diskReadReal(memoryAddress, numSectors, startDiskTrack,
                              startDiskSector, unitNum);

    if(result == -1)
    {
        args->arg4 = (void*) -1;
        args->arg1 = (void*) 0;
    }
    else
    {
        args->arg4 = (void *) 0;
        args->arg1 = (void*) ((long) result);
    }

    //clearProc(getCurrentProc());

    setToUserMode();
}

/*
 *  Reads sectors sectors from the disk indicated by unit, starting at track track
 *  and sector first. The sectors are copied into buffer. Your driver must handle
 *  a range of sectors specified by first and sectors that spans a track boundary
 *  (after reading the last sector in a track it should read the first sector in
 *  the next track). A file cannot wrap around the end of the disk.
 *  Return values:
 *    -1: invalid parameters
 *     0: sectors were read successfully >0: disk’s status register
 */
int diskReadReal(void* memoryAddress, int numSectors, int startDiskTrack,
                 int startDiskSector, int unitNum)
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("diskReadReal(): called.\n");
    }

    // check for illegal input values
    if(numSectors < 0 || numSectors >= USLOSS_DISK_TRACK_SIZE)
    {
        if(DEBUG4 && debugflag4)
        {
            USLOSS_Console("diskReadReal(): invalid args.\n");
        }
        return -1;
    }
    else if(startDiskTrack < 0 || startDiskTrack >= DiskSizes[unitNum])
    {
        if(DEBUG4 && debugflag4)
        {
            USLOSS_Console("diskReadReal(): invalid args.\n");
        }
        return -1;
    }
    else if(startDiskSector < 0 || startDiskSector >= USLOSS_DISK_TRACK_SIZE)
    {
        if(DEBUG4 && debugflag4)
        {
            USLOSS_Console("diskReadReal(): invalid args.\n");
        }
        return -1;
    }
    else if(unitNum < 0 || unitNum >= USLOSS_DISK_UNITS)
    {
        if(DEBUG4 && debugflag4)
        {
            USLOSS_Console("diskReadReal(): invalid args.\n");
        }
        return -1;
    }
    int endingDiskTrack = startDiskSector + numSectors/USLOSS_DISK_TRACK_SIZE;
    if(endingDiskTrack >= DiskSizes[unitNum])
    {
        if(DEBUG4 && debugflag4)
        {
            USLOSS_Console("diskReadReal(): would have read past the end of the disk.\n");
        }
        return -1;
    }

    // Put this into the disk driver queue and block
    diskQueueAdd(DISK_READ, memoryAddress, numSectors, startDiskTrack, startDiskSector, unitNum);
    blockOnMbox();  // TODO: We still need to unblock this proc after it's finished on the queue
    processPtr proc = &ProcTable[getpid() % MAXPROC];
    int status = proc->diskRequest.resultStatus;
    clearProc(proc);
    return status;
}

/*
 *  System call for user function DiskWrite. Serves as a bridge between DiskWrite
 *  and diskWriteReal
 */
void diskWrite(systemArgs *args)
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("diskWrite(): called.\n");
    }

    initProc();

    // Check the syscall number
    if (args->number != SYS_DISKWRITE)
    {
        USLOSS_Console("diskRead(): Called with wrong syscall number.\n");;
        USLOSS_Halt(1);
    }

    void* memoryAddress = args->arg1;
    int numSectors = (int) ((long) args->arg2);
    int startDiskTrack = (int) ((long) args->arg3);
    int startDiskSector = (int) ((long) args->arg4);
    int unitNum = (int) ((long) args->arg5);

    int result = diskWriteReal(memoryAddress, numSectors, startDiskTrack,
                              startDiskSector, unitNum);

    if(result == -1)
    {
        args->arg4 = (void*) -1;
        args->arg1 = (void*) 0;
    }
    else
    {
        args->arg4 = (void *) 0;
        args->arg1 = (void*) ((long) result);
    }

    //clearProc(getCurrentProc());

    setToUserMode();
}

/*
 *  Writes sectors sectors to the disk indicated by unit, starting at track track
 *  and sector first. The contents of the sectors are read from buffer. Like diskRead,
 *  your driver must handle a range of sectors specified by first and sectors that
 *  spans a track boundary. A file cannot wrap around the end of the disk.
 *  Return values:
 *    -1: invalid parameters
 *     0: sectors were written successfully >0: disk’s status register
 */
int diskWriteReal(void* memoryAddress, int numSectors, int startDiskTrack,
                 int startDiskSector, int unitNum)
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("diskWriteReal(): called.\n");
    }

    // check for illegal input values
    if(numSectors < 0 || numSectors >= USLOSS_DISK_TRACK_SIZE)
    {
        if(DEBUG4 && debugflag4)
        {
            USLOSS_Console("diskWriteReal(): invalid args.\n");
        }
        return -1;
    }
    else if(startDiskTrack < 0 || startDiskTrack >= DiskSizes[unitNum])
    {
        if(DEBUG4 && debugflag4)
        {
            USLOSS_Console("diskWriteReal(): invalid args.\n");
        }
        return -1;
    }
    else if(startDiskSector < 0 || startDiskSector >= USLOSS_DISK_TRACK_SIZE)
    {
        if(DEBUG4 && debugflag4)
        {
            USLOSS_Console("diskWriteReal(): invalid args.\n");
        }
        return -1;
    }
    else if(unitNum < 0 || unitNum >= USLOSS_DISK_UNITS)
    {
        if(DEBUG4 && debugflag4)
        {
            USLOSS_Console("diskWriteReal(): invalid args.\n");
        }
        return -1;
    }
    int endingDiskTrack = startDiskSector + numSectors/USLOSS_DISK_TRACK_SIZE;
    if(endingDiskTrack >= DiskSizes[unitNum])
    {
        if(DEBUG4 && debugflag4)
        {
            USLOSS_Console("diskWriteReal(): would have written past the end of the disk.\n");
        }
        return -1;
    }

    // Put this into the disk driver queue and block
    diskQueueAdd(DISK_WRITE, memoryAddress, numSectors, startDiskTrack, startDiskSector, unitNum);
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("diskWriteReal(): finished adding request to the queue.\n");
    }
    blockOnMbox();
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("diskWriteReal(): write request finished.\n");
    }
    processPtr proc = &ProcTable[getpid() % MAXPROC];
    int status = proc->diskRequest.resultStatus;
    clearProc(proc);
    return status;
}

/*
 *  System call for user function DiskSize. Serves as a bridge between DiskSize
 *  and diskSizeReal
 */
void diskSize(systemArgs *args)
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("diskSize(): called.\n");
    }

    initProc();

    // Check the syscall number
    if (args->number != SYS_DISKSIZE)
    {
        USLOSS_Console("diskRead(): Called with wrong syscall number.\n");;
        USLOSS_Halt(1);
    }

    int unit = (int) ((long) args->arg1);

    int sector = -1;
    int track = -1;
    int disk = -1;

    int result = diskSizeReal(unit, &sector, &track, &disk);

    args->arg1 = (void*) ((long) sector);
    args->arg2 = (void*) ((long) track);
    args->arg3 = (void*) ((long) disk);
    args->arg4 = (void*) ((long) result);

    //clearProc(getCurrentProc());

    setToUserMode();
}

/*
 *  Returns information about the size of the disk indicated by unit. The sector
 *  parameter is filled in with the number of bytes in a sector, track with the
 *  number of sectors in a track, and disk with the number of tracks in the disk.
 *  Return values:
 *    -1: invalid parameters
 *     0: disk size parameters returned successfully
 */
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

/*
 * Initialize the current process with a request and add it to the disk queue
 */
void diskQueueAdd(int op, void *memAddress, int numSectors, int startTrack, int startSector, int unit)
{
    getMutex(diskMutex[unit]);
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("diskQueueAdd(): called.\n");
        printQueue(unit);
    }

    processPtr proc = &ProcTable[getpid() % MAXPROC];
    diskRequest *request = &proc->diskRequest;

    request->op = op;
    request->memAddress = memAddress;
    request->numSectors = numSectors;
    request->startTrack = startTrack;
    request->startSector = startSector;
    request->unit = unit;


    if (DiskDriverQueue[unit] == NULL)
    {
        DiskDriverQueue[unit] = proc;
    }
    else if (compareRequests(&(DiskDriverQueue[unit]->diskRequest), &(proc->diskRequest)) > 0)
    {
        proc->nextDiskQueueProc = DiskDriverQueue[unit];
        DiskDriverQueue[unit] = proc;
    }
    else
    {
        processPtr current = DiskDriverQueue[unit];
        processPtr next = current->nextDiskQueueProc;
        while (next != NULL && compareRequests(&(proc->diskRequest), &(current->diskRequest)) > 0)
        {
            current = next;
            next = next->nextDiskQueueProc;
        }
        current->nextDiskQueueProc = proc;
        proc->nextDiskQueueProc = next;
    }

    if(DEBUG4 && debugflag4)
    {
        printQueue(unit);
    }
    returnMutex(diskMutex[unit]);

    semvReal(diskSem[unit]);
}

/*
 * Returns a pointer to the next disk request to process. Cleans the next
 * pointer on the returned process. Removes the returned process from the queue
 */
processPtr dequeueDiskRequest(int unit)
{
    getMutex(diskMutex[unit]);
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("diskQueueRequest(): called.\n");
        printQueue(unit);
    }

    // Return null when the queue is empty
    if (DiskDriverQueue[unit] == NULL)
    {
        returnMutex(diskMutex[unit]);
        if(DEBUG4 && debugflag4)
        {
            USLOSS_Console("diskQueueRequest(): returning NULL.\n");
        }
        return NULL;
    }

    // Initialize next request on first call
    if (NextDiskRequest[unit] == NULL)
    {
        NextDiskRequest[unit] = DiskDriverQueue[unit];
    }

    // Get the request to dequeue and update
    processPtr ret = NextDiskRequest[unit];
    if(DEBUG4 && debugflag4 && ret == NULL)
    {
        USLOSS_Console("diskQueueRequest(): ret is set to NULL.\n");
    }
    NextDiskRequest[unit] = NextDiskRequest[unit]->nextDiskQueueProc;

    // Search for the parent of the request to dequeue and remove
    if (ret != DiskDriverQueue[unit])
    {
        processPtr parent = DiskDriverQueue[unit];
        while (parent->nextDiskQueueProc != ret)
        {
            parent = parent->nextDiskQueueProc;
        }

        // Remove ret
        parent->nextDiskQueueProc = ret->nextDiskQueueProc;
    }
    else
    {
        DiskDriverQueue[unit] = ret->nextDiskQueueProc;
    }

    if (NextDiskRequest[unit] == NULL)
    {
        NextDiskRequest[unit] = DiskDriverQueue[unit];
    }

    ret->nextDiskQueueProc = NULL;

    if(DEBUG4 && debugflag4)
    {
        printQueue(unit);
    }
    returnMutex(diskMutex[unit]);

    return ret;
}

/*
 *  Perform a disk operation as defined in the request struct in processPtr proc
 */
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

/*
 *  A utility function to seek to the given track
 */
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

/*
 *  A debugging function that prints the current disk queue and next disk request
 */
void printQueue(int unit){
    USLOSS_Console("Printing the disk queue for unit %d\nQueue: ", unit);
    processPtr current = DiskDriverQueue[unit];
    while(current != NULL){
        USLOSS_Console("%d ", current->pid);
        current = current->nextDiskQueueProc;
    }
    USLOSS_Console("\t\tNext: ");
    if(NextDiskRequest[unit] != NULL)
    {
        USLOSS_Console("%d", NextDiskRequest[unit]->pid);
    }
    USLOSS_Console("\n");
}
