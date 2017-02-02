/* error.h: global error handling
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
 *  29-Jan-2017  JH  created
 */

#ifndef _ERROR_H_
#define _ERROR_H_

#include <stdio.h>

// possible errors
#define ERROR_OK	0
#define ERROR_FILESYSTEM_FORMAT	-1 // error in filesystem structure
#define ERROR_FILESYSTEM_OVERFLOW -2 // too many files on PDP
#define ERROR_FILESYSTEM_DUPLICATE -3 // duplicate file name
#define ERROR_FILESYSTEM_INVALID -4 // invalid PDP filesystem selected
#define	ERROR_HOSTDIR -5 // error with shared directory
#define ERROR_HOSTFILE -6 // file error on hostfile system
#define ERROR_ILLPARAMVAL -7 // illegaler function parameter
#define ERROR_IMAGE_MODE	-8 // tape image in wrong operation mode
#define ERROR_IMAGE_EOF -9 // file pointer moved outside tape image




//#define	ERROR_MAX_TRACE_LEVEL	10

#ifndef _ERROR_C_
extern FILE *ferr; // variable error stream
extern int error_code ;
//extern char  error_message[ERROR_MAX_TRACE_LEVEL+1][1024] ;
#endif

void error_clear(void) ;
int error_set(int code, char *fmt, ...) ;

#endif
