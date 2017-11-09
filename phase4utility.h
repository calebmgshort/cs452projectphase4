#ifndef _PHASE4UTILITY_H
#define _PHASE4UTILITY_H
#include "devices.h"

extern void checkMode(char *);
extern void enableInterrupts();
extern void setToUserMode();
extern void blockOnMbox();
extern void unblockByMbox(processPtr);
extern void getMutex(int);
extern void returnMutex(int);
extern void clearProcRequest(processPtr);
extern void clearProc(processPtr);
extern void initProc();
extern processPtr getCurrentProc();
extern int compareRequests(diskRequest *, diskRequest *);
extern void sendPrivateMessage(int, void *, int);
extern void receivePrivateMessage(void *, int);
extern int receivePrivateMessageCond(void *, int);
#endif
