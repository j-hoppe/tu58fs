#
# tu58fs emulator makefile
#

# UNIX comms model
PROG = tu58fs

# compiler flags and libraries
CC_DBG_FLAGS = -ggdb3 -O0
LDFLAGS = -lpthread -lrt

ifeq ($(MAKE_TARGET_ARCH),BBB)
	CC=$(BBB_CC) -std=c99 -U__STRICT_ANSI__
	OBJDIR=bin-bbb
else ifeq ($(MAKE_TARGET_ARCH),RPI)
	CC=arm-linux-gnueabihf-gcc -std=c99 -U__STRICT_ANSI__
	OBJDIR=bin-rpi
else ifeq ($(MAKE_TARGET_ARCH),CYGWIN)
    # compiling under CYGWIN
    OS_CCDEFS=
	OBJDIR=bin-cygwin
#    OS_CCDEFS=-m32
    PROG = tu58fs.exe
else
    # compiling for X64
    OS_CCDEFS=-m64
	OBJDIR=bin-ubuntu-x64
endif

CCFLAGS= \
	-I.	\
	-c	\
	$(CCDEFS) $(CC_DBG_FLAGS) $(OS_CCDEFS)

OBJECTS = $(OBJDIR)/main.o \
		$(OBJDIR)/getopt2.o \
		$(OBJDIR)/tu58drive.o \
		$(OBJDIR)/image.o \
		$(OBJDIR)/serial.o \
		$(OBJDIR)/hostdir.o \
		$(OBJDIR)/error.o \
		$(OBJDIR)/utils.o \
		$(OBJDIR)/boolarray.o \
		$(OBJDIR)/filesort.o \
		$(OBJDIR)/filesystem.o \
		$(OBJDIR)/device_info.o \
		$(OBJDIR)/xxdp.o \
		$(OBJDIR)/xxdp_radi.o \
		$(OBJDIR)/rt11.o	\
		$(OBJDIR)/rt11_radi.o \


$(OBJDIR)/$(PROG) : $(OBJECTS)
	$(CC) -o $@ $(OBJECTS) $(LDFLAGS)
	file $@
#	mv $@ $(OBJDIR) ; file $(OBJDIR)/$@

all :   $(OBJDIR)/$(PROG)

clean :
	-rm -f $(OBJECTS)
#	-chmod a-x,ug+w,o-w *.c *.h makefile
#	-chmod a+rx $(OBJDIR)/$(PROG)
#	-chown `whoami` *

purge : clean
	-rm -f $(OBJDIR)/$(PROG)

$(OBJDIR)/main.o : main.c main.h
	$(CC) $(CCFLAGS) main.c -o $@

$(OBJDIR)/serial.o : serial.c serial.h
	$(CC) $(CCFLAGS) serial.c -o $@

$(OBJDIR)/getopt2.o : getopt2.c getopt2.h
	$(CC) $(CCFLAGS) getopt2.c -o $@

$(OBJDIR)/hostdir.o : hostdir.c hostdir.h
	$(CC) $(CCFLAGS) hostdir.c -o $@

$(OBJDIR)/error.o : error.c error.h
	$(CC) $(CCFLAGS) error.c -o $@

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

$(OBJDIR)/filesystem.o : filesystem.c filesystem.h
	$(CC) $(CCFLAGS) filesystem.c -o $@

$(OBJDIR)/device_info.o : device_info.c device_info.h
	$(CC) $(CCFLAGS) device_info.c -o $@

$(OBJDIR)/xxdp.o : xxdp.c xxdp.h
	$(CC) $(CCFLAGS) xxdp.c -o $@

$(OBJDIR)/xxdp_radi.o : xxdp_radi.c xxdp_radi.h
	$(CC) $(CCFLAGS) xxdp_radi.c -o $@

$(OBJDIR)/rt11.o : rt11.c rt11.h
	$(CC) $(CCFLAGS) rt11.c -o $@

$(OBJDIR)/rt11_radi.o : rt11_radi.c rt11_radi.h
	$(CC) $(CCFLAGS) rt11_radi.c -o $@

# the end
