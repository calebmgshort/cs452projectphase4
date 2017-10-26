#include "libuser.h"

#define CHECKMODE {    \
    if (USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) { \
        USLOSS_Console("Trying to invoke syscall from kernel\n"); \
        USLOSS_Halt(1);  \
    }  \
}

int Sleep(int seconds)
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("Sleep(): called.\n");
    }
    USLOSS_Sysargs sysArg;
    CHECKMODE;
    sysArg.number = SYS_SLEEP;
    sysArg.arg1 = (void *) ((long) seconds);

    USLOSS_Syscall(&sysArg);

    int returnStatus = (int) ((long) sysArg.arg4);

    return returnStatus;
}

int DiskRead(void *dbuff, int unit, int track, int first, int sectors, int *status)
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("DiskRead(): called.\n");
    }
    USLOSS_Sysargs sysArg;
    CHECKMODE;
    sysArg.number = SYS_DISKREAD;
    sysArg.arg1 = dbuff;
    sysArg.arg2 = (void *) ((long) sectors);
    sysArg.arg3 = (void *) ((long) track);
    sysArg.arg4 = (void *) ((long) first);
    sysArg.arg5 = (void *) ((long) unit);

    USLOSS_Syscall(&sysArg);

    // Return arg4 and put arg1 in status
    *status = (int) ((long) sysArg.arg1);
    int returnStatus = (int) ((long) sysArg.arg4);

    return returnStatus;
}

int DiskWrite(void *dbuff, int unit, int track, int first, int sectors, int *status)
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("DiskWrite(): called.\n");
    }
    USLOSS_Sysargs sysArg;
    CHECKMODE;
    sysArg.number = SYS_DISKWRITE;
    sysArg.arg1 = dbuff;
    sysArg.arg2 = (void *) ((long) sectors);
    sysArg.arg3 = (void *) ((long) track);
    sysArg.arg4 = (void *) ((long) first);
    sysArg.arg5 = (void *) ((long) unit);

    USLOSS_Syscall(&sysArg);

    // Return arg4 and put arg1 in status
    *status = (int) ((long) sysArg.arg1);
    int returnStatus = (int) ((long) sysArg.arg4);

    return returnStatus;
}

int DiskSize(int unit, int *sector, int *track, int *disk)
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("DiskSize(): called.\n");
    }
    USLOSS_Sysargs sysArg;
    CHECKMODE;
    sysArg.number = SYS_DISKSIZE;
    sysArg.arg1 = (void *) ((long) unit);

    USLOSS_Syscall(&sysArg);

    *sector = (int) ((long) sysArg.arg2);
    *track = (int) ((long) sysArg.arg3);
    *disk = (int) ((long) sysArg.arg1);   // TODO: Is this right? This should be the size of a sector
    int returnStatus = (int) ((long) sysArg.arg4);

    return returnStatus;
}

int TermRead(char *buff, int bsize, int unit_id, int *nread)
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("TermRead(): called.\n");
    }
    USLOSS_Sysargs sysArg;
    CHECKMODE;
    sysArg.number = SYS_TERMREAD;
    sysArg.arg1 = (void *) buff;
    sysArg.arg2 = (void *) ((long) bsize);
    sysArg.arg3 = (void *) ((long) unit_id);

    USLOSS_Syscall(&sysArg);

    *nread = (int) ((long) sysArg.arg2);
    int returnStatus = (int) ((long) sysArg.arg4);

    return returnStatus;
}

int TermWrite(char *buff, int bsize, int unit_id, int *nwrite)
{
    // TODO: What is bsize for?
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("TermWrite(): called.\n");
    }
    USLOSS_Sysargs sysArg;
    CHECKMODE;
    sysArg.number = SYS_TERMWRITE;
    sysArg.arg1 = (void *) buff;
    sysArg.arg2 = (void *) ((long) (nwrite));
    sysArg.arg3 = (void *) ((long) unit_id);

    USLOSS_Syscall(&sysArg);

    *nwrite = (int) ((long) sysArg.arg2);
    int returnStatus = (int) ((long) sysArg.arg4);

    return returnStatus; 
}
