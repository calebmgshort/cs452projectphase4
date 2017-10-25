#include "libuser.h"

int Sleep(int seconds)
{
    return -1; // TODO
}

int DiskRead(void *dbuff, int unit, int track, int first, int sectors, int *status)
{
    return -1; // TODO
}

int DiskWrite(void *dbuff, int unit, int track, int first, int sectors, int *status)
{
    return -1; // TODO
}

int DiskSize(int unit, int *sector, int *track, int *disk)
{
    return -1; // TODO
}

int TermRead(char *buff, int bsize, int unit_id, int *nread)
{
    return -1; // TODO
}

int TermWrite(char *buff, int bsize, int unit_id, int *nwrite)
{
    return -1; // TODO
}
