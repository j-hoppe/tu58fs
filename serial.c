//
// serial.c - serial line and console control
//
// Original (C) 1984 Dan Ts'o <Rockefeller Univ. Dept. of Neurobiology>
// Update   (C) 2005-2016 Donald N North <ak6dn_at_mindspring_dot_com>
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// o Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// o Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// o Neither the name of the copyright holder nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
// TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// This is the TU58 emulation program written at Rockefeller Univ., Dept. of
// Neurobiology. We copyright (C) it and permit its use provided it is not
// sold to others. Originally written by Dan Ts'o circa 1984 or so.
//
#define _SERIAL_C_

//
// TU58 serial support routines
//

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <fcntl.h>

#include "error.h"
#include "utils.h"
#include "main.h"	// option flags
#include "serial.h"	// own

#ifdef MACOSX
#define IUCLC 0 // Not POSIX
#define OLCUC 0 // Not POSIX
#define CBAUD 0 // Not POSIX
#endif

#include <termios.h>

// console parameters
static struct termios consSave;

//
// stop transmission on output
//
void serial_devtxstop(serial_device_t *serial) {
	tcflow(serial->fd, TCOOFF);
	return;
}

//
// (re)start transmission on output
//
void serial_devtxstart(serial_device_t *serial) {
	tcflow(serial->fd, TCOON);
	return;
}

//
// set/clear break condition on output
//
void serial_devtxbreak(serial_device_t *serial) {
	tcsendbreak(serial->fd, 0);
	return;
}

//
// initialize tx serial buffers
//
void serial_devtxinit(serial_device_t *serial) {
	// flush all output
	tcflush(serial->fd, TCOFLUSH);

	// reset send buffer
	serial->wcnt = 0;
	serial->wptr = serial->wbuf;

	return;
}

//
// initialize rx serial buffers
//
void serial_devrxinit(serial_device_t *serial) {
	// flush all input
	tcflush(serial->fd, TCIFLUSH);

	// reset receive buffer
	serial->rcnt = 0;
	serial->rptr = serial->rbuf;

	return;
}

//
// wait for an error on the serial line
// return NYI, OK, BREAK, ERROR flag
//
int32_t serial_devrxerror(serial_device_t *serial) {
	// not implemented
	return DEV_NYI;
}

//
// return number of characters available
//
int32_t serial_devrxavail(serial_device_t *serial) {
	// get more characters if none available
	if (serial->rcnt <= 0) {
		serial->rcnt = read(serial->fd, serial->rbuf, sizeof(serial->rbuf));
		serial->rptr = serial->rbuf;
	}
	if (serial->rcnt < 0)
		serial->rcnt = 0;

	// return characters available
	return serial->rcnt;
}

//
// write characters direct to device
//
int32_t serial_devtxwrite(serial_device_t *serial, uint8_t *buf, int32_t cnt) {
	// write characters if asked, return number written
	int32_t result = 0;
	if (cnt > 0) {
		// write is monolitic and may take long
		// make sure serial_tx_lasttime_ms doe not time out
		serial->tx_lasttime_ms = now_ms() + 60000; // signal busy: 1 minute in the future
		result = write(serial->fd, buf, cnt);
		serial->tx_lasttime_ms = now_ms(); // now up to date
	}
	return result;
}

//
// send any outgoing characters in buffer
//
void serial_devtxflush(serial_device_t *serial) {
	int32_t acnt;

	// write any characters we have
	if (serial->wcnt > 0) {
		if ((acnt = serial_devtxwrite(serial, serial->wbuf, serial->wcnt)) != serial->wcnt)
			error("devtxflush(): write error, expected=%d, actual=%d", serial->wcnt, acnt);
	}

	// buffer is now empty
	serial->wcnt = 0;
	serial->wptr = serial->wbuf;

	// wait until all characters are transmitted
	tcdrain(serial->fd);

	return;
}

//
// return char from rbuf, wait until some arrive
//
uint8_t serial_devrxget(serial_device_t *serial) {
	// get more characters if none available
	while (serial_devrxavail(serial) <= 0)
		/*spin*/;

	serial->rx_lasttime_ms = now_ms(); // signal activity

	// count, return next character
	serial->rcnt--;
	return *(serial->rptr)++;
}

//
// put char on wbuf
//
void serial_devtxput(serial_device_t *serial, uint8_t c) {

	// must flush if hit the end of the buffer
	if (serial->wcnt >= sizeof(serial->wbuf))
		serial_devtxflush(serial);

	// count, add one character to buffer
	serial->wcnt++;
	*(serial->wptr)++ = c;

	serial->tx_lasttime_ms = now_ms(); // signal activity

	return;
}

//
// return baud rate mask for a given rate
//
static int32_t devbaud(int32_t rate) {
	static int32_t baudlist[] = { 3000000, B3000000, 2500000, B2500000, 2000000, B2000000,
			1500000, B1500000, 1152000, B1152000, 1000000, B1000000, 921600, B921600, 576000,
			B576000, 500000, B500000, 460800, B460800, 230400, B230400, 115200, B115200, 57600,
			B57600, 38400, B38400, 19200, B19200, 9600, B9600, 4800, B4800, 2400, B2400, 1200,
			B1200, 600, B600, 300, B300, -1, -1 };
	int32_t *p = baudlist;
	int32_t r;

	// search table for a baud rate match, return corresponding entry
	while ((r = *p++) != -1)
		if (r == rate)
			return *p;
		else
			p++;

	// not found ...
	return -1;
}

// split and check a format string like "8n1", "7e2", ...
// result: 0 = OK, else error
int serial_decode_format(char *formatstr, int *result_bitcount, char *result_parity,
		int *result_stopbits) {
	if (!formatstr || strlen(formatstr) != 3)
		return 1;
	switch (formatstr[0]) {
	case '5':
		*result_bitcount = 5 ;
		break ;
	case '6':
		*result_bitcount = 6 ;
		break ;
	case '7':
		*result_bitcount = 7 ;
		break ;
	case '8':
		*result_bitcount = 8 ;
		break ;
	default: return 1;
	}

	switch (toupper(formatstr[1])) {
	case 'O':
	case 'E':
	case 'N':
		*result_parity = toupper(formatstr[1]) ;
		break ;
	default: return 1;
	}

	switch (formatstr[2]) {
	case '1':
		*result_stopbits = 1 ;
		break ;
	case '2':
		*result_stopbits = 2 ;
		break ;
	default: return 1;
	}
	return 0 ; // all OK
}

//
// open/initialize serial port
//
void serial_devinit(serial_device_t *serial, char *port, int32_t speed, int32_t databits,
		char parity, int32_t stopbits) {
	serial->rx_lasttime_ms = 0;
	serial->tx_lasttime_ms = 0;

	// init unix serial port mode
	struct termios line;
	char name[64];
	unsigned int n;

	serial->baudrate = speed;
	serial->bitcount = 1 + databits + stopbits; // total bit count

	// open serial port
	int32_t euid = geteuid();
	int32_t uid = getuid();
	setreuid(euid, -1);
	if (sscanf(port, "%u", &n) == 1)
		sprintf(name, "/dev/ttyS%u", n - 1);
	else
		strcpy(name, port);
	if ((serial->fd = open(name, O_RDWR | O_NDELAY | O_NOCTTY)) < 0)
		fatal("no serial line [%s]", name);
	setreuid(uid, euid);

	// get current line params, error if not a serial port
	if (tcgetattr(serial->fd, &serial->lineSave))
		fatal("not a serial device [%s]", name);

	// copy current parameters
	line = serial->lineSave;

	// input param
	line.c_iflag &= ~( IGNBRK | BRKINT | IMAXBEL | INPCK | ISTRIP |
	INLCR | IGNCR | ICRNL | IXON | IXOFF |
	IUCLC | IXANY | PARMRK | IGNPAR);
	line.c_iflag |= (0);

	// output param
	line.c_oflag &= ~( OPOST | OLCUC | OCRNL | ONLCR | ONOCR |
	ONLRET | OFILL | CRDLY | NLDLY | BSDLY |
	TABDLY | VTDLY | FFDLY | OFDEL);
	line.c_oflag |= (0);

	// control param
	line.c_cflag &= ~( CBAUD | CSIZE | CSTOPB | PARENB | PARODD |
	HUPCL | CRTSCTS | CLOCAL | CREAD);
	line.c_cflag |= ( CLOCAL | CREAD);

	// explicitly set size
	switch (databits) {
	case 5: // who needs this?
		line.c_cflag |= CS5;
		break;
	case 6: // who needs this?
		line.c_cflag |= CS6;
		break;
	case 7:
		line.c_cflag |= CS7;
		break;
	default:
		line.c_cflag |= CS8;
		break;
	}

	// explicitly set parity
	switch (toupper(parity)) {
	case 'E':
		line.c_cflag |= PARENB;
		line.c_cflag &= ~PARODD;
		serial->bitcount++; // one extra bit
		break;
	case 'O':
		line.c_cflag |= PARENB;
		line.c_cflag |= PARODD;
		serial->bitcount++; // one extra bit
		break;
	default: // no parity
		line.c_cflag &= ~PARENB;
		line.c_cflag &= ~PARODD;
	}

	// set two stop bits if requested, else default to one
	if (stopbits == 2)
		line.c_cflag |= CSTOPB;

	// local param
	line.c_lflag &= ~( ISIG | ICANON | ECHO | ECHOE | ECHOK |
	ECHONL | NOFLSH | TOSTOP | IEXTEN | FLUSHO |
	ECHOKE | ECHOCTL);
	line.c_lflag |= (0);

	// timing/read param
	line.c_cc[VMIN] = 1; // return a min of 1 chars
	line.c_cc[VTIME] = 0; // no timer

	// flush all existing input data
	tcflush(serial->fd, TCIFLUSH);

	// set baud rate, if it is legal
	if (devbaud(speed) == -1) {
		error("illegal serial speed %d., ignoring", speed);
	} else {
		cfsetispeed(&line, devbaud(speed));
		cfsetospeed(&line, devbaud(speed));
	}

	// set new device parameters
	tcsetattr(serial->fd, TCSANOW, &line);

	// and non-blocking also
	if (fcntl(serial->fd, F_SETFL, FNDELAY) == -1)
		error("failed to set non-blocking read");

	// zap current data, if any
	serial_devtxinit(serial);
	serial_devrxinit(serial);

	return;
}

//
// restore/close serial port
//
void serial_devrestore(serial_device_t *serial) {
	tcsetattr(serial->fd, TCSANOW, &serial->lineSave);
	close(serial->fd);
	serial->fd = -1;
	return;
}

//
// set console line parameters
// raw = 0: normal console
// raw = 1: no key processing
void coninit(int rawmode) {
	struct termios cons;

	// background mode, don't do anything
	if (opt_background)
		return;

	// get current console parameters
	if (tcgetattr(fileno(stdin), &consSave))
		fatal("stdin not a serial device");

	// copy modes
	cons = consSave;

	// set new modes
	if (rawmode) {
		cfmakeraw(&cons);
	} else {
		cons.c_lflag &= ~( ICANON | ECHO);
	}

	// now set param
	tcflush(fileno(stdin), TCIFLUSH);
	tcsetattr(fileno(stdin), TCSANOW, &cons);

	// set non-blocking reads
	if (fcntl(fileno(stdin), F_SETFL, FNDELAY) == -1)
		error("stdin failed to set non-blocking read");

	return;
}

//
// restore console line parameters
//
void conrestore(void) {
	// background mode, don't do anything
	if (opt_background)
		return;

	// restore console mode to saved
	tcsetattr(fileno(stdin), TCSANOW, &consSave);
	return;
}

//
// get console character
//
int32_t conget(void) {
	char buf[1];
	int32_t s;

	// background mode, don't return anything
	if (opt_background)
		return -1;

	// try to read at most one char (may be none)
	s = read(fileno(stdin), buf, sizeof(buf));

	// if got a char return it, else return -1
	return s == 1 ? *buf : -1;
}

// the end
