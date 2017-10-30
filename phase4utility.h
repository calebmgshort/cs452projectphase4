#ifndef _PHASE4UTILITY_H
#define _PHASE4UTILITY_H
#include "devices.h"

extern void checkMode(char *);
extern void enableInterrupts();
extern void setToUserMode();
extern void blockOnMbox();
extern void unblockByMbox(processPtr);
extern void addProcToClockQueue(processPtr);
extern processPtr nextClockQueueProc();
extern void removeClockQueueProc();
extern void checkClockQueue(int);
extern void performDiskOp(processPtr, int);
extern void diskQueueAdd(int, void *, int, int, int, int);
extern processPtr dequeueDiskRequest();
#endif
