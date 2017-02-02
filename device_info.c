/* device_info.c - tables with data about DEC devices
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
#define _DEVICE_INFO_C_

#include <stdlib.h>
#include <string.h>
#include "utils.h"
#include "device_info.h"	// own

// uncomment supported devices

device_info_t device_info_table[] = {
    {
    	.device_type = devTU58, // tag
        .device_name = "TU58",
        .mnemonic = "DD",
		.block_count = 512
    },
/*
    {
        // oversized TU58 tape with 4MB capacity
    	.device_type = devTU58M4, // tag
        .device_name = "TU58M4",
        .mnemonic = "DD",
		.block_count = 8192
    },
    {
        // oversized TU58 tape with max capacity of 32MB
    	.device_type = devTU58M32, // tag
        .device_name = "TU58M32",
        .mnemonic = "DD",
		.block_count = 65535
    },
    {
    	.device_type = devRP0456 ,
        .device_name = "RP04,5,6",
        .mnemonic = "DB",
        .block_count = 48000 // XXDP
    },
    {
    	.device_type = devRK035 ,
        .device_name = "RK03,5",
        .mnemonic = "DK",
        .block_count = 4800 // XXDP
    },
*/
    {
    	.device_type = devRL01 ,
        .device_name = "RL01",
        .mnemonic = "DL",
        .block_count = 10225 // pyRT RT11
    },
    {
    	.device_type = devRL02 ,
        .device_name = "RL02",
        .mnemonic = "DL",
        .block_count = 20465 // pyRT11 RT11
    },
/*
    {
    	.device_type = devRK067 ,
        .device_name = "RK06,7",
        .mnemonic = "DM",
        .block_count = 27104 // XXDP
    },
    {
    	.device_type = devRP023 ,
        .device_name = "RP02,3",
        .mnemonic = "DP",
        .block_count = -1 // unknown, bad fiche
    },
    {
    	.device_type = devRM ,
        .device_name = "RM03",
        .mnemonic = "DR",
        .block_count = 48000 // XXDP
    },
    {
    	.device_type = devRS ,
        .device_name = "RS03,4",
        .mnemonic = "DS",
        .block_count = 989 // XXDP
    },
    {
    	.device_type = devTU56 ,
        .device_name = "TU56,DECTAPE",
        .mnemonic = "DT",
        .block_count = 576 // XXDP
    },
    {
    	.device_type = devRX01 ,
        .device_name = "RX01",
        .mnemonic = "DX",
        .block_count = 494 // XXDP
    },
    {
    	.device_type = devRX02 ,
        .device_name = "RX02",
        .mnemonic = "DY",
        .block_count = 988 // XXDP, pyRT11
    },
    {
    	.device_type = devRF ,
        .device_name = "RF11",
        .mnemonic = "RF",
        .block_count = -1 // unknown
    },
*/
    {
    	.device_type = 0 // end marker
    }
} ;





char *device_type_to_name(device_type_t device_type) {
	device_info_t *di  = (device_info_t*) search_tagged_array(device_info_table, sizeof(device_info_t),
			device_type);
	if (! di)
		return NULL ;
	else
		return di->device_name ;
}

device_type_t device_type_from_name(char * name) {
	// search for name
	device_info_t *di ;
	for (di = device_info_table ; di->device_type ; di++)
		if (!strcasecmp(name, di->device_name))
			return di->device_type;
	return devNONE;
}

char *device_type_namelist() {
	static char buffer[1024];
	device_info_t *di ;
	buffer[0] = 0;
	for (di = device_info_table ; di->device_type ; di++) {
		if (strlen(buffer))
			strcat(buffer, ",");
		strcat(buffer, di->device_name);
	}
	return buffer;
}


