#include <usloss.h>
#include <usyscall.h>
#include "devices.h"

extern int debugflag4;

void termRead(systemArgs *args)
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("termRead(): called.\n");
    }
}

void termReadReal()
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("termReadReal(): called.\n");
    }
}

void termWrite(systemArgs *args)
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("termWrite(): called.\n");
    }
}

void termWriteReal()
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("termWriteReal(): called.\n");
    }
}
