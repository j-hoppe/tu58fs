/*  tu58device.c - server for TU58 protocol, ctrl over serial line, work on image
 *
 *  Original (C) 1984 Dan Ts'o <Rockefeller Univ. Dept. of Neurobiology>
 *  Update   (C) 2005-2016 Donald N North <ak6dn_at_mindspring_dot_com>
 *  Update   (C) 2017 Joerg Hoppe <j_hoppe@t-online.de>, www.retrocmp.com
 *  Update   (C) 2024 June Tate-Gans <june@theonelab.com>, www.nybblesandbytes.net
 *
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  o Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  o Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 *  o Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 *  TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 *  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 *  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  This is the TU58 emulation program written at Rockefeller Univ., Dept. of
 *  Neurobiology. We copyright (C) it and permit its use provided it is not
 *  sold to others. Originally written by Dan Ts'o circa 1984 or so.
 *
 *  21-Jul-2024 June Tate-Gans  fixes for macos build, add configuration header
 *  07-May-2017 JH, Don North  compiles under MACOS, passes GCC warning levels -Wall -Wextra
 *  12-Jan-2017 JH  taken from tu58em
 */
#define _TU58DRIVE_C_

//
// TU58 Drive Emulator Machine
//
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <strings.h>
#include <ctype.h>
#include <pthread.h>

#include "error.h"
#include "utils.h"
#include "device_info.h"
#include "image.h"
#include "main.h"	// option flags
#include "serial.h"
#include "config.h"	// host compiler configuration
#include "tu58.h"	// protocoll
#include "tu58drive.h"	// own

// hold one image per device
image_t *tu58_image[TU58_DEVICECOUNT];

// the serial port
serial_device_t tu58_serial;

#if !defined(HAVE_CLOCK_GETTIME)
// clock_gettime() is not available under older macos versions.
#define CLOCK_REALTIME 1
#include <mach/mach_time.h>
#include <mach/clock.h>
#include <mach/mach.h>
void clock_gettime(int dummy, struct timespec *t) {
	uint64_t mt;
	UNUSED(dummy) ;
	mt = mach_absolute_time();
	t->tv_sec = mt / 1000000000;
	t->tv_nsec = mt % 1000000000;
}
#endif


// delays for modeling device access

static struct {
	uint16_t nop;	// ms per NOP, STATUS commands
	uint16_t init;	// ms per INIT command
	uint16_t test;	// ms per DIAGNOSE command
	uint16_t seek;	// ms per SEEK command (s.b. variable)
	uint16_t read;	// ms per READ 128B packet command
	uint16_t write;	// ms per WRITE 128B packet command
} tudelay[] = {
//    nop init test  seek read write
		{ 1, 1, 1, 0, 0, 0 }, // timing=0 infinitely fast...
		{ 1, 1, 25, 25, 25, 25 }, // timing=1 fast enough to fool diagnostic
		{ 1, 1, 25, 200, 100, 100 }, // timing=2 closer to real TU58 behavior
		};

// global state

uint8_t mrsp = 0;			// set nonzero to indicate MRSP mode is active

// communication beetween thread and control
uint8_t tu58_doinit = 0;			// set nonzero to indicate should send INITs continuously
uint8_t tu58_runonce = 0;	// set nonzero to indicate emulator has been run

// Inter-thread communication: control off offline-state
int volatile tu58_offline_request;  // 1: main thread wants offline mode
int volatile tu58_offline; // TU58 is offline, all drives without cartridge

void tu58images_init() {
	int32_t unit;
	for (unit = 0; unit < TU58_DEVICECOUNT; unit++) { // minimal init
		tu58_image[unit] = NULL;
	}
}

// allocate an image for a TU58 device
image_t *tu58image_create(int32_t unit, int forced_data_size) {
	if (!IMAGE_UNIT_VALID(unit)) {
		fatal("bad unit %d", unit); //terminates
	}
	if (tu58_image[unit] != NULL)
		fatal("tu58image_create(): duplicate allocation"); //terminates

	tu58_image[unit] = image_create(devTU58, unit, forced_data_size);
	return tu58_image[unit];
}

// select image over unit number
image_t *tu58image_get(int32_t unit) {
	if (!IMAGE_UNIT_VALID(unit)) {
		fatal("bad unit %d", unit);
		return NULL; // not reached
	}
	return tu58_image[unit];
}

// save all changes
void tu58images_closeall(void) {
	image_t *img;
	int32_t unit;
	for (unit = 0; unit < TU58_DEVICECOUNT; unit++) {
		img = tu58_image[unit];
		if (img) {
			if (img->open)
				image_sync(img);
			image_destroy(img);
		}
	}
}

// save changed images after idle delay
void tu58images_sync_all() {
	image_t *img;
	int32_t unit;

	if (!opt_synctimeout_sec)
		return; // not wanted

	for (unit = 0; unit < TU58_DEVICECOUNT; unit++) {
		img = tu58_image[unit];

		if (img && img->open) {
			if (opt_debug)
				info("unit %d sync ", unit);
			image_sync(img); // does locking
		}
	}
}

//

// reinitialize TU58 state
//
static void reinit(void) {
	// clear all buffers, wait a bit
	serial_devrxinit(&tu58_serial);
	serial_devtxinit(&tu58_serial);
	delay_ms(5);

	// init sequence, send immediately
	serial_devtxstart(&tu58_serial);
	serial_devtxput(&tu58_serial, TUF_INIT);
	serial_devtxput(&tu58_serial, TUF_INIT);
	serial_devtxflush(&tu58_serial);

	return;
}

//
// read of boot is not packetized, is just raw data
//
static void bootio(void) {
	image_t *img;
	int32_t unit;
	int32_t count;
	uint8_t buffer[TU_BOOT_LEN];

	// check unit number for validity
	unit = serial_devrxget(&tu58_serial);
	img = tu58image_get(unit);
	if (!img || !img->open) {
		error("bootio bad unit %d", unit);
		return;
	}

	if (opt_verbose)
		info("%-8s unit=%d blk=0x%04X cnt=0x%04X", "boot", unit, 0,
		TU_BOOT_LEN);

	// seek to block zero, should never be an error :-)
	if (image_blockseek(img, 0, 0, 0)) {
		error("boot seek error unit %d", unit);
		return;
	}

	// read one block of data
	if ((count = image_read(img, buffer, TU_BOOT_LEN)) != TU_BOOT_LEN) {
		error("boot file read error unit %d, expected %d, received %d", unit,
		TU_BOOT_LEN, count);
		return;
	}

	// write one block of data to serial line
	if ((count = serial_devtxwrite(&tu58_serial, buffer, TU_BOOT_LEN)) != TU_BOOT_LEN) {
		error("boot serial write error unit %d, expected %d, received %d", unit,
		TU_BOOT_LEN, count);
		return;
	}

	return;
}

//
// debug dump a packet to ferr
//
static void dumppacket(tu_packet *pkt, char *name) {
	int32_t count = 0;
	uint8_t *ptr = (uint8_t *) pkt;

	// formatted packet dump, but skip it in background mode
	if (!opt_background) {
		fprintf(ferr, "info: %s()\n", name);
		while (count++ < pkt->cmd.length + 2) {
			if (count == 3 || ((count - 4) % 32 == 31))
				fprintf(ferr, "\n");
			fprintf(ferr, " %02X", *ptr++);
		}
		fprintf(ferr, "\n %02X %02X\n", ptr[0], ptr[1]);
	}

	return;
}

//
// compute checksum of a TU58 packet
//
static uint16_t checksum(tu_packet *pkt) {
	int32_t count = pkt->cmd.length + 2; // +2 for flag/length bytes
	uint8_t *ptr = (uint8_t *) pkt; // start at flag byte
	uint32_t chksum = 0; // initial checksum value

	while (--count >= 0) {
		chksum += *ptr++; // at least one byte
		if (--count >= 0)
			chksum += (*ptr++ << 8); // at least two bytes
		chksum = (chksum + (chksum >> 16)) & 0xFFFF; // 16b end around carry
	}

	return chksum;
}

//
// wait for a CONT to arrive
//
static void wait4cont(uint8_t code) {
	uint8_t c;
	int32_t maxchar = TU_CTRL_LEN + TU_DATA_LEN + 8;

	// send any existing data out ... makes USB serial emulation be real slow if enabled!
	if (0)
		serial_devtxflush(&tu58_serial);

	// don't do any waiting if flag not set
	if (!code)
		return;

	// wait for a CONT to arrive, but only so long
	do {
		c = serial_devrxget(&tu58_serial);
		if (opt_debug)
			info("wait4cont(): char=0x%02X", c);
	} while (c != TUF_CONT && --maxchar >= 0);

	// all done
	return;
}

//
// put a packet
//
static void putpacket(tu_packet *pkt) {
	int32_t count = pkt->cmd.length + 2; // +2 for flag/length bytes
	uint8_t *ptr = (uint8_t *) pkt; // start at flag byte
	uint16_t chksum;

	// send all packet bytes
	while (--count >= 0) {
		serial_devtxput(&tu58_serial, *ptr++);
		wait4cont(mrsp);
	}

	// compute/send checksum bytes, append to packet
	chksum = checksum(pkt);
	serial_devtxput(&tu58_serial, *ptr++ = chksum >> 0);
	wait4cont(mrsp);
	serial_devtxput(&tu58_serial, *ptr++ = chksum >> 8);
	wait4cont(mrsp);

	// for debug...
	if (opt_debug)
		dumppacket(pkt, "putpacket");

	// now actually send the packet (or whatever is left to send)
	serial_devtxflush(&tu58_serial);

	return;
}

//
// get a packet
//
static int32_t getpacket(tu_packet *pkt) {
	int32_t count = pkt->cmd.length + 2; // +2 for checksum bytes
	uint8_t *ptr = (uint8_t *) pkt + 2; // skip over flag/length bytes
	uint16_t rcvchk, expchk;

	// get remaining packet bytes, incl two checksum bytes
	while (--count >= 0)
		*ptr++ = serial_devrxget(&tu58_serial);

	// get checksum bytes
	rcvchk = (ptr[-1] << 8) | (ptr[-2] << 0);

	// compute expected checksum
	expchk = checksum(pkt);

	// for debug...
	if (opt_debug)
		dumppacket(pkt, "getpacket");

	// message on error
	if (expchk != rcvchk)
		error("getpacket checksum error: exp=0x%04X rcv=0x%04X", expchk, rcvchk);

	// return checksum match indication
	return (expchk != rcvchk);
}

//
// tu58 sends end packet to host
//
static void endpacket(uint8_t unit, uint8_t code, uint16_t count, uint16_t status) {
	static tu_cmdpkt ek = { TUF_CTRL, TU_CTRL_LEN, TUO_END, 0, 0, 0, 0, 0, 0, -1 };

	ek.unit = unit;
	ek.modifier = code; // success/fail code
	ek.count = count;
	ek.block = status; // summary status

	putpacket((tu_packet *) &ek);
	serial_devtxflush(&tu58_serial); // finish packet transmit

	return;
}

//
// return requested block size of a tu58 access
//
static inline int32_t blocksize(uint8_t modifier) {
	return (modifier & TUM_B128) ? TU58_BLOCKSIZE / 4 : TU58_BLOCKSIZE;
}

//
// host seek of tu58
//
static void tuseek(tu_cmdpkt *pk) {
	image_t *img;
	// check unit number for validity
	img = tu58image_get(pk->unit);
	if (!img || !img->open) {
		error("tuseek bad unit %d", pk->unit);
		endpacket(pk->unit, TUE_BADU, 0, 0);
		return;
	}

	// offline = no cartridges
	if (tu58_offline) {
		endpacket(pk->unit, TUE_BADF, 0, 0);
		return;
	}

	// seek to desired block
	if (image_blockseek(img, blocksize(pk->modifier), pk->block, 0)) {
		error("tuseek unit %d bad block 0x%04X", pk->unit, pk->block);
		endpacket(pk->unit, TUE_BADB, 0, 0);
		return;
	}

	// fake a seek time
	delay_ms(tudelay[opt_timing].seek);

	// success if we get here
	endpacket(pk->unit, TUE_SUCC, 0, 0);

	return;
}

//
// host read from tu58
//
static void turead(tu_cmdpkt *pk) {
	int32_t count;
	tu_datpkt dk;
	image_t *img;

	// check unit number for validity
	img = tu58image_get(pk->unit);
	if (!img || !img->open) {
		error("turead bad unit %d", pk->unit);
		endpacket(pk->unit, TUE_BADU, 0, 0);
		return;
	}

	// offline = no cartridges
	if (tu58_offline) {
		endpacket(pk->unit, TUE_BADF, 0, 0);
		return;
	}

	// seek to desired ending block offset
	if (image_blockseek(img, blocksize(pk->modifier), pk->block, pk->count - 1)) {
		error("turead unit %d bad block 0x%04X", pk->unit, pk->block);
		endpacket(pk->unit, TUE_BADB, 0, 0);
		return;
	}

	// seek to desired starting block offset
	if (image_blockseek(img, blocksize(pk->modifier), pk->block, 0)) {
		error("turead unit %d bad block 0x%04X", pk->unit, pk->block);
		endpacket(pk->unit, TUE_BADB, 0, 0);
		return;
	}

	// fake a seek time
	delay_ms(tudelay[opt_timing].seek);

	// send data in packets until we run out
	for (count = pk->count; count > 0; count -= dk.length) {

		// max bytes to send at once is TU_DATA_LEN
		dk.flag = TUF_DATA;
		dk.length = count < TU_DATA_LEN ? count : TU_DATA_LEN;

		if (image_read(img, dk.data, dk.length) == dk.length) {
			// successful file read, send packet
			putpacket((tu_packet *) &dk);
			// fake a read time
			delay_ms(tudelay[opt_timing].read);
		} else {
			// whoops, something bad happened
			error("turead unit %d data error block 0x%04X count 0x%04X", pk->unit, pk->block,
					pk->count);
			endpacket(pk->unit, TUE_PARO, pk->count - count, 0);
			return;
		}
	}

	// success if we get here
	endpacket(pk->unit, TUE_SUCC, pk->count, 0);

	return;
}

//
// host write to tu58
//
static void tuwrite(tu_cmdpkt *pk) {
	int32_t count;
	int32_t status;
	tu_datpkt dk;
	image_t *img;

	// check unit number for validity
	img = tu58image_get(pk->unit);
	if (!img || !img->open) {
		error("tuwrite bad unit %d", pk->unit);
		endpacket(pk->unit, TUE_BADU, 0, 0);
		return;
	}

	// offline = no cartridges
	if (tu58_offline) {
		endpacket(pk->unit, TUE_BADF, 0, 0);
		return;
	}

	// seek to desired ending block offset
	if (image_blockseek(img, blocksize(pk->modifier), pk->block, pk->count - 1)) {
		error("tuwrite unit %d bad block 0x%04X", pk->unit, pk->block);
		endpacket(pk->unit, TUE_BADB, 0, 0);
		return;
	}

	// seek to desired starting block offset
	if (image_blockseek(img, blocksize(pk->modifier), pk->block, 0)) {
		error("tuwrite unit %d bad block 0x%04X", pk->unit, pk->block);
		endpacket(pk->unit, TUE_BADB, 0, 0);
		return;
	}

	// fake a seek time
	delay_ms(tudelay[opt_timing].seek);

	// keep looping if more data is expected
	for (count = pk->count; count > 0; count -= dk.length) {

		// send continue flag; we are ready for more data
		serial_devtxput(&tu58_serial, TUF_CONT);
		serial_devtxflush(&tu58_serial);
		if (opt_debug)
			info("sending <CONT>");

		uint8_t last;
		dk.flag = -1;

		// loop until we see data flag
		do {
			last = dk.flag;
			dk.flag = serial_devrxget(&tu58_serial);
			if (opt_debug)
				info("flag=0x%02X last=0x%02X", dk.flag, last);
			if (last == TUF_INIT && dk.flag == TUF_INIT) {
				// two in a row is special
				serial_devtxput(&tu58_serial, TUF_CONT); // send 'continue'
				serial_devtxflush(&tu58_serial); // send immediate
				if (opt_debug)
					info("<INIT><INIT> seen, sending <CONT>, abort write");
				return; // abort command
			} else if (dk.flag == TUF_CTRL) {
				error("protocol error, unexpected CTRL flag during write");
				endpacket(pk->unit, TUE_DERR, 0, 0);
				return;
			} else if (dk.flag == TUF_XOFF) {
				if (opt_debug)
					info("<XOFF> seen, stopping output");
				serial_devtxstop(&tu58_serial);
			} else if (dk.flag == TUF_CONT) {
				if (opt_debug)
					info("<CONT> seen, starting output");
				serial_devtxstart(&tu58_serial);
			}
		} while (dk.flag != TUF_DATA);

		// byte following data flag is packet data length
		dk.length = serial_devrxget(&tu58_serial);

		// get remainder of the data packet
		if (getpacket((tu_packet *) &dk)) {
			// whoops, checksum error, fail
			error("data checksum error");
			endpacket(pk->unit, TUE_DERR, 0, 0);
			return;
		}

		// write data packet to file
		if ((status = image_write(img, dk.data, dk.length)) != dk.length) {
			if (status == -2) {
				// whoops, unit is write protected
				error("tuwrite unit %d is write protected block 0x%04X count 0x%04X", pk->unit,
						pk->block, pk->count);
				endpacket(pk->unit, TUE_WPRO, pk->count - count, 0);
			} else {
				// whoops, some other data write error (like past EOF)
				error("tuwrite unit %d data write error block 0x%04X count 0x%04X", pk->unit,
						pk->block, pk->count);
				endpacket(pk->unit, TUE_PARO, pk->count - count, 0);
			}
			return;
		}

		// fake a write time
		delay_ms(tudelay[opt_timing].write);
	}

	// must fill out last block with zeros
	if ((count = pk->count % blocksize(pk->modifier)) > 0) {
		uint8_t buffer[TU58_BLOCKSIZE];
		bzero(buffer, (count = blocksize(pk->modifier) - count));
		if (opt_debug)
			info("tuwrite unit %d filling %d zeroes", pk->unit, count);
		if (image_write(img, buffer, count) != count) {
			// whoops, something bad happened
			error("tuwrite unit %d data error block 0x%04X count 0x%04X", pk->unit, pk->block,
					pk->count);
			endpacket(pk->unit, TUE_PARO, pk->count, 0);
			return;
		}
		// fake a write time
		delay_ms(tudelay[opt_timing].write);
	}

	// success if we get here
	endpacket(pk->unit, TUE_SUCC, pk->count, 0);

	return;
}

//
// decode and execute control packets
//
static void command(int8_t flag) {
	tu_cmdpkt pk;
	struct timespec time_start;
	struct timespec time_end;
	char *name = "none";
	uint8_t mode = 0;

	// avoid uninitialized variable warnings
	time_start.tv_sec = 0;
	time_start.tv_nsec = 0;
	time_end.tv_sec = 0;
	time_end.tv_nsec = 0;

	pk.flag = flag;
	pk.length = serial_devrxget(&tu58_serial);

	// check control packet length ... if too long flush it
	if (pk.length > sizeof(tu_cmdpkt)) {
		error("bad length 0x%02X in cmd packet", pk.length);
		reinit();
		return;
	}

	// check packet checksum ... if bad error it
	if (getpacket((tu_packet *) &pk)) {
		error("cmd checksum error");
		endpacket(pk.unit, TUE_DERR, 0, 0);
		return;
	}

	if (opt_debug)
		info("opcode=0x%02X length=0x%02X", pk.opcode, pk.length);

	// dump command if requested
	if (opt_verbose) {

		// parse commands to classes
		switch (pk.opcode) {
		case TUO_DIAGNOSE:
			name = "diagnose";
			mode = 1;
			break;
		case TUO_GETCHAR:
			name = "getchar";
			mode = 1;
			break;
		case TUO_INIT:
			name = "init";
			mode = 1;
			break;
		case TUO_NOP:
			name = "nop";
			mode = 1;
			break;
		case TUO_GETSTATUS:
			name = "getstat";
			mode = 1;
			break;
		case TUO_SETSTATUS:
			name = "setstat";
			mode = 1;
			break;
		case TUO_SEEK:
			name = "seek";
			mode = 2;
			break;
		case TUO_READ:
			name = "read";
			mode = 3;
			break;
		case TUO_WRITE:
			name = "write";
			mode = 3;
			break;
		default:
			name = "unknown";
			mode = 3;
			break;
		}

		// dump data
		switch (mode) {
		case 0:
			info("%-8s", name);
			break;
		case 1:
			info("%-8s unit=%d", name, pk.unit);
			break;
		case 2:
			info("%-8s unit=%d sw=0x%02X mod=0x%02X blk=0x%04X", name, pk.unit, pk.switches,
					pk.modifier, pk.block);
			break;
		case 3:
			info("%-8s unit=%d sw=0x%02X mod=0x%02X blk=0x%04X cnt=0x%04X", name, pk.unit,
					pk.switches, pk.modifier, pk.block, pk.count);
			break;
		}

		// get start time of processing
		clock_gettime(CLOCK_REALTIME, &time_start);

	}

	// if we are MRSP capable, look at the switches
	if (opt_mrspen)
		mrsp = (pk.switches & TUS_MRSP) ? 1 : 0;

	// decode packet
	switch (pk.opcode) {

	case TUO_READ: // read data from tu58
		turead(&pk);
		break;

	case TUO_WRITE: // write data to tu58
		tuwrite(&pk);
		break;

	case TUO_SEEK: // reposition tu58
		tuseek(&pk);
		break;

	case TUO_DIAGNOSE: // diagnose packet
		delay_ms(tudelay[opt_timing].test);
		endpacket(pk.unit, TUE_SUCC, 0, 0);
		break;

	case TUO_GETCHAR: // get characteristics packet
		delay_ms(tudelay[opt_timing].nop);
		if (opt_mrspen) {
			// MRSP capable just sends the end packet
			endpacket(pk.unit, TUE_SUCC, 0, 0);
		} else {
			// MRSP detect mode not enabled
			// indicate we are not MRSP capable
			tu_datpkt dk;
			dk.flag = TUF_DATA;
			dk.length = TU_CHAR_LEN;
			bzero(dk.data, dk.length);
			putpacket((tu_packet *) &dk);
		}
		break;

	case TUO_INIT: // init packet
		delay_ms(tudelay[opt_timing].init);
		serial_devtxinit(&tu58_serial);
		serial_devrxinit(&tu58_serial);
		endpacket(pk.unit, TUE_SUCC, 0, 0);
		break;

	case TUO_NOP: // nop packet
	case TUO_GETSTATUS: // get status packet
	case TUO_SETSTATUS: // set status packet
		delay_ms(tudelay[opt_timing].nop);
		endpacket(pk.unit, TUE_SUCC, 0, 0);
		break;

	default: // unknown packet
		delay_ms(tudelay[opt_timing].nop);
		endpacket(pk.unit, TUE_BADO, 0, 0);
		break;

	}

	if (opt_verbose) {

		uint32_t delta;

		// get end time of processing
		clock_gettime(CLOCK_REALTIME, &time_end);

		// compute elapsed time in milliseconds
		delta = 1000L * (time_end.tv_sec - time_start.tv_sec)
				+ (time_end.tv_nsec - time_start.tv_nsec) / 1000000L;
		if (delta == 0)
			delta = 1;

		// print elapsed time in milliseconds
		if (opt_debug)
			info("%-8s time=%dms", name, delta);

	}

	return;
}

//
// field requests from host
//
void* tu58_server(void* none) {
	uint8_t flag = TUF_NULL;
	uint8_t last = TUF_NULL;
	UNUSED(none);

	// some init
	reinit(); // empty serial line buffers
	tu58_doinit = !opt_nosync; // start sending init flags?

	tu58_offline_request = 0;
	tu58_offline = 0;

	// say hello
	info("emulator %sstarted", tu58_runonce++ ? "re" : "");

	// loop forever ... almost
	for (;;) {

		if (tu58_offline_request && !tu58_offline) {
			// if requested, go offline after inactivity timeout

			if (tu58_serial.rx_lasttime_ms + opt_offlinetimeout_sec * 1000 < now_ms()
					&& tu58_serial.tx_lasttime_ms + opt_offlinetimeout_sec * 1000 < now_ms()) {
				tu58_offline = 1;
				if (opt_verbose)
					info("TU58 now offline");
			}
		} else if (!tu58_offline_request && tu58_offline) {
			tu58_offline = 0;
			if (opt_verbose)
				info("TU58 now online");
		}
		// if offline, on read/write/seek a "no cartridge" is sent

		// loop while no characters are available
		if (serial_devrxavail(&tu58_serial) == 0) {
			// delays and printout only if not VAX
			if (!opt_vax) {
				// send INITs if still required
				if (tu58_doinit) {
					if (opt_debug)
						fprintf(ferr, ".");
					serial_devtxput(&tu58_serial, TUF_INIT);
					serial_devtxflush(&tu58_serial);
					tu58_serial.tx_lasttime_ms = 0; // does not count as traffic
					delay_ms(75);
				}
				delay_ms(25);
				continue; // loop again
			}
		} else
			tu58_doinit = 0; // quit sending init flags

#ifdef ORG
		// loop while no characters are available
		while (serial_devrxavail() == 0) {
			// delays and printout only if not VAX
			if (!opt_vax) {
				// send INITs if still required
				if (tu58_doinit) {
					if (opt_debug) fprintf(ferr, ".");
					serial_devtxput(TUF_INIT);
					serial_devtxflush();
					delay_ms(75);
				}
				delay_ms(25);
			}
		}
		tu58_doinit = 0; // quit sending init flags
#endif

		// process received characters
		last = flag;
		flag = serial_devrxget(&tu58_serial);
		if (opt_debug)
			info("flag=0x%02X last=0x%02X", flag, last);

		switch (flag) {

		case TUF_CTRL:
			// control packet - process
			command(flag);
			break;

		case TUF_INIT:
			// init flag
			if (opt_debug)
				info("<INIT> seen");
			if (last == TUF_INIT) {
				// two in a row is special
				if (!opt_vax)
					delay_ms(tudelay[opt_timing].init); // no delay for VAX
				serial_devtxput(&tu58_serial, TUF_CONT); // send 'continue'
				serial_devtxflush(&tu58_serial); // send immediate
				flag = -1; // undefined
				if (opt_debug)
					info("<INIT><INIT> seen, sending <CONT>");
			}
			break;

		case TUF_BOOT:
			// special boot sequence
			if (opt_debug)
				info("<BOOT> seen");
			bootio();
			break;

		case TUF_NULL:
			// ignore nulls (which are BREAKs)
			if (opt_debug)
				info("<NULL> seen");
			break;

		case TUF_CONT:
			// continue restarts output
			if (opt_debug)
				info("<CONT> seen, starting output");
			serial_devtxstart(&tu58_serial);
			break;

		case TUF_XOFF:
			// send disable flag stops output
			if (opt_debug)
				info("<XOFF> seen, stopping output");
			serial_devtxstop(&tu58_serial);
			break;

		case TUF_DATA:
			// data packet - should never see one here
			error("protocol error - data flag out of sequence");
			reinit();
			break;

		default:
			// whoops, protocol error
			error("unknown packet flag 0x%02X (%c)", flag,
			isprint(flag) ? flag : '.');
			break;

		} // switch (flag)

	} // for (;;)

	return (void*) 0;
}

//
// monitor for break/error on line, restart emulator if seen
//
void* tu58_monitor(void* none) {
	int32_t sts;
	uint64_t now;
	uint64_t next_sync_time;
	UNUSED(none) ;

	next_sync_time = now_ms() + opt_synctimeout_sec * 1000;
	for (;;) {

		// check for any error
		switch (sts = serial_devrxerror(&tu58_serial)) {
		case DEV_ERROR: // error
		case DEV_BREAK: // break
			// kill and restart the emulator
			if (opt_verbose)
				info("BREAK detected");
#ifdef THIS_DOES_NOT_YET_WORK_RELIABLY
			if (pthread_cancel(tu58_server))
			error("unable to cancel emulation thread");
			if (pthread_join(tu58_server, NULL))
			error("unable to join on emulation thread");
			if (pthread_create(&tu58_server, NULL, run, NULL))
			error("unable to restart emulation thread");
#endif // THIS_DOES_NOT_YET_WORK_RELIABLY
			break;
		case DEV_OK: // OK
			break;
		case DEV_NYI: // not yet implemented
			// return (void*)1;
			break;
		default: // something else...
			error("monitor(): unknown flag %d", sts);
			break;
		}
		now = now_ms();
		// image_*() routines have, mutex locking, so no change while saving possible
		if (next_sync_time < now
				&& tu58_serial.rx_lasttime_ms + opt_synctimeout_sec * 1000 < now
				&& tu58_serial.tx_lasttime_ms + opt_synctimeout_sec * 1000 < now) {
			// next sync time passed, and RS232 inactive
			tu58images_sync_all();
			next_sync_time = now + opt_synctimeout_sec * 1000;
		}

		// bit of a delay, loop again
		delay_ms(5);

	}

	return (void*) 0;
}

// the end
