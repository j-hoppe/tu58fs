/* main.h: tu58fs user interface, global resources
 *
 * TU58 emulator for serial port, with file sharing to host
 *
 * Copyright (c) 2017, Joerg Hoppe, j_hoppe@t-online.de, www.retrocmp.com
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
 */

#ifndef _MAIN_H_
#define _MAIN_H_

#define PROGNAME	"tu58fs"

#ifndef _MAIN_C_

extern char opt_serial_port[256] ; // default port number (COM1, /dev/ttyS0)
extern int opt_speed  ; // default line speed
extern int opt_stop ; // default stop bits, 1 or 2
extern int opt_verbose ; // set nonzero to output more info
extern int opt_timing ; // set nonzero to add timing delays
extern int opt_mrspen ; // set nonzero to enable MRSP mode
extern int opt_nosync ; // set nonzero to skip sending INIT at restart
extern int opt_debug ; // set nonzero for debug output
extern int opt_debug ; // set nonzero for debug output
extern int opt_debug ; // set nonzero for debug output
extern int opt_vax ; // set to remove delays for aggressive VAX console timeouts
extern int opt_background ; // set to run in background mode (no console I/O except errors)
extern int opt_synctimeout_sec ; // save changed image to disk after so many seconds of write-inactivity
extern int opt_offlinetimeout_sec ; // TU58 waits with "offline" until so many seconds of RS232-inactivity

#endif


#endif
