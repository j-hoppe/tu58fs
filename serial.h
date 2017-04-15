/*
 * serial.h
 *
 *  Created on: 13.01.2017
 *      Author: root
 */

#ifndef _SERIAL_H_
#define _SERIAL_H_

#include <stdint.h>
#include <termios.h>

#define DEV_NYI		-1	// not yet implemented
#define DEV_OK		 0	// no error
#define DEV_BREAK	 1	// BREAK on line
#define DEV_ERROR	 2	// ERROR on line

#define	SERIAL_BUFSIZE	256	// size of serial line buffers (bytes, each way)

typedef struct {
	// serial device descriptor, default to nada
	int32_t fd; // file descriptor
	int baudrate;
	int bitcount; // start + data + parity + stop

	// last time something was received/transmitted
	// if > now: transmit in progress
	uint64_t rx_lasttime_ms;
	uint64_t tx_lasttime_ms;

	// serial output buffer
	uint8_t wbuf[SERIAL_BUFSIZE];
	uint8_t *wptr;
	int32_t wcnt;

	// serial input buffer
	uint8_t rbuf[SERIAL_BUFSIZE];
	uint8_t *rptr;
	int32_t rcnt;

	// async line parameters
	struct termios lineSave;
} serial_device_t;

int serial_decode_format(char *formatstr, int *result_bitcount, char *result_parity,
		int *result_stopbits);
void serial_devinit(serial_device_t *serial, char *port, int32_t speed, int32_t databits,
		char parity, int32_t stopbits);
void serial_devrestore(serial_device_t *serial);

void serial_devtxbreak(serial_device_t *serial);
void serial_devtxstop(serial_device_t *serial);
void serial_devtxstart(serial_device_t *serial);
void serial_devtxinit(serial_device_t *serial);
void serial_devtxflush(serial_device_t *serial);
void serial_devtxput(serial_device_t *serial, uint8_t);
int32_t serial_devtxwrite(serial_device_t *serial, uint8_t *, int32_t);
void serial_devrxinit(serial_device_t *serial);
int32_t serial_devrxavail(serial_device_t *serial);
int32_t serial_devrxerror(serial_device_t *serial);
uint8_t serial_devrxget(serial_device_t *serial);

void coninit(int rawmode);
void conrestore(void);
int32_t conget(void);

#endif /* SERIAL_H_ */
