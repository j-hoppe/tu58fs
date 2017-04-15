/* bootloader.h: deposit TU58 bootloader into monitor and start
 *
 *  Copyright (c) 2017, Joerg Hoppe
 *  j_hoppe@t-online.de, www.retrocmp.com
 *
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  - Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 *  - Neither the name of the copyright holder nor the names of its
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
 *
 *  23-Mar-2017  JH  created
 */
#define _BOOTLOADER_C_

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "error.h"
#include "serial.h"
#include "monitor.h"
#include "bootloader.h"     // own

/*	Assembler code of relocatable PDP-11 TU58 bootloader
 * |
 * |M9312 'DD' BOOT prom for TU58 D     MACRO V05.05  Tuesday 23-Mar-99 00:02  Page 1
 * |
 * |
 * |      1
 * |      2                                             .title  M9312 'DD' BOOT prom for TU58 DECtapeII serial tape controller (REVISED)
 * |      3
 * |      4                                             ; Original edition AK6DN Don North,
 * |      5                                             ; Further processed JH
 * |      6
 * |      7                                             ; This source code is an M9312 boot PROM for the TU58 version 23-765B9.
 * |      8                                             ;
 * |      9                                             ; This boot PROM is for the TU58 DECtapeII serial tape controller.
 * |     10                                             ;
 * |     11                                             ; Multiple units and/or CSR addresses are supported via different entry points.
 * |     12                                             ;
 * |     13                                             ; Standard devices are 82S131, Am27S13, 74S571 or other compatible bipolar
 * |     14                                             ; PROMs with a 512x4 TriState 16pin DIP architecture. This code resides in
 * |     15                                             ; the low half of the device; the top half is blank and unused.
 * |     16                                             ;
 * |     17                                             ; Alternatively, 82S129 compatible 256x4 TriState 16pin DIP devices can be
 * |     18                                             ; used, as the uppermost address line (hardwired low) is an active low chip
 * |     19                                             ; select (and will be correctly asserted low).
 * |     20
 * |     21
 * |     22                                             ;VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV
 * |     23                                             ;
 * |     24                                             ; The original code in 23-765A9 is REALLY BROKEN when it comes to
 * |     25                                             ; supporting a non-std CSR other than 776500 in R1 on entry
 * |     26                                             ;
 * |     27                                             ; All the hard references to:  ddrbuf, ddxcsr, ddxbuf
 * |     28                                             ; have been changed to:         2(R1),  4(R1),  6(R1)
 * |     29                                             ;
 * |     30                                             ; The one reference where 'ddrcsr' might have been used is '(R1)' instead
 * |     31                                             ; which is actually correct (but totally inconsistent with other usage).
 * |     32                                             ;
 * |     33                                             ;AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
 * |     34
 * |     35
 * |     36             176500                  ddcsr   =176500                         ; std TU58 csrbase
 * |     37
 * |     38             000000                  ddrcsr  =+0                             ; receive control
 * |     39             000002                  ddrbuf  =+2                             ; receive data
 * |     40             000004                  ddxcsr  =+4                             ; transmit control
 * |     41             000006                  ddxbuf  =+6                             ; transmit data
 * |     42
 * |     43     000000                                  .asect
 * |     44             010000                          .=10000
 * |     45
 * |     46                                             ; --------------------------------------------------
 * |     47
 * |     48     010000                          start:
 * |     49
 * |     50     010000  000261                  dd0n:   sec                             ; boot std csr, unit zero, no diags
 * |     51     010002  012700  000000          dd0d:   mov     #0,r0                   ; boot std csr, unit zero, with diags
 * |     52     010006  012701  176500          ddNr:   mov     #ddcsr,r1               ; boot std csr, unit <R0>
 * |     53
 * |     54
 * |     55                                     ; mov #101,r3   ; transmit A
 * |     56                                     ; mov r3,@#176506
 * |     57                                     ; halt
 * |M9312 'DD' BOOT prom for TU58 D    MACRO V05.05  Tuesday 23-Mar-99 00:02  Page 1-1
 * |
 * |
 * |     58                                     ;        call    xmtch                  ; transmit unit number
 * |     59                                     ; halt
 * |     60                                     ; mov #132,r3   ; transmit A
 * |     61                                     ;        call    xmtch                  ; transmit unit number
 * |     62                                     ; halt
 * |     63     010012  012706  002000          go:     mov     #2000,sp                ; setup a stack
 * |     64     010016  005004                          clr     r4                      ; zap old return address
 * |     65     010020  005261  000004                  inc     ddxcsr(r1)              ; set break bit
 * |     66     010024  005003                          clr     r3                      ; data 000,000
 * |     67     010026  004767  000050                  call    xmtch8                  ; transmit 8 zero chars
 * |     68     010032  005061  000004                  clr     ddxcsr(r1)              ; clear break bit
 * |     69     010036  005761  000002                  tst     ddrbuf(r1)              ; read/flush any stale rx char
 * |     70     010042  012703  004004                  mov     #<010*400>+004,r3       ; data 010,004
 * |     71     010046  004767  000034                  call    xmtch2                  ; transmit 004 (init) and 010 (boot)
 * |     72     010052  010003                          mov     r0,r3                   ; get unit number
 * |     73     010054  004767  000030                  call    xmtch                   ; transmit unit number
 * |     74
 * |     75     010060  005003                          clr     r3                      ; clear rx buffer ptr
 * |     76     010062  105711                  2$:     tstb    (r1)                    ; wait for rcv'd char available
 * |     77     010064  100376                          bpl     2$                      ; br if not yet
 * |     78     010066  116123  000002                  movb    ddrbuf(r1),(r3)+        ; store the char in buffer, bump ptr
 * |     79     010072  022703  001000                  cmp     #1000,r3                ; hit end of buffer (512. bytes)?
 * |     80     010076  101371                          bhi     2$                      ; br if not yet
 * |     81
 * |     82                                     ;        halt
 * |     83     010100  005007                          clr     pc                      ; jump to bootstrap at zero
 * |     84
 * |     85     010102                          xmtch8: ; transmit 4x the two chars in r3
 * |     86     010102  004717                          jsr     pc,(pc)                 ; recursive call for char replication
 * |     87     010104                          xmtch4:
 * |     88     010104  004717                          jsr     pc,(pc)                 ; recursive call for char replication
 * |     89     010106                          xmtch2: ; transmit 2 chars in lower r3, upper r3
 * |     90     010106  004717                          jsr     pc,(pc)                 ; recursive call for char replication
 * |     91     010110                          xmtch:  ; xmt char in lower r3, then swap r3
 * |     92     010110  105761  000004                  tstb    ddxcsr(r1)              ; wait for xmit buffer available
 * |     93     010114  100375                          bpl     xmtch                   ; br if not yet
 * |     94     010116  110361  000006                  movb    r3,ddxbuf(r1)           ; send the char
 * |     95     010122  000303                          swab    r3                      ; swap to other char
 * |     96     010124  000207                          return ; rts    pc              ; now recurse or return
 * |     97
 * |     98             000001                          .end
 * |M9312 'DD' BOOT prom for TU58 D    MACRO V05.05  Tuesday 23-Mar-99 00:02  Page 1-2
 * |Symbol table
 * |
 * |DDCSR = 176500      DDRCSR= 000000          DD0D    010002          START   010000          XMTCH4  010104
 * |DDNR    010006      DDXBUF= 000006          DD0N    010000          XMTCH   010110          XMTCH8  010102
 * |DDRBUF= 000002      DDXCSR= 000004          GO      010012          XMTCH2  010106
 * |
 * |. ABS.      010126    000   (RW,I,GBL,ABS,OVR)
 * |            000000    001   (RW,I,LCL,REL,CON)
 * |Errors detected:  0
 * |
 * |*** Assembler statistics
 * |
 * |
 * |Work  file  reads: 0
 * |Work  file writes: 0
 * |Size of work file: 48 Words  ( 1 Pages)
 * |Size of core pool: 15104 Words  ( 59 Pages)
 * |Operating  system: RT-11
 * |
 * |Elapsed time: 00:00:09.30
 * |DK:DDBOOT,DK:DDBOOT.LST=DK:DDBOOT

 */

/*** binary image of TU58 boot loader ***/
#define obj_code_word_count ((010124 - 010000)/2 + 1)
static uint16_t obj_code[obj_code_word_count] = { 0000261, 0012700, 0000000,
		0012701, 0176500, 0012706, 0002000, 0005004, 0005261, 0000004, 0005003,
		0004767, 0000050, 0005061, 0000004, 0005761, 0000002, 0012703, 0004004,
		0004767, 0000034, 0010003, 0004767, 0000030, 0005003, 0105711, 0100376,
		0116123, 0000002, 0022703, 0001000, 0101371, 0005007, 0004717, 0004717,
		0004717, 0105761, 0000004, 0100375, 0110361, 0000006, 0000303, 0000207 };


void bootloader_show_code(FILE *fout, int opt_boot_address) {
	unsigned i;
		fprintf(stdout, "# TU58 boot loader octal address/value dump\n");
		// dump out address/value pairs
		for (i = 0; i < obj_code_word_count; i++) {
			unsigned addr = opt_boot_address + 2 * i;
			unsigned value = obj_code[i];
			fprintf(stdout, "%06o %06o\n", addr, value);
		}
		fprintf(stdout, "\n");
}


/*
 * Download object code over monitor into PDP-11
 */
int bootloader_download(serial_device_t *serialdevice,
		monitor_type_t monitor_type, int opt_boot_address) {
	monitor_t monitor;
	unsigned i;

		if (monitor_init(&monitor, serialdevice, monitor_type, stdout))
			return error_set(ERROR_MONITOR,
					"monitor_init in bootloader_download");
		if (monitor_assert_prompt(&monitor))
			return error_set(ERROR_MONITOR,
					"Boot loader download: PDP-11 console prompt not found.");

		// dump out address/value pairs
		for (i = 0; i < obj_code_word_count; i++) {
			unsigned addr = opt_boot_address + 2 * i;
			unsigned value = obj_code[i];
			if (monitor_deposit(&monitor, addr, value))
				return error_set(ERROR_MONITOR, "bootloader_download");
		}
//monitor_trace_dump(stderr) ; //debug
		monitor_close(&monitor);
	return ERROR_OK;
}

/*
 * Start execution of monitor code
 */
int bootloader_go(serial_device_t *serialdevice, monitor_type_t monitor_type,
		int boot_address) {
	monitor_t monitor;
	if (monitor_init(&monitor, serialdevice, monitor_type, stdout))
		return error_set(ERROR_MONITOR, "monitor_init in bootloader_go");
	if (monitor_assert_prompt(&monitor))
		return error_set(ERROR_MONITOR,
				"monitor_assert_prompt in bootloader_go");
	if (monitor_go(&monitor, boot_address, 0))
		return error_set(ERROR_MONITOR, "monitor_go in bootloader_go");
	return ERROR_OK;
}

/* primitive teletype:
 * User "keyboard" input from stdin, display to stdout,
 * Interface to PDP-11 over pre-initialized "serialdevice".
 * Exit
 */
int run_teletype(serial_device_t *serialdevice, monitor_type_t monitor_type, char *userinfo) {
#define EXITKEY1 0x01
#define EXITKEY2 0x01

	monitor_t monitor;
	int ready = 0;
	int exitkey1_typed = 0; // 1 if 1st exit hotkey typed

	// no user echo to stdout by monitor_*() functions
	if (monitor_init(&monitor, serialdevice, monitor_type, NULL))
		return error_set(ERROR_MONITOR, "monitor_init in teletype");

	// console is in RAW mode: so do own CR LF
	fprintf(stdout,
			"\r\nTU58FS: teletype session to PDP-11 console opened.\r\n");
	fprintf(stdout,
			"TU58FS: Terminate teletype session with ^A ^A double key sequence.\r\n");
	if (userinfo)
		fprintf(stdout,	"TU58FS: %s\r\n", userinfo);
	while (!ready) {
		int status;
		fd_set readfds;
		struct timeval timeout;

		/*
		 * 1. read char from PDP, echo to user
		 */
		char *s = monitor_gets(&monitor, 0);
		if (s && strlen(s)) {
			// show unprocessed on Linux console
			fputs(s, stdout);
			fflush(stdout);
		}
		/*
		 * 2. if user keyboard input, xmt to PDP-11 and check for exit hot key
		 */
		// http://stackoverflow.com/questions/26948723/checking-the-stdin-buffer-if-its-empty
		FD_ZERO(&readfds);
		FD_SET(STDIN_FILENO, &readfds);
		timeout.tv_sec = timeout.tv_usec = 0; // zero timeout, just poll
		status = select(1, &readfds, NULL, NULL, &timeout); // rtfm
		if (status < 0) {
			error_set(ERROR_TTY, "Teletype keyboard polling,  errno=%u", errno);
		} else if (status > 0) {
			// single char input!
			char c = getchar();

			// test for exit hotkey sequence
			if (exitkey1_typed && c == EXITKEY2) {
				// terminate
				ready = 1;
			} else if (!exitkey1_typed && c == EXITKEY1) {
				exitkey1_typed = 1;
				// do not echo to PDP
			} else {
				unsigned char buff[2];
				// char in buff[0], send it to PDP.
				exitkey1_typed = 0;
				buff[0] = c;
				buff[1] = 0; // char -> string
				monitor_puts(&monitor, buff);
			}
		}
	}

	monitor_close(&monitor);
	fprintf(stdout, "\r\nTU58FS: teletype session closed.\r\n");
	return ERROR_OK;
}
