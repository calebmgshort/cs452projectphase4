/*
 *  File: phase4term.c
 *  Purpose: This file holds functions and global variables that deal with the
 *  terminal devices
 */

#include <usloss.h>
#include <usyscall.h>
#include <stdlib.h>
#include <stdio.h>

#include "phase1.h"
#include "phase2.h"
#include "providedPrototypes.h"
#include "devices.h"
#include "phase4utility.h"
#include "phase4term.h"

extern int debugflag4;

// Global terminal variables
termInputBuffer TermReadBuffers[USLOSS_TERM_UNITS];
semaphore TermReadBufferLocks[USLOSS_TERM_UNITS];
int TermReadBufferWaitMbox[USLOSS_TERM_UNITS];
int TermWriteWaitMbox[USLOSS_TERM_UNITS];
int TermWriteMessageMbox[USLOSS_TERM_UNITS];

/*
 *  System call for user function TermRead. Serves as a bridge between TermRead
 *  and termReadReal
 */
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

/*
 *  This routine reads a line of text from the terminal indicated by unit into
 *  the buffer pointed to by buffer. A line of text is terminated by a newline
 *  character (‘\n’), which is copied into the buffer along with the other
 *  characters in the line. If the length of a line of input is greater than the
 *  value of the size parameter, then the first size characters are returned and
 *  the rest discarded.
 *  The terminal device driver should maintain a fixed-size buffer of 10 lines to
 *  store characters read prior to an invocation of termRead (i.e. a read-ahead
 *  buffer). Characters should be discarded if the read-ahead buffer overflows.
 *  Return values:
 *    -1: invalid parameters
 *    >0: number of characters read
 */
int termReadReal(int unit, int size, char *resultBuffer)
{
    if (DEBUG4 && debugflag4)
    {
        USLOSS_Console("termReadReal(): called.\n");
    }

    // Check args
    if (unit < 0 || unit >= USLOSS_TERM_UNITS)
    {
        return -1;
    }
    if (size <= 0)
    {
        return -1;
    }

    // Acquire the mutex lock on the buffer
    sempReal(TermReadBufferLocks[unit]);

    // Get the buffer
    termInputBuffer *buffer = &TermReadBuffers[unit];

    // Wait if the buffer is empty
    if (buffer->lineToRead == EMPTY)
    {
        semvReal(TermReadBufferLocks[unit]);
        MboxReceive(TermReadBufferWaitMbox[unit], NULL, 0);
        sempReal(TermReadBufferLocks[unit]);
    }

    // Copy the data from the buffer
    termLine line = buffer->lines[buffer->lineToRead];
    int i;
    for (i = 0; i < size; i++)
    {
        if (i == line.indexToModify)
        {
            break;
        }
        resultBuffer[i] = line.characters[i];
    }

    // Indicate that the line has been read
    buffer->lineToRead++;
    if (buffer->lineToRead == 10)
    {
        buffer->lineToRead = 0;
    }
    if (buffer->lineToRead == buffer->lineToModify)
    {
        buffer->lineToRead = EMPTY;
    }

    // Release the lock
    semvReal(TermReadBufferLocks[unit]);

    return i;
}

/*
 *  System call for user function TermWrite. Serves as a bridge between TermWrite
 *  and termWriteReal
 */
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

/*
 *  This routine writes size characters — a line of text pointed to by text to the
 *  terminal indicated by unit. A newline is not automatically appended, so if one
 *  is needed it must be included in the text to be written. This routine should not
 *  return until the text has been written to the terminal.
 *  Return values:
 *    -1: invalid parameters
 *    >0: number of characters written
 */
int termWriteReal(int unit, int size, char *text)
{
    if (DEBUG4 && debugflag4)
    {
        USLOSS_Console("termWriteReal(): called.\n");
    }

    // Check params
    if (unit < 0 || unit >= USLOSS_TERM_UNITS)
    {
        return -1;
    }
    if (size < 0 || size > MAXLINE)
    {
        return -1;
    }

    // Send the text to the mailbox
    MboxSend(TermWriteMessageMbox[unit], text, size);

    if (DEBUG4 && debugflag4)
    {
        USLOSS_Console("termWriteReal(): About to block pid %d after write to terminal %d.\n", getpid(), unit);
    }

    // Once the text has been sent, wait for the writing to complete
    MboxReceive(TermWriteWaitMbox[unit], NULL, 0);

    if (DEBUG4 && debugflag4)
    {
        USLOSS_Console("termWriteReal(): Pid %d unblocked.\n", getpid());
    }

    return size;
}

/*
 * Initializes the supplied termInputBuffer.
 */
void clearBuffer(termInputBuffer *buffer)
{
    buffer->lineToRead = EMPTY;
    buffer->lineToModify = 0;
    for (int i = 0; i < 10; i++)
    {
        termLine *line = &buffer->lines[i];
        line->indexToModify = 0;
        for (int j = 0; j < MAXLINE; j++)
        {
            line->characters[j] = '\0';
        }
    }
}

/*
 * Writes the given input char into the buffer associated with the given term
 * unit. unit must be a valid term unit [0, USLOSS_TERM_UNITS - 1]
 */
void storeChar(int unit, char input)
{
    // Get the buffer
    termInputBuffer *buffer = &TermReadBuffers[unit];

    // Acquire the mutex lock
    sempReal(TermReadBufferLocks[unit]);

    // Modify the approrpriate line
    termLine *line = &buffer->lines[buffer->lineToModify];
    line->characters[line->indexToModify] = input;
    line->indexToModify++;

    // If we've reached the end of the line, move to the next
    if (line->indexToModify == MAXLINE || input == '\n')
    {
        // If the buffer was previously empty, tell it where it can be read
        if (buffer->lineToRead == EMPTY)
        {
            buffer->lineToRead = buffer->lineToModify;
        }
        buffer->lineToModify++;

        // If something was waiting on the mailbox, unblock it
        MboxCondSend(TermReadBufferWaitMbox[unit], NULL, 0);
    }

    // Wrap around if necessary
    if (buffer->lineToModify == 10)
    {
        buffer->lineToModify = 0;
    }

    // Release the mutex lock
    semvReal(TermReadBufferLocks[unit]);
}

void awakenTerminal(int unit)
{
    char buf[20];
    sprintf(buf, "term%d.in", unit);
    FILE *term = fopen(buf, "a");
    fprintf(term, "Close Terminal");
    fclose(term);
}
