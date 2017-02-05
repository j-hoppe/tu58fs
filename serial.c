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

#define	BUFSIZE	256	// size of serial line buffers (bytes, each way)

// last time something was received/transmitted
// if > now: transmit in progress
uint64_t serial_rx_lasttime_ms ;
uint64_t serial_tx_lasttime_ms ;

// serial output buffer
static uint8_t wbuf[BUFSIZE];
static uint8_t *wptr;
static int32_t wcnt;

// serial input buffer
static uint8_t rbuf[BUFSIZE];
static uint8_t *rptr;
static int32_t rcnt;

// serial device descriptor, default to nada
static int32_t device = -1;
// async line parameters
static struct termios lineSave;

// console parameters
static struct termios consSave;



//
// stop transmission on output
//
void devtxstop (void)
{
    tcflow(device, TCOOFF);
    return;
}



//
// (re)start transmission on output
//
void devtxstart (void)
{
    tcflow(device, TCOON);
    return;
}



//
// set/clear break condition on output
//
void devtxbreak (void)
{
    tcsendbreak(device, 0);
    return;
}



//
// initialize tx serial buffers
//
void devtxinit (void)
{
    // flush all output
    tcflush(device, TCOFLUSH);

    // reset send buffer
    wcnt = 0;
    wptr = wbuf;

    return;
}



//
// initialize rx serial buffers
//
void devrxinit (void)
{
    // flush all input
    tcflush(device, TCIFLUSH);

    // reset receive buffer
    rcnt = 0;
    rptr = rbuf;

    return;
}



//
// wait for an error on the serial line
// return NYI, OK, BREAK, ERROR flag
//
int32_t devrxerror (void)
{
    // not implemented
    return DEV_NYI;
}



//
// return number of characters available
//
int32_t devrxavail (void)
{
    // get more characters if none available
    if (rcnt <= 0) {
	rcnt = read(device, rbuf, sizeof(rbuf));
	rptr = rbuf;
    }
    if (rcnt < 0) rcnt = 0;

    // return characters available
    return rcnt;
}



//
// write characters direct to device
//
int32_t devtxwrite (uint8_t *buf,
		    int32_t cnt)
{
    // write characters if asked, return number written
	int32_t result = 0 ;
    if (cnt > 0) {
		// write is monolitic and may take long
		// make sure serial_tx_lasttime_ms doe not time out
		serial_tx_lasttime_ms = now_ms() + 60000 ; // signal busy: 1 minute in the future
	    result = write(device, buf, cnt);
	    serial_tx_lasttime_ms = now_ms() ; // now up to date
    }
    return result;
}



//
// send any outgoing characters in buffer
//
void devtxflush (void)
{
    int32_t acnt;

    // write any characters we have
    if (wcnt > 0) {
	if ((acnt = devtxwrite(wbuf, wcnt)) != wcnt)
	    error("devtxflush(): write error, expected=%d, actual=%d", wcnt, acnt);
    }

    // buffer is now empty
    wcnt = 0;
    wptr = wbuf;

    // wait until all characters are transmitted
    tcdrain(device);

    return;
}



//
// return char from rbuf, wait until some arrive
//
uint8_t devrxget (void)
{
    // get more characters if none available
    while (devrxavail() <= 0) /*spin*/;

    serial_rx_lasttime_ms = now_ms() ; // signal activity

    // count, return next character
    rcnt--;
    return *rptr++;
}



//
// put char on wbuf
//
void devtxput (uint8_t c)
{

    // must flush if hit the end of the buffer
    if (wcnt >= sizeof(wbuf)) devtxflush();

    // count, add one character to buffer
    wcnt++;
    *wptr++ = c;

    serial_tx_lasttime_ms = now_ms() ; // signal activity

    return;
}



//
// return baud rate mask for a given rate
//
static int32_t devbaud (int32_t rate)
{
    static int32_t baudlist[] = { 3000000, B3000000,
				  2500000, B2500000,
				  2000000, B2000000,
				  1500000, B1500000,
				  1152000, B1152000,
				  1000000, B1000000,
				  921600,  B921600,
				  576000,  B576000,
				  500000,  B500000,
				  460800,  B460800,
				  230400,  B230400,
				  115200,  B115200,
				  57600,   B57600,
				  38400,   B38400,
				  19200,   B19200,
				  9600,    B9600,
				  4800,    B4800,
				  2400,    B2400,
				  1200,    B1200,
			          -1,	    -1 };
    int32_t *p = baudlist;
    int32_t r;

    // search table for a baud rate match, return corresponding entry
    while ((r = *p++) != -1) if (r == rate) return *p; else p++;

    // not found ...
    return -1;
}



//
// open/initialize serial port
//
void devinit (char *port,
	      int32_t speed,
	      int32_t stop)
{
    serial_rx_lasttime_ms = 0;
    serial_tx_lasttime_ms = 0;


    // init unix serial port mode
    struct termios line;
    char name[64];
    unsigned int n;

    // open serial port
    int32_t euid = geteuid();
    int32_t uid = getuid();
    setreuid(euid, -1);
    if (sscanf(port, "%u", &n) == 1) sprintf(name, "/dev/ttyS%u", n-1); else strcpy(name, port);
    if ((device = open(name, O_RDWR|O_NDELAY|O_NOCTTY)) < 0) fatal("no serial line [%s]", name);
    setreuid(uid, euid);

    // get current line params, error if not a serial port
    if (tcgetattr(device, &lineSave)) fatal("not a serial device [%s]", name);

    // copy current parameters
    line = lineSave;

    // input param
    line.c_iflag &= ~( IGNBRK | BRKINT | IMAXBEL | INPCK | ISTRIP |
		       INLCR  | IGNCR  | ICRNL   | IXON  | IXOFF  |
		       IUCLC  | IXANY  | PARMRK  | IGNPAR );
    line.c_iflag |=  ( 0 );

    // output param
    line.c_oflag &= ~( OPOST  | OLCUC | OCRNL | ONLCR | ONOCR |
		       ONLRET | OFILL | CRDLY | NLDLY | BSDLY |
		       TABDLY | VTDLY | FFDLY | OFDEL );
    line.c_oflag |=  ( 0 );

    // control param
    line.c_cflag &= ~( CBAUD  | CSIZE | CSTOPB  | PARENB | PARODD |
		       HUPCL | CRTSCTS | CLOCAL | CREAD );
    line.c_cflag |=  ( CLOCAL | CREAD | CS8 );

    // set two stop bits if requested, else default to one
    if (stop == 2) line.c_cflag |= CSTOPB;

    // local param
    line.c_lflag &= ~( ISIG   | ICANON  | ECHO   | ECHOE  | ECHOK  |
		       ECHONL | NOFLSH  | TOSTOP | IEXTEN | FLUSHO |
		       ECHOKE | ECHOCTL );
    line.c_lflag |=  ( 0 );

    // timing/read param
    line.c_cc[VMIN] = 1; // return a min of 1 chars
    line.c_cc[VTIME] = 0; // no timer

    // flush all existing input data
    tcflush(device, TCIFLUSH);

    // set baud rate, if it is legal
    if (devbaud(speed) == -1) {
	error("illegal serial speed %d., ignoring", speed);
    } else {
	cfsetispeed(&line, devbaud(speed));
	cfsetospeed(&line, devbaud(speed));
    }

    // set new device parameters
    tcsetattr(device, TCSANOW, &line);

    // and non-blocking also
    if (fcntl(device, F_SETFL, FNDELAY) == -1)
	error("failed to set non-blocking read");

    // zap current data, if any
    devtxinit();
    devrxinit();

    return;
}



//
// restore/close serial port
//
void devrestore (void)
{
    tcsetattr(device, TCSANOW, &lineSave);
    close(device);
    device = -1;
    return;
}



//
// set console line parameters
//
void coninit (void)
{
    struct termios cons;

    // background mode, don't do anything
    if (opt_background) return;

    // get current console parameters
    if (tcgetattr(fileno(stdin), &consSave))
	fatal("stdin not a serial device");

    // copy modes
    cons = consSave;

    // set new modes
    cons.c_lflag &= ~( ICANON | ECHO );

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
void conrestore (void)
{
    // background mode, don't do anything
    if (opt_background) return;

    // restore console mode to saved
    tcsetattr(fileno(stdin), TCSANOW, &consSave);
    return;
}



//
// get console character
//
int32_t conget (void)
{
    char buf[1];
    int32_t s;

    // background mode, don't return anything
    if (opt_background) return -1;

    // try to read at most one char (may be none)
    s = read(fileno(stdin), buf, sizeof(buf));

    // if got a char return it, else return -1
    return s == 1 ? *buf : -1;
}



// the end
