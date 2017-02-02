/* xxdp_radi.h - XXDP-random access device information
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
 *  20-Jan-2017  JH  created
 *
 */
#ifndef _XXDP_RADI_H_
#define _XXDP_RADI_H_

#include <stdint.h>

// XXDP Random Access Device Information
typedef struct {
	int	device_type; // device_type_t tag to search for. see search_tagged_array()
	uint16_t ufd_block_1; // 1st UFD block
	uint16_t ufd_blocks_num; // number of ufd blocks
	uint16_t bitmap_block_1; // 1st bit map block
	uint16_t bitmaps_num; // number of bitmaps
	int mfd1;
	int mfd2;
	int blocks_num; // # of blocks XXDP uses
	uint16_t prealloc_blocks_num; // number of blocks to preallocate
	int interleave;
	uint16_t boot_block;
	uint16_t monitor_block;
} xxdp_radi_t;

#ifndef _XXDP_RADI_C_
extern xxdp_radi_t xxdp_radi[];
#endif


#endif /* _XXDP_RADI_H_ */
