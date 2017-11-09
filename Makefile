#PREFIX = /home/theimbichner/Csc452Project/
#LDFLAGS += -L. -L./libraries/linux -L${PREFIX}/lib
PREFIX = /Users/Caleb/Documents/ComputerScience/csc452/project/compilefolder#${HOME}
LDFLAGS += -L. -L./libraries/osx -L${PREFIX}/lib

TARGET = libphase4.a
ASSIGNMENT = 452phase4
CC = gcc
AR = ar

COBJS = phase4.o phase4utility.o libuser.o phase4clock.o phase4disk.o phase4term.o
CSRCS = ${COBJS:.o=.c}

PHASE1LIB = patrickphase1
PHASE2LIB = patrickphase2
PHASE3LIB = patrickphase3

HDRS = providedPrototypes.h libuser.h devices.h phase4utility.h phase1.h phase2.h phase3.h phase4.h phase4clock.h phase4disk.h phase4term.h

INCLUDE = ${PREFIX}/include

CFLAGS = -Wall -g -std=gnu99 -I. -I${INCLUDE}

UNAME := $(shell uname -s)

ifeq ($(UNAME), Darwin)
        CFLAGS += -D_XOPEN_SOURCE
endif

TESTDIR = testcases
TESTS = test00 test01 test02 test03 test04 test05 test06 test07 test08 \
        test09 test10 test11 test12 test13 test14 test15 test16 test17 \
        test18

LIBS = -l$(PHASE3LIB) -l$(PHASE2LIB) -l$(PHASE1LIB) -lusloss3.6 -l$(PHASE1LIB) -l$(PHASE2LIB) -l $(PHASE3LIB) -lphase4


$(TARGET):	$(COBJS)
		$(AR) -r $@ $(COBJS)

$(TESTS):	$(TARGET) p1.o
	$(CC) $(CFLAGS) -c $(TESTDIR)/$@.c
	$(CC) $(LDFLAGS) -o $@ $@.o $(LIBS) p1.o

clean:
	rm -f $(COBJS) $(TARGET) test*.o test*.txt term* $(TESTS) \
		libuser.o p1.o core

phase4.o:	devices.h

submit: $(CSRCS) $(HDRS) Makefile
	tar cvzf phase4.tgz $(CSRCS) $(HDRS) Makefile p1.c
