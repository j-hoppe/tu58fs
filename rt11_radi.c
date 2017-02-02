/* rt11_radi.c - RT11-random access device information
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
 *  28-Jan-2017  JH  created
 *
 */
#define _RT11_RADI_C_

#include "device_info.h"
#include "rt11_radi.h"

/*
 * AA-5279B-TC_RT-11_V4.0_System_Users_Guide_Mar80.pdf page 4-110
 * See AA-5279B-TC RT-11 V4.0 User Guide, "INITIALIZE", pp. 4-108..110
 * RK06/7 = 32 bad blocks, RL01/RL02 = 10
 */
rt11_radi_t rt11_radi[] = {
    {
   		.device_type = devRK035, // device_type_t tag to search for. see search_tagged_array()
  		.block_count = -1, // unknown
   		.first_dir_blocknr = 6,
   		.replacable_bad_blocks = 0,
   		.dir_seg_count = 16
    },
    {
   		.device_type = devTU58, // device_type_t tag to search for. see search_tagged_array()
   		.block_count = 512,
   		.first_dir_blocknr = 6,
   		.replacable_bad_blocks = 0,
   		.dir_seg_count = 1
    },
	{
    	.device_type = devTU56, // tag DECtape
        .block_count = -1, // unknown
		.first_dir_blocknr = 6,
		.replacable_bad_blocks = 0,
		.dir_seg_count = 1
    },
	{
    	.device_type = devRF, // tag
        .block_count = -1,
		.first_dir_blocknr = 6,
		.replacable_bad_blocks = 0,
		.dir_seg_count = 4
    },
	{
    	.device_type = devRS, // tag
        .block_count = -1, // unknown
		.first_dir_blocknr = 6,
		.replacable_bad_blocks = 0,
		.dir_seg_count = 4
    },
	{
    	.device_type = devRP023, // tag
        .block_count = -1, // unknown
		.first_dir_blocknr = 6,
		.replacable_bad_blocks = 0,
		.dir_seg_count = 31
    },
	{
    	.device_type = devRX01, // tag
        .block_count = 494, // as XXDP
		.first_dir_blocknr = 6,
		.replacable_bad_blocks = 0,
		.dir_seg_count = 1
    },
	{
    	.device_type = devRX02, // tag
        .block_count = 988,
		.first_dir_blocknr = 6,
		.replacable_bad_blocks = 0,
		.dir_seg_count = 4
    },
	{
    	.device_type = devRK067, // tag
        .block_count = -1, // unknown
		.first_dir_blocknr = 6,
		.replacable_bad_blocks = 32,
		.dir_seg_count = 31
    },
	{
    	.device_type = devRL01, // tag
        .block_count = 10225,// pyRT11
		.first_dir_blocknr = 6,
		.replacable_bad_blocks = 0,
		.dir_seg_count = 16
    },
	{
    	.device_type = devRL02, // tag
        .block_count = 20465, // pyRT11
		.first_dir_blocknr = 6,
		.replacable_bad_blocks = 0,
		.dir_seg_count = 16
    },
    {
    	.device_type = 0 // end marker
    }
} ;




