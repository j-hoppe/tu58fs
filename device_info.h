/* device_info.h - tables with data about DEC devices
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
 *  24-Jan-2017  JH  created
 *
 */
#ifndef _DEVICE_INFO_H_
#define _DEVICE_INFO_H_

typedef enum {
	devNONE = 0,
	devTU58,
	devRP0456,
	devRK035,
	devRL01,
	devRL02,
	devRK067,
	devRP023,
	devRM,
	devRS,
	devTU56,
	devRX01,
	devRX02,
	devRF = 13
} device_type_t;

typedef struct {
	int	device_type; // device_type_t tag to search for. see search_tagged_array()
	char *device_name;
	char *mnemonic;
	int block_count; // total number of usable blocks on device, without bad sector area
	int max_block_count; // if device has variable size
} device_info_t;


#ifndef _DEVICE_INFO_C_
extern device_info_t device_info_table[] ;
#endif


char *device_type_to_name(device_type_t device_type);
device_type_t device_type_from_name(char * name);
char *device_type_namelist(void) ;



#endif
