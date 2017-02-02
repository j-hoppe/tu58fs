/*
 * serial.h
 *
 *  Created on: 13.01.2017
 *      Author: root
 */

#ifndef _SERIAL_H_
#define _SERIAL_H_

#define DEV_NYI		-1	// not yet implemented
#define DEV_OK		 0	// no error
#define DEV_BREAK	 1	// BREAK on line
#define DEV_ERROR	 2	// ERROR on line


#ifndef _SERIAL_C_
extern uint64_t serial_rx_lasttime_ms ;
extern uint64_t serial_tx_lasttime_ms ;
#endif


void devtxbreak (void);
void devtxstop (void);
void devtxstart (void);
void devtxinit (void);
void devtxflush (void);
void devtxput (uint8_t);
int32_t devtxwrite (uint8_t *, int32_t);
void devrxinit (void);
int32_t devrxavail (void);
int32_t devrxerror (void);
uint8_t devrxget (void);
void devinit (char *, int32_t, int32_t);
void devrestore (void);
void coninit (void);
void conrestore (void);
int32_t conget (void);

#endif /* SERIAL_H_ */
