#
# tu58em emulator makefile
#

# UNIX comms model
PROG = tu58fs

# compiler flags and libraries
COMM = -UWINCOMM
#CFLAGS = -I. -Wall -c $(COMM)
CC_DBG_FLAGS = -ggdb3 -O0
LDFLAGS = -lpthread -lrt

ifeq ($(MAKE_TARGET_ARCH),BBB)
	CC=$(BBB_CC) -std=c99 -U__STRICT_ANSI__
	OBJDIR=bin-bbb
else
    # compiling for X64
    OS_CCDEFS=-m64
	OBJDIR=bin-ubuntu-x64
endif

CCFLAGS= \
	-I.	\
	-c	\
	$(COMM)	\
	$(CCDEFS) $(CC_DBG_FLAGS) $(OS_CCDEFS)

OBJECTS = $(OBJDIR)/main.o \
		$(OBJDIR)/getopt2.o \
		$(OBJDIR)/tu58drive.o \
		$(OBJDIR)/image.o \
		$(OBJDIR)/serial.o \
		$(OBJDIR)/hostdir.o \
		$(OBJDIR)/utils.o \
		$(OBJDIR)/boolarray.o \
		$(OBJDIR)/filesort.o \
		$(OBJDIR)/xxdp.o \
		$(OBJDIR)/xxdp_radi.o \
		$(OBJDIR)/rt11.o


$(PROG) : $(OBJECTS)
	$(CC) -o $@ $(OBJECTS) $(LDFLAGS)
	mv $@ $(OBJDIR) ; file $(OBJDIR)/$@

all :   $(PROG)

clean :
	-rm -f $(OBJECTS)
	-chmod a-x,ug+w,o-w *.c *.h makefile
	-chmod a+rx $(OBJDIR)/$(PROG)
	-chown `whoami` *

purge : clean
	-rm -f $(PROG)

$(OBJDIR)/main.o : main.c main.h
	$(CC) $(CCFLAGS) main.c -o $@

$(OBJDIR)/serial.o : serial.c serial.h
	$(CC) $(CCFLAGS) serial.c -o $@

$(OBJDIR)/getopt2.o : getopt2.c getopt2.h
	$(CC) $(CCFLAGS) getopt2.c -o $@

$(OBJDIR)/hostdir.o : hostdir.c hostdir.h
	$(CC) $(CCFLAGS) hostdir.c -o $@

$(OBJDIR)/utils.o : utils.c utils.h
	$(CC) $(CCFLAGS) utils.c -o $@

$(OBJDIR)/boolarray.o : boolarray.c boolarray.h
	$(CC) $(CCFLAGS) boolarray.c -o $@

$(OBJDIR)/filesort.o : filesort.c filesort.h
	$(CC) $(CCFLAGS) filesort.c -o $@

$(OBJDIR)/tu58drive.o : tu58drive.c tu58.h tu58drive.h
	$(CC) $(CCFLAGS) tu58drive.c -o $@

$(OBJDIR)/image.o : image.c image.h
	$(CC) $(CCFLAGS) image.c -o $@

$(OBJDIR)/rt11.o : rt11.c rt11.h
	$(CC) $(CCFLAGS) rt11.c -o $@

$(OBJDIR)/xxdp.o : xxdp.c xxdp.h
	$(CC) $(CCFLAGS) xxdp.c -o $@

$(OBJDIR)/xxdp_radi.o : xxdp_radi.c xxdp.h
	$(CC) $(CCFLAGS) xxdp_radi.c -o $@

# the end
