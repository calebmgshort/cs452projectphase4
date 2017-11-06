#ifndef _PHASE4DISK_H
#define _PHASE4DISK_H

#include "devices.h"

extern void diskRead(systemArgs *);
extern void diskWrite(systemArgs *);
extern void diskSize(systemArgs *);

extern int diskReadReal(void *, int, int, int, int);
extern int diskWriteReal(void *, int, int, int, int);
extern int diskSizeReal(int, int *, int *, int *);

extern int performDiskOp(processPtr);
extern void diskQueueAdd(int, void*, int, int, int, int);
extern processPtr dequeueDiskRequest();
extern int seekTrack(int, int);

#endif
