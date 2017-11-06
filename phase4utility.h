#ifndef _PHASE4UTILITY_H
#define _PHASE4UTILITY_H
#include "devices.h"

extern void checkMode(char *);
extern void enableInterrupts();
extern void setToUserMode();
extern void blockOnMbox();
extern void unblockByMbox(processPtr);
extern void clearProcRequest(processPtr);
extern void clearProc(processPtr);
extern int compareRequests(diskRequest *, diskRequest *);

#endif
