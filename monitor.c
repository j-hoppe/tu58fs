/* monitor.c: interface to different PDP-11 console monitors
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
 *  07-May-2017  JH  passes GCC warning levels -Wall -Wextra
 *  24-Mar-2017  JH  created
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>

#include "error.h"
#include "utils.h"

#include "serial.h"
#include "monitor.h"	// own

#define CR	"\r"
#define LF	"\n"

// #define TRACE	// just a debugging tool

#define TRACE_OUT 0
#define TRACE_IN 1
#define TRACE_STATUS 2

#ifdef TRACE
typedef struct {
	uint64_t timestamp_us;
	int type; // In,OUT,STATUS
	char *data;
	char *info;
} trace_entry_t;

#define TRACE_ENTRY_COUNT	1000
static trace_entry_t *trace_entry[TRACE_ENTRY_COUNT + 1];
#endif

// "serial" is an open port
// set switch signals to stop running CPU
// check "tty" is internal
// result: 0 = OK
int monitor_init(monitor_t *_this, serial_device_t *serial, monitor_type_t monitor_type,
		FILE *fecho) {
	_this->serial = serial;
	_this->fecho = fecho;
	_this->monitor_type = monitor_type;
#ifdef TRACE
	trace_entry[0] = NULL; // empty list
#endif
	switch (_this->monitor_type) {
	case monitor_odt:
		_this->prompt = "@";
		break;
	case monitor_m9312:
		_this->prompt = "@";
		break;
	case monitor_m9301:
		_this->prompt = "$"; // and NUL appended
		break;
	default:
		return error_set(ERROR_MONITOR, "monitor_init(): Illegal boot monitor %u selected",
				monitor_type);
	}
//	_this->responsetime_us = 200000; // 1/5 th seconds
			// calc time for one bit from baudrate. 1 char = 10 bits
	_this->chartime_us = (1000000 * serial->bitcount) / _this->serial->baudrate;
	//
	// Wait for CPU reponse: 10 milli seconds CPU
	// 1 char time transfer to CPU, 1 char response, 1 char reserve
	_this->responsetime_us = 10000 + 5 * _this->chartime_us;
	// worst case: M9301 at 300 baud: needs 10 chars?!
	_this->responsetime_us = 10000 + 10 * _this->chartime_us;

	_this->last_address = 0xffffffff; // invalidate

	return ERROR_OK;
}

void monitor_close(monitor_t *_this) {
	UNUSED(_this) ;
	// tty closed by caller
	monitor_trace_clear();
}

void monitor_trace_clear() {
#ifdef TRACE
	int i;
	for (i = 0; trace_entry[i]; i++) {
		trace_entry_t *te = trace_entry[i];
		free(te->data);
		free(te->info);
		free(te);
	}
	trace_entry[0] = NULL; // empty list
#endif
}

// write and clear
// idx |   from/to
void monitor_trace_dump(FILE *f) {
#ifdef TRACE
	int colwidth = 40;
	char line[256];
	uint64_t prev_us;
	int i;
	trace_entry_t *te;
	// two columns;: to PDP | from PDP

	fprintf(f, "\n");

	for (i = 0; trace_entry[i]; i++) {
		trace_entry_t *te = trace_entry[i];
		char *s_type;
		if (te->type == TRACE_IN)
		s_type = "RCV from PDP";
		else if (te->type == TRACE_OUT)
		s_type = "XMT to   PDP";
		else
		s_type = "STATUS";

		fprintf(f, "#%4u | dt = %9llu us | %12s | %30s | %s\n", i,
				(uint64_t)(i == 0 ? 0 : te->timestamp_us - prev_us),
				s_type,
				te->data,
				te->info);
		prev_us = te->timestamp_us;
	}

	monitor_trace_clear();
#else
	UNUSED(f) ;
#endif
}

// registers serial xmt/rcv for protocol debugging
// direction 1 = "In" = received from PDP
// direction 0 = "Out" = transmitted to PDP
// info: code in monitor logic
static void monitor_trace(char *data, unsigned data_size, int type, char *infofmt, ...) {
#ifdef TRACE
	va_list args;
	char buff[1024];
	trace_entry_t *te;
	int i;
	// do not log empty data
	if (type != TRACE_STATUS && ( data == NULL || data_size == 0))
	return;// no

	// find end of trace
	i = 0;
	while (i < TRACE_ENTRY_COUNT && trace_entry[i] != NULL)
	i++;
	if (i >= TRACE_ENTRY_COUNT) {
		monitor_trace_dump(stderr);
		fatal("monitor trace overflow");
	}
	te = trace_entry[i++] = malloc(sizeof(trace_entry_t));
	trace_entry[i] = NULL; //terminate list

	te->timestamp_us = now_us();
	te->type = type;
	te->data = strdup(strprintable(data, data_size));

	va_start(args, infofmt);
	vsprintf(buff, infofmt, args);
	va_end(args);
	te->info = strdup(buff);
#else
	UNUSED(data) ;
	UNUSED(data_size) ;
	UNUSED(type) ;
	UNUSED(infofmt) ;
#endif
}

// wait for char on serial port
int monitor_getc(monitor_t *_this, int timeout_us) {
	char tmpbuff[2];
	int n;
	int timeout ;
	timeout_set(timeout_us);
	// "Busy waiting" loop, as delay() routines are unreliable under CygWin.
	do {
		// Fix for slow single thread CPUs (BBB):
		// test timeout BEFORE read()
		timeout = timeout_reached() ;
		fcntl(_this->serial->fd, F_SETFL, FNDELAY);
		tmpbuff[0] = 0; // clear
		n = read(_this->serial->fd, tmpbuff, 1);
//		if (n < 0)
//			return error_set(ERROR_MONITOR, "monitor_getc() failed with errno=%u", errno);
		if (n > 0) {
			// monitor_trace(tmpbuff, n, TRACE_IN, "monitor_getc") ;
			return tmpbuff[0];
		}
	} while (n <= 0 && !timeout);
	return EOF;
}

// wait for "@" or timeout
// if "last_answer: check this string first
// Uses a moving string window for endless PDP-11 output

static int monitor_wait_prompt(monitor_t *_this, char *last_answer) {
	char buff[256];
	int rounds = 5;
	char *s;

	buff[0] = 0;
	if (last_answer)
		strcat(buff, last_answer);
monitor_trace(NULL, 0, TRACE_STATUS, "monitor_wait_prompt():responsetime_us=%u", _this->responsetime_us) ;
	do {
		s = monitor_gets(_this, _this->responsetime_us);
		if (s) {
			// append to buffer, clip off first chars
			int n = strlen(s);
			if (n + strlen(buff) + 1 >= sizeof(buff))
				memmove(buff, buff + n, sizeof(buff) - n);
			strcat(buff, s);
		}
		// find prompt in string
		if (strstr(buff, _this->prompt) != NULL)
			return ERROR_OK; // found
	} while (--rounds);

	monitor_trace_dump(ferr);
	return STATUS_MONITOR_NOPROMPT; // not found after timeout
}


// check for @ prompt
// M9312 needs 2x CR to show "@"
int monitor_assert_prompt(monitor_t *_this) {
	int res ;
	monitor_puts(_this, CR); // hit "ENTER"
	if (monitor_wait_prompt(_this, NULL) == ERROR_OK)
		return ERROR_OK; // first CR successful

	// no prompt. try 2nd CR (M9312)
	monitor_puts(_this, CR); // hit "ENTER"
	res = monitor_wait_prompt(_this, NULL) ;

	if (res == ERROR_OK)
		return ERROR_OK; // 2nd CR successful
	else if (res > 0) {  // no prompt: return status
		monitor_trace_dump(ferr);
		return res ;
	} else { // error
		monitor_trace_dump(ferr);
		return error_set(res, "monitor_assert_prompt(): no %s-prompt on tty %u",
				_this->prompt, _this->serial->fd);
	}
}

// gets()
// fetch data send from PDP with max RS232 speed.
// wait "timeout" for 1st char, then wait for RS232 transmission period.
char *monitor_gets(monitor_t *_this, int timeout_us) {
	static char buffer[10240];
	unsigned charcount;
	int c;

	charcount = 0;
	buffer[0] = 0; // clear
	// "Busy waiting" loop, as delay() routines are unreliable under CygWin.
	do {
		c = monitor_getc(_this, timeout_us);
		if (c != EOF) {
			// append char to string, new timeout
			buffer[charcount++] = (char) c;
			buffer[charcount] = 0;
			if (charcount + 1 >= sizeof(buffer))
				fatal("monitor_gets(): buffer overflow");
			if (_this->fecho)
				fputc(c, _this->fecho);
			timeout_us = 3 * _this->chartime_us; // next chars full RS232 speed
		}
	} while (c != EOF);
	monitor_trace(buffer, charcount, TRACE_IN, "monitor_gets");

	return buffer;
}

int monitor_puts(monitor_t *_this, char *s) {
	int n;
	n = strlen(s);
	n = write(_this->serial->fd, s, n);
	if (n < 0) {
		monitor_trace_dump(ferr);
		return error_set(ERROR_MONITOR, "serial_puts() failed");
	}
	monitor_trace(s, n, TRACE_OUT, "monitor_puts");
	return ERROR_OK;
	// do not echo output, PDP-11 should do this
}

// write a string and verify echo from PDP-11
// fails, if PDP-11 is still transmitting other chars
int monitor_puts_echocheck(monitor_t *_this, char *s, int timeout_us) {
	int i, m, n;
	if (s == NULL || strlen(s) == 0)
		return ERROR_OK;
	n = strlen(s);

	// wait max "timeout" for every char
	for (i = 0; i < n; i++) {
		// send a single char
		char c = s[i];
		int echo_found;

		m = write(_this->serial->fd, &c, 1);
		if (m <= 0)
			return error_set(ERROR_MONITOR, "serial_putc() failed");
		monitor_trace(&c, TRACE_IN, 0, "monitor_puts_echocheck");

		// wait until echo or timeout
		echo_found = 0;
		timeout_set(timeout_us);

		while (!echo_found && !timeout_reached()) {
			char tmpbuff[2];

			fcntl(_this->serial->fd, F_SETFL, FNDELAY);
			tmpbuff[0] = 0; // clear
			m = read(_this->serial->fd, tmpbuff, 1);
			if (m > 0)
				monitor_trace(tmpbuff, m, TRACE_IN, "monitor_puts_echocheck");
			// else if (m < 0)
			//	monitor_trace(NULL, 0, TRACE_STATUS, "monitor_puts_echocheck");

			if (m == 1) {
				if (_this->fecho)
					fputc(tmpbuff[0], _this->fecho);

				echo_found = 1;
				if (c != tmpbuff[0]) {
					monitor_trace_dump(ferr);
					return error_set(ERROR_MONITOR,
							"monitor_puts_echoed(): sent 0x%x, received 0x%x.", c, tmpbuff[0]);
				}
			}
		}
		if (!echo_found) {
			monitor_trace(NULL, 0, TRACE_STATUS, "monitor_puts_echoed(): no echo");
			monitor_trace_dump(ferr);
			return error_set(ERROR_MONITOR,
					"monitor_puts_echoed(): sent 0x%x, no echo after %0.1f ms.", c,
					timeout_us / 1000.0);
		}
	}
	return ERROR_OK;
}


// deposit memory cells
// LSI ODT "@" prompt must have been verified!
static int monitor_odt_deposit(monitor_t *_this, unsigned address, unsigned value) {
	char buffer[256];
	char *answer;

	// write <address>/
	sprintf(buffer, "%o/", address);
	if (monitor_puts_echocheck(_this, buffer, _this->responsetime_us)) // verify address
		return error_code;
	// LSI ODT responds with current value, or ?
	answer = monitor_gets(_this, _this->responsetime_us); // read whole answer
	// check for ?. if memory inaccessible
	if (strchr(answer, '?') != NULL) {
		monitor_trace_dump(ferr);
		return error_set(ERROR_MONITOR, "Error reading addr %o\n", address);
	} else {
		// deposit
		sprintf(buffer, "%o", value);
		if (monitor_puts_echocheck(_this, buffer, _this->responsetime_us)) // verify digits
			return error_code;
		monitor_puts(_this, CR);
		answer = monitor_gets(_this, _this->responsetime_us); // read prompt
	}
	// answer is now the "?\n@" or the "\n@". verify
	if (monitor_wait_prompt(_this, answer)) {
		return error_set(ERROR_MONITOR, "ODT deposit: @-prompt not found");
	}
	return ERROR_OK;
}

// Start a program.
// after odt_init(), HALT is set and  ODT prompt is there
// go back to RUN,
// then type "<addr>G".
// if "no_go", then the final "G" is not send
// result: > 0, if success
static int monitor_odt_go(monitor_t *_this, unsigned address, int no_go) {
	char buffer[256];
	char *s;

	if (no_go) {
		sprintf(buffer, "%o", address);
		monitor_puts_echocheck(_this, buffer, _this->responsetime_us);
	} else {
		sprintf(buffer, "%oG", address);
		// No CR, "G" starts program execution immediately
		monitor_puts_echocheck(_this, buffer, _this->responsetime_us);
		// echo all command output, wait 1 second
		do {
			s = monitor_gets(_this, _this->responsetime_us);
		} while (s && strlen(s));
	}
	return ERROR_OK;
}

// write "L <address>", wait for prompt
static int monitor_m9312_load_addr(monitor_t *_this, unsigned address) {
	char buffer[256];
	char *answer;
	_this->last_address = 0xffffffff; // invalidate
	sprintf(buffer, "L %o", address);
	if (monitor_puts_echocheck(_this, buffer, _this->responsetime_us)) // verify address
		return error_code;
	monitor_puts(_this, CR);
	answer = monitor_gets(_this, _this->responsetime_us); // read prompt
	if (monitor_wait_prompt(_this, answer))
		return error_set(ERROR_MONITOR, "M9312/M9301 address load: %s-prompt not found",
				_this->prompt);
	_this->last_address = address;
	return ERROR_OK;
}

// deposit memory cells
// M9312 "@" prompt or M9301 "$NUL" prompt must have been verified!
static int monitor_m9312_deposit(monitor_t *_this, unsigned address, unsigned value) {
	char buffer[256];
	char *answer;

	// write L <address>, if different from previous
	if (address != _this->last_address + 2) {
		if (monitor_m9312_load_addr(_this, address))
			return error_code;
	} else
		_this->last_address = address; // deposit increments the "current address"

	// deposit
	sprintf(buffer, "D %o", value);
	if (monitor_puts_echocheck(_this, buffer, _this->responsetime_us)) // verify digits
		return error_code;
	monitor_puts(_this, CR);
	answer = monitor_gets(_this, _this->responsetime_us); // read prompt
	if (monitor_wait_prompt(_this, answer))
		return error_set(ERROR_MONITOR, "M9312/M9301 deposit: %s-prompt not found",
				_this->prompt);



	return ERROR_OK;
}

// Start a program.
// after odt_init(), HALT is set and  ODT prompt is there
// go back to RUN,
// then type "<addr>G".
// if "no_go", then the final "G" is not send
// result: > 0, if success
static int monitor_m9312_go(monitor_t *_this, unsigned address, int no_go) {
	char *s;

	// set start address
	if (monitor_m9312_load_addr(_this, address))
		return error_code;

	// write the "S"
	monitor_puts_echocheck(_this, "S", _this->responsetime_us);

	if (!no_go) {
		monitor_puts(_this, CR);
		// L<CR> starts program execution
		// echo all command output, wait 1 second
		do {
			s = monitor_gets(_this, _this->responsetime_us);
		} while (s && strlen(s));
	}
	return ERROR_OK;
}

/*
 * Function distribution to different monitor logic
 */
int monitor_deposit(monitor_t *_this, unsigned address, unsigned value) {
	switch (_this->monitor_type) {
	case monitor_odt:
		return monitor_odt_deposit(_this, address, value);
	case monitor_m9312:
	case monitor_m9301: // only difference is the prompt
		return monitor_m9312_deposit(_this, address, value);
	default:
		return error_set(ERROR_MONITOR, "monitor_deposit(): Illegal boot monitor %u selected",
				_this->monitor_type);
	}
}

int monitor_go(monitor_t *_this, unsigned address, int no_go) {
	switch (_this->monitor_type) {
	case monitor_odt:
		return monitor_odt_go(_this, address, no_go);
	case monitor_m9312:
	case monitor_m9301: // only difference is the prompt
		return monitor_m9312_go(_this, address, no_go);
	default:
		return error_set(ERROR_MONITOR, "monitor_go(): Illegal boot monitor %u selected",
				_this->monitor_type);
	}
}

