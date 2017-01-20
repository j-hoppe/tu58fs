/* tu58device.h - server for TU58 protocol, ctrl over seriaql line, work on image
  *
  * Original (C) 1984 Dan Ts'o <Rockefeller Univ. Dept. of Neurobiology>
  * Update   (C) 2005-2016 Donald N North <ak6dn_at_mindspring_dot_com>
  * Update   (C) 2017 Joerg Hoppe <j_hoppe@t-online.de>, www.retrocmp.com
  *
  * All rights reserved.
  *
  * Redistribution and use in source and binary forms, with or without
  * modification, are permitted provided that the following conditions
  * are met:
  *
  * o Redistributions of source code must retain the above copyright
  *   notice, this list of conditions and the following disclaimer.
  *
  * o Redistributions in binary form must reproduce the above copyright
  *   notice, this list of conditions and the following disclaimer in the
  *   documentation and/or other materials provided with the distribution.
  *
  * o Neither the name of the copyright holder nor the names of its
  *   contributors may be used to endorse or promote products derived from
  *   this software without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  * HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
  * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
  * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
  * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  * This is the TU58 emulation program written at Rockefeller Univ., Dept. of
  * Neurobiology. We copyright (C) it and permit its use provided it is not
  * sold to others. Originally written by Dan Ts'o circa 1984 or so.
  *
  */

#ifndef _TU58DRIVE_H_
#define _TU58DRIVE_H_

#include "image.h"


#define DEV_NYI		-1	// not yet implemented
#define DEV_OK		 0	// no error
#define DEV_BREAK	 1	// BREAK on line
#define DEV_ERROR	 2	// ERROR on line

#define TU58_DEVICECOUNT 8 // # of TU58 drives
#define TU58_BLOCKSIZE	512	// number of bytes per block
#define TU58_CARTRIDGE_BLOCKCOUNT	512	// number of blocks per tape

#define IMAGE_UNIT_VALID(unit)	\
	( (unit) >= 0 || (unit) < TU58_DEVICECOUNT )

#ifndef _TU58DRIVE_C_
// data cartridges
extern image_t tu58_image[TU58_DEVICECOUNT];


// communication beetween thread and main()
extern uint8_t tu58_doinit ;	// set nonzero to indicate should send INITs continuously
extern uint8_t tu58_runonce;	// set nonzero to indicate emulator has been run

extern volatile int	tu58_offline_request;  // 1: main thread wants offline mode
extern volatile int	tu58_offline ; // TU58 is offline, all drives without cartridge

#endif

image_t *tu58image_get(int32_t unit) ;
void tu58images_init(void);
void tu58images_closeall(void);
void tu58images_sync_all();


void* tu58_server (void* none) ;
void* tu58_monitor (void* none) ;



#endif /* _TU58DRIVE_H_ */
