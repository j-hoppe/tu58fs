/* monitor.h: interface to different PDP-11 console monitors
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
 *  24-Mar-2017  JH  created
 *
 */
#ifndef _MONITOR_H_
#define _MONITOR_H_

#include <stdio.h>
#include "serial.h"

typedef enum {
	monitor_none = 0,
	monitor_showcode = 1,
	monitor_odt = 2,
	monitor_m9312 = 3,
	monitor_m9301 = 4
} monitor_type_t;

// monitor state machine
typedef struct {
	monitor_type_t monitor_type;
	char *prompt; // prompt string "@"
	serial_device_t *serial; // serial PDP-11 console port
	int responsetime_us; // time to wait for PDP_11 response after command
	int chartime_us; // time for one serial char in micro secs
	FILE *fecho; // if set: stream for user echo
	unsigned last_address; // last address set with console cmd (M9312 "L")
} monitor_t;

int monitor_init(monitor_t *_this, serial_device_t *serial,
		monitor_type_t monitor_type, FILE *hostconsole);
int monitor_assert_prompt(monitor_t *_this);
char *monitor_gets(monitor_t *_this, int timeout_us);
int monitor_puts(monitor_t *_this, char *s);
int monitor_deposit(monitor_t *_this, unsigned address, unsigned value);
int monitor_go(monitor_t *_this, unsigned address, int no_enter);

void monitor_close(monitor_t *_this);

void monitor_trace_clear(void);
void monitor_trace_dump(FILE *f); //debug

#endif
