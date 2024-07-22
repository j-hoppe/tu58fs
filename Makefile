#
# tu58fs emulator makefile
#

# UNIX comms model
PROG = tu58fs

# compiler flags and libraries
CC_DBG_FLAGS = -ggdb3 -O0 -Wall -Wextra

MAKE_TARGET_ARCH ?= $(shell uname)
$(warning Building for $(MAKE_TARGET_ARCH))

ifeq ($(MAKE_TARGET_ARCH),BBB)
    CC=$(BBB_CC) -std=c99 -U__STRICT_ANSI__
    LDFLAGS = -lpthread -lrt
    OBJDIR=bin-bbb
else ifeq ($(MAKE_TARGET_ARCH),RPI)
    CC=arm-linux-gnueabihf-gcc -std=c99 -U__STRICT_ANSI__
    LDFLAGS = -lpthread -lrt
    OBJDIR=bin-rpi
else ifeq ($(MAKE_TARGET_ARCH),CYGWIN)
    # compiling under CYGWIN
    OS_CCDEFS=
    LDFLAGS = -lpthread
    OBJDIR=bin-cygwin
#    OS_CCDEFS=-m32
    PROG = tu58fs.exe
else ifeq ($(MAKE_TARGET_ARCH),Darwin)
    # compiling for macos
    OS_CCDEFS=-m64 -std=c99 -U__STRICT_ANSI__
    LDFLAGS = -lpthread
    OBJDIR=bin-macos
else ifeq ($(MAKE_TARGET_ARCH),Linux)
    # compiling for X64
    OS_CCDEFS=-m64
    LDFLAGS = -lpthread
    OBJDIR=bin-ubuntu-x64
else
    $(error Unknown target arch $(MAKE_TARGET_ARCH))
endif

CFLAGS := \
    -I.	\
    $(CCDEFS) \
    $(CC_DBG_FLAGS) \
    $(OS_CCDEFS)

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
    $(OBJDIR)/monitor.o \
    $(OBJDIR)/bootloader.o \


all: $(OBJDIR)/$(PROG)

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/$(PROG): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS)
	file $@

$(OBJDIR)/%.o: %.c %.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJECTS)

purge: clean
	rm -rf $(OBJDIR)

