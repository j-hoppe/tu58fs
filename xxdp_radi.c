/* xxdp_radi.c - XXDP-random access device information
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
#define _XXDP_RADI_C_

#include "utils.h"
#include "xxdp.h"

// index = dec_device_t
xxdp_radi_t xxdp_radi[] = {
    {
        .device = "TU58",
        .mnemonic = "DD",
        .ufd_block_1 = 3,
        .ufd_blocks_num = 4,
        .bitmap_block_1 = 7,
        .bitmaps_num = 1,
        .mfd1 = 1,
        .mfd2 = 2,
		/* DEC defines the XXDP tape to have 511 blocks.
		 * But after decades of XXDPDIR and friends,
		 * 512 seems to be the standard.
		 */
        .device_blocks_num = 512, // 511,
        .prealloc_blocks_num = 40,
        .interleave = 1,
        .boot_block = 0,
        .monitor_block = 8
    },
    {
        .device = "RP04,5,6",
        .mnemonic = "DB",
        .ufd_block_1 = 3,
        .ufd_blocks_num = 170,
        .bitmap_block_1 = 173,
        .bitmaps_num = 50,
        .mfd1 = 1,
        .mfd2 = 2,
        .device_blocks_num = 48000,
        .prealloc_blocks_num = 255,
        .interleave = 1,
        .boot_block = 0,
        .monitor_block = 223
    },
    {
        .device = "RK03,5",
        .mnemonic = "DK",
        .ufd_block_1 = 3,
        .ufd_blocks_num = 16,
        .bitmap_block_1 = 4795, // ??
        .bitmaps_num = 5,
        .mfd1 = 1,
        .mfd2 = 4794,
        .device_blocks_num = 4800,
        .prealloc_blocks_num = 69,
        .interleave = 5,
        .boot_block = 0,
        .monitor_block = 30
    },
    {
        .device = "RL01",
        .mnemonic = "DL",
        .ufd_block_1 = 24,
        .ufd_blocks_num = 146, // 24..169 fiche bad, don north
        .bitmap_block_1 = 2,
        .bitmaps_num = 22,
        .mfd1 = 1,
        .mfd2 = -1,
        .device_blocks_num = 10200,
        .prealloc_blocks_num = 200,
        .interleave = 1,
        .boot_block = 0,
        .monitor_block = 170
    },
    {
        .device = "RL02",
        .mnemonic = "DL",
        .ufd_block_1 = 24,
        .ufd_blocks_num = 146, // 24..169 fiche bad, don north
        .bitmap_block_1 = 2,
        .bitmaps_num = 22,
        .mfd1 = 1,
        .mfd2 = -1,
        .device_blocks_num = 20460,
        .prealloc_blocks_num = 200,
        .interleave = 1,
        .boot_block = 0,
        .monitor_block = 170
    },
    {
        .device = "RK06,7",
        .mnemonic = "DM",
        .ufd_block_1 = 31,
        .ufd_blocks_num = 96,
        .bitmap_block_1 = 2,
        .bitmaps_num = 29,
        .mfd1 = 1,
        .mfd2 = -1,
        .device_blocks_num = 27104,
        .prealloc_blocks_num = 157,
        .interleave = 1,
        .boot_block = 0,
        .monitor_block = 127
    },
    {
        .device = "RP02,3",
        .mnemonic = "DP",
        .ufd_block_1 = 3,
        .ufd_blocks_num = 170,
        .bitmap_block_1 = 173,
        .bitmaps_num = 2,
        .mfd1 = 1,
        .mfd2 = 2,
        .device_blocks_num = -1, // unknown, bad fiche
        .prealloc_blocks_num = 255,
        .interleave = 1,
        .boot_block = 0,
        .monitor_block = 223
    },
    {
        .device = "RM03",
        .mnemonic = "DR",
        .ufd_block_1 = 52,
        .ufd_blocks_num = 170,
        .bitmap_block_1 = 2,
        .bitmaps_num = 50,
        .mfd1 = 1,
        .mfd2 = -1,
        .device_blocks_num = 48000,
        .prealloc_blocks_num = 255,
        .interleave = 1,
        .boot_block = 0,
        .monitor_block = 222
    },
    {
        .device = "RS03,4",
        .mnemonic = "DS",
        .ufd_block_1 = 3,
        .ufd_blocks_num = 4,
        .bitmap_block_1 = 7,
        .bitmaps_num = 2,
        .mfd1 = 1,
        .mfd2 = 2,
        .device_blocks_num = 989 ,
        .prealloc_blocks_num = 41,
        .interleave = 1,
        .boot_block = 0,
        .monitor_block = 9
    },
    {
        .device = "TU56",
        .mnemonic = "DT",
        .ufd_block_1 = 102,
        .ufd_blocks_num = 2,
        .bitmap_block_1 = 104,
        .bitmaps_num = 1,
        .mfd1 = 100,
        .mfd2 = 101,
        .device_blocks_num = 576,
        .prealloc_blocks_num = 69,
        .interleave = 5,
        .boot_block = 0,
        .monitor_block = 30 // bad fiche, don north
    },
    {
        .device = "RX01",
        .mnemonic = "DX",
        .ufd_block_1 = 3,
        .ufd_blocks_num = 4,
        .bitmap_block_1 = 7,
        .bitmaps_num = 1,
        .mfd1 = 1,
        .mfd2 = 2,
        .device_blocks_num = 494,
        .prealloc_blocks_num = 40,
        .interleave = 1,
        .boot_block = 0,
        .monitor_block = 8
    },
    {
        .device = "RX02",
        .mnemonic = "DY",
        .ufd_block_1 = 3,
        .ufd_blocks_num = 16,
        .bitmap_block_1 = 19,
        .bitmaps_num = 4,
        .mfd1 = 1,
        .mfd2 = 2,
        .device_blocks_num = -1, // 988 ?  unknown, bad fiche
        .prealloc_blocks_num = 55,
        .interleave = 1,
        .boot_block = 0,
        .monitor_block = 23
    }

} ;




