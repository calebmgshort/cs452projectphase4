#include "libuser.h"

int Sleep(int seconds)
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("Sleep(): called.\n");
    }
    return -1; // TODO
}

int DiskRead(void *dbuff, int unit, int track, int first, int sectors, int *status)
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("DiskRead(): called.\n");
    }
    return -1; // TODO
}

int DiskWrite(void *dbuff, int unit, int track, int first, int sectors, int *status)
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("DiskWrite(): called.\n");
    }
    return -1; // TODO
}

int DiskSize(int unit, int *sector, int *track, int *disk)
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("DiskSize(): called.\n");
    }
    return -1; // TODO
}

int TermRead(char *buff, int bsize, int unit_id, int *nread)
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("TermRead(): called.\n");
    }
    return -1; // TODO
}

int TermWrite(char *buff, int bsize, int unit_id, int *nwrite)
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("TermWrite(): called.\n");
    }
    return -1; // TODO
}
