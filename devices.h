#ifndef _DEVICES_H
#define _DEVICES_H

#define DEBUG4 1;

typedef struct process process;
typedef struct process * processPtr;

struct process
{
    int pid;                          // The pid of this process
};

#define EMPTY -1;

#endif
