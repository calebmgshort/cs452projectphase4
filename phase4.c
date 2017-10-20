#include <usloss.h>
#include <usyscall.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase4.h>
#include <stdlib.h>
#include "devices.h"

// Debugging flag
int debugflag4 = 0;

semaphore 	running;

static int	ClockDriver(char *);
static int	DiskDriver(char *);

void sleep(systemArgs *);
void diskRead();
void diskWrite();
void diskSize();
void termRead();
void termWrite();

int sleepReal(int);
void diskReadReal();
void diskWriteReal();
void diskSizeReal();
void termReadReal();
void termWriteReal();

process ProcTable[MAXPROC];

int start3(char *args)
{
    if(DEBUG4 && debugflag4)
    {
        USLOSS_Console("start3(): started");
    }

    char	name[128];
    char        termbuf[10];
    int		i;
    int		clockPID;
    int         diskPIDs[USLOSS_DISK_UNITS];
    int         termPIDs[USLOSS_TERM_UNITS];
    int		pid;
    int		status;

    // Check kernel mode
    checkMode("start3");

    // Initialize the system call vector
    systemCallVec[SYS_SLEEP] = sleep;
    systemCallVec[SYS_DISKREAD] = diskRead;
    systemCallVec[SYS_DISKWRITE] = diskWrite;
    systemCallVec[SYS_DISKSIZE] = diskSize;
    systemCallVec[SYS_TERMREAD] = termRead;
    systemCallVec[SYS_TERMWRITE] = termWrite;

    // Initialize the ProcTable
    for (int i = 0; i < MAXPROC; i++)
    {
        ProcTable[i].pid = EMPTY;
        ProcTable[i].privateMboxID = MboxCreate(0, MAX_MESSAGE);
    }

    /*
     * Create clock device driver
     * I am assuming a semaphore here for coordination.  A mailbox can
     * be used instead -- your choice.
     */
    running = semcreateReal(0);
    clockPID = fork1("Clock driver", ClockDriver, NULL, USLOSS_MIN_STACK, 2);
    if (clockPID < 0)
    {
	       USLOSS_Console("start3(): Can't create clock driver\n");
	       USLOSS_Halt(1);
    }

    /*
     * Wait for the clock driver to start. The idea is that ClockDriver
     * will V the semaphore "running" once it is running.
     */
    sempReal(running);

    /*
     * Create the disk device drivers here.  You may need to increase
     * the stack size depending on the complexity of your
     * driver, and perhaps do something with the pid returned.
     */
    for (i = 0; i < USLOSS_DISK_UNITS; i++)
    {
        sprintf(buf, "%d", i);
        pid = fork1(name, DiskDriver, buf, USLOSS_MIN_STACK, 2);
        diskPIDs[i] = pid;
        if (pid < 0)
        {
            USLOSS_Console("start3(): Can't create term driver %d\n", i);
            USLOSS_Halt(1);
        }

        // Wait for the driver to start
        sempReal(running);
    }

    // May be other stuff to do here before going on to terminal drivers

    // Create terminal device drivers
    for (i = 0; i < USLOSS_TERM_UNITS; i++)
    {
        sprintf(buf, "%d", i);
        pid = fork1(name, TermDriver, buf, USLOSS_MIN_STACK, 2);
        termPIDs[i] = pid;
        if (pid < 0)
        {
            USLOSS_Console("start3(): Can't create term driver %d\n", i);
            USLOSS_Halt(1);
        }

        // Wait for the driver to start
        sempReal(running);
    }

    /*
     * Create first user-level process and wait for it to finish.
     * These are lower-case because they are not system calls;
     * system calls cannot be invoked from kernel mode.
     * I'm assuming kernel-mode versions of the system calls
     * with lower-case first letters, as shown in provided_prototypes.h
     */
    pid = spawnReal("start4", start4, NULL, 4 * USLOSS_MIN_STACK, 3);
    pid = waitReal(&status);

    // Zap the device drivers
    zap(clockPID);
    for (i = 0; i < USLOSS_DISK_UNITS; i++)
    {
        zap(diskPIDs[i]);
    }
    for (i = 0; i < USLOSS_TERM_UNITS; i++)
    {
        zap(termPIDs[i]);
    }

    // Quit
    quit(0);
    return 0;
}

static int ClockDriver(char *arg)
{
    int result;
    int status;

    // Let the parent know we are running and enable interrupts.
    semvReal(running);
    USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);

    // Infinite loop until we are zap'd
    while(!isZapped())
    {
	result = waitdevice(USLOSS_CLOCK_DEV, 0, &status);
	if (result != 0)
        {
	    return 0;
	}
	/*
	 * Compute the current time and wake up any processes
	 * whose time has come.
	 */
    }
    quit(0);
}

static int DiskDriver(char *arg)
{
    return 0;
}

void sleep(systemArgs *args)
{
    // Unpack args
    int secs = (int) ((long) args->arg1);

    // Defer to sleepReal
    long result = sleepReal(secs);

    // Put return values in args
    args->arg4 = (void *) result;

    // Set to user mode
    setToUserMode();
}

int sleepReal(int secs)
{
    // Put an entry in the clock driver queue
    // Block this process
    blockOnMbox();
}
