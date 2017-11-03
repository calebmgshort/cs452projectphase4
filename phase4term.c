#include <usloss.h>
#include <usyscall.h>
#include "devices.h"
#include "phase4utility.h"
#include "phase4term.h"

extern int debugflag4;

void termRead(systemArgs *args)
{
    if (DEBUG4 && debugflag4)
    {
        USLOSS_Console("termRead(): called.\n");
    }

    // Check the syscall number
    if (args->number != SYS_TERMREAD)
    {
        USLOSS_Console("termRead(): Called with wrong syscall number.\n");
        USLOSS_Halt(1);
    }

    // Extract args
    int unit = (int) ((long) args->arg3);
    int size = (int) ((long) args->arg2);
    char *buffer = (char *) args->arg1;
    
    // Defer to termReadReal
    int result = termReadReal(unit, size, buffer);
    
    // Pack up return values
    if (result == -1)
    {
        args->arg2 = (void *) 0;
        args->arg4 = (void *) -1;
    }
    else
    {
        args->arg2 = (void *) ((long) result);
        args->arg4 = (void *) 0;
    }

    // Set to user mode
    setToUserMode();
}

int termReadReal(int unit, int size, char *buffer)
{
    if (DEBUG4 && debugflag4)
    {
        USLOSS_Console("termReadReal(): called.\n");
    }
    return -1;
}

void termWrite(systemArgs *args)
{
    if (DEBUG4 && debugflag4)
    {
        USLOSS_Console("termWrite(): called.\n");
    }

    // Check the syscall number
    if (args->number != SYS_TERMWRITE)
    {
        USLOSS_Console("termWrite(): Called with wrong syscall number.\n");
        USLOSS_Halt(1);
    }

    // Extract args
    int unit = (int) ((long) args->arg3);
    int size = (int) ((long) args->arg2);
    char *text = (char *) args->arg1;

    // Defer to termWriteReal
    int result = termWriteReal(unit, size, text);

    // Pack up return values
    if (result == -1)
    {
        args->arg2 = (void *) 0;
        args->arg4 = (void *) -1;
    }
    else
    {
        args->arg2 = (void *) ((long) result);
        args->arg4 = (void *) 0;
    }

    // Set to user mode
    setToUserMode();
}

int termWriteReal(int unit, int size, char *text)
{
    if (DEBUG4 && debugflag4)
    {
        USLOSS_Console("termWriteReal(): called.\n");
    }
    return -1;
}
