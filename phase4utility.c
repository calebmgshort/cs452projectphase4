#include <usloss.h>

void checkMode(char *funcName)
{
    unsigned int psr = USLOSS_PsrGet();
    unsigned int kernelMode = psr & USLOSS_PSR_CURRENT_MODE;
    if (!kernelMode)
    {
        USLOSS_Console("%s(): Called from user mode.  Halting...\n", funcName);
        USLOSS_Halt(1);
    }
}
