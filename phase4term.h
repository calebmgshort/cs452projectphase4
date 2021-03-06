#ifndef _PHASE4TERM_H
#define _PHASE4TERM_H

#include "devices.h"

extern void termRead(systemArgs *);
extern void termWrite(systemArgs *);

extern int termReadReal(int, int, char *);
extern int termWriteReal(int, int, char *);

extern void clearBuffer(termInputBuffer *);
extern void storeChar(int, char);
extern void awakenTerminal(int);
#endif
