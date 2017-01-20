//
// tu58.h - definitions of TU58 protocol params
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



// TU58 Radial Serial Protocol

// Packet Flag / Single Byte Commands

#define TUF_NULL	0	// null
#define	TUF_DATA	1	// data packet
#define	TUF_CTRL	2	// control packet
#define	TUF_INIT	4	// initialize
#define TUF_BOOT	8	// boot
#define	TUF_CONT	16	// continue
#define	TUF_XON		17	// flow control start (XON)
#define	TUF_XOFF	19	// flow control stop (XOFF)

// Opcodes

#define	TUO_NOP		0	// no operation
#define	TUO_INIT	1	// initialize
#define	TUO_READ	2	// read block
#define	TUO_WRITE	3	// write block
#define	TUO_SEEK	5	// seek to block
#define TUO_DIAGNOSE	7	// run diagnostics
#define TUO_GETSTATUS	8	// get status
#define TUO_SETSTATUS	9	// set status
#define TUO_GETCHAR	10	// get characteristics
#define	TUO_END		64	// end packet

// Modifiers

#define TUM_RDRS       	1	// read with reduced sensitivity
#define TUM_WRRV       	1	// write with read verify
#define TUM_B128	128	// special addressing mode

// Switches

#define TUS_MRSP	8	// modified RSP sync mode
#define TUS_MAIN	16	// maintenance mode

// End packet success codes

#define TUE_SUCC	 0	// success
#define TUE_SUCR	 1	// success with retry
#define TUE_FAIL 	-1	// failed self test
#define TUE_PARO 	-2	// partial operation
#define TUE_BADU 	-8	// bad unit
#define TUE_BADF	-9	// no cartridge
#define TUE_WPRO	-11	// write protected
#define TUE_DERR	-17	// data check error
#define TUE_SKRR	-32	// seek error
#define TUE_MTRS	-33	// motor stopped
#define TUE_BADO	-48	// bad op code
#define TUE_BADB	-55	// bad block number
#define TUE_COMM	-127	// communications error

// lengths of packets

#define TU_CTRL_LEN	10	// size of cmd packet (opcode..block bytes)
#define TU_DATA_LEN	128	// size of data transfer segment
#define TU_CHAR_LEN	24	// size of getchar data packet
#define TU_BOOT_LEN	512	// size of a boot block



// Packet format, cmd/end vs data

#ifdef __PCH__
typedef struct {
    uint8	flag;			// packet type
    uint8	length;			// message length
    uint8	opcode;			// operation code
    uint8	modifier;		// modifier
    uint8	unit;			// drive number
    uint8	switches;		// switches
    uint16	sequence;		// sequence number, always zero
    uint16	count;			// byte count for read or write
    uint16	block;			// block number for read, write, or seek
    uint16	chksum;			// checksum, 16b end-around carry
} tu_cmdpkt;

typedef struct {
    uint8	flag;			// packet type
    uint8	length;			// message length
    uint8	data[TU_DATA_LEN];	// ptr to 1..DATALEN data bytes
    uint16	chksum;			// checksum, 16b end-around carry
} tu_datpkt;
#else
typedef struct {
    uint8_t	flag;			// packet type
    uint8_t	length;			// message length
    uint8_t	opcode;			// operation code
    uint8_t	modifier;		// modifier
    uint8_t	unit;			// drive number
    uint8_t	switches;		// switches
    uint16_t	sequence;		// sequence number, always zero
    uint16_t	count;			// byte count for read or write
    uint16_t	block;			// block number for read, write, or seek
    uint16_t	chksum;			// checksum, 16b end-around carry
} tu_cmdpkt;

typedef struct {
    uint8_t	flag;			// packet type
    uint8_t	length;			// message length
    uint8_t	data[TU_DATA_LEN];	// ptr to 1..DATALEN data bytes
    uint16_t	chksum;			// checksum, 16b end-around carry
} tu_datpkt;
#endif

typedef union {				// either:
    tu_cmdpkt	cmd;			// a control packet
    tu_datpkt	dat;			// a data packet
} tu_packet;






// the end
