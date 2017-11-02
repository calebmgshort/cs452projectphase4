#ifndef _PHASE4CLOCK_H
#define _PHASE4CLOCK_H

#include "devices.h"

extern void sleep(systemArgs *);
extern int sleepReal(int);
extern void addProcToClockQueue(processPtr);
extern processPtr nextClockQueueProc();
extern void removeClockQueueProc();
extern void checkClockQueue(int);

#endif
