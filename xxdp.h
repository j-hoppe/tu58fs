/* xxdp.h - handles the XXDP filesystem on an image.
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
 *  12-Jan-2017  JH  created
 */
#ifndef XXDP_H_
#define XXDP_H_

#include <stdio.h>
#include <stdint.h>
#include <time.h>

#include "boolarray.h"
#include "utils.h"

/* Logical structure of XXDP filesystem.
*  See
 * CHQFSA0 XXDP+ FILE STRUCT DOC
 *
 */
#define XXDP_BLOCKSIZE   512
#define XXDP_MAX_BLOCKCOUNT 0x10000 // block addr only 16 bit

// layout data not in RADI
#define	XXDP_BITMAP_WORDS_PER_MAP 60 // 1 map = 16*60 = 960 block bits
#define	XXDP_UFD_ENTRY_WORDCOUNT	9 // len of UFD entry
#define XXDP_UFD_ENTRIES_PER_BLOCK	28 // 29 file entries per UFD block
// own limits
#define	XXDP_MAX_FILES_PER_IMAGE 1000
#define XXDP_MAX_BLOCKS_PER_LIST       1024  //  own: max filesize: * 510

// boot block and monitor blocks are pseudo files
#define XXDP_BOOTBLOCK_FILENAME	"$BOOT.BLK" // valid XXDP file names
#define XXDP_MONITOR_FILENAME	"$MONI.TOR"


typedef uint16_t blocknr_t ;


typedef struct {
	int	 track ;
	int	 sector ;
	int	 cylinder ;
	int	blocknr ;
} diskaddr_t ;

// Random Access Device Information
typedef struct {
	char *device;
	char *mnemonic;
	blocknr_t ufd_block_1; // 1st UFD block
	blocknr_t ufd_blocks_num; // number of ufd blocks
	blocknr_t bitmap_block_1; // 1st bit map block
	blocknr_t bitmaps_num; // number of bitmaps
	int mfd1;
	int mfd2;
	int device_blocks_num; // number of blocks on device
	blocknr_t prealloc_blocks_num; // number of blocks to preallocate
	int interleave;
	blocknr_t boot_block;
	blocknr_t monitor_block;
} xxdp_radi_t;

typedef struct {
	unsigned count;
	blocknr_t blocknr[XXDP_MAX_BLOCKS_PER_LIST]; // later dynamic?
} xxdp_blocklist_t;

// a range of block which is treaed as one byte stream
// - for the bootloader on a xxdp image
// - for the monitor
typedef struct {
	blocknr_t	blocknr ; // start
	blocknr_t	blockcount ; // count of blocks
	uint8_t *data;  // space for blockcount * BLOCKSIZE data
	uint32_t data_size; // byte count in data[]
	char *filename ; // pseudo filename
} xxdp_multiblock_t;

// boolean marker for block usage
typedef struct {
	xxdp_blocklist_t blocklist ;
	uint8_t used[XXDP_MAX_BLOCKCOUNT];
} xxdp_bitmap_t;

typedef struct {
// these blocks are allocated, but no necessarily all used
	xxdp_blocklist_t blocklist;
	int contiguous; //  1: blocknumbers are numbered start, start+1, start+2, ...
	char filnam[80];  // normally 6 chars, encoded in 2 words RADIX50. Special filenames longer
	char ext[40]; // normally 3 chars, encoded 1 word
	blocknr_t block_count ; // saved blockcount from UFD.
	// UFD should not differ from blocklist.count !
	uint32_t data_size; // byte count in data[]
	uint8_t *data; // dynamic array with 'size' entires
	struct tm date; // file date. only y,m,d valid
	uint8_t	changed ; // calc'd from image_changed_blocks
} xxdp_file_t;

// all elements of a populated xxdp disk image
typedef struct {
	int expandable ; // boolean: blockcount may be increased in _add_file()

	 // the device this filesystem resides on
	 dec_device_t dec_device ;
	 xxdp_radi_t *radi ;	// Random Access Device Information

	 int blockcount ; // usable blocks in filesystem.
	 blocknr_t	preallocated_blockcount ; // fix blocks at start
	 int interleave;
 	 int mfd_variety; // Master File Directory in format 1 or format 2?

 	diskaddr_t bad_sector_file_sd ; // DEC std 144 bad sector file single density
 	diskaddr_t bad_sector_file_dd ; // dto, double density

	// link to image data and size
	// pointer reference outside locations,
	// which may be change if image-resize is necessary
	 uint8_t **image_data_ptr; // ptr to uint8_t data[]
	 uint32_t *image_size_ptr ; // ptr to size of data[]
	 boolarray_t *image_changed_blocks ; // blocks marked as "changed". may be NULL

	xxdp_multiblock_t *bootblock;
	xxdp_multiblock_t *monitor;
	xxdp_bitmap_t *bitmap;

	// Data in  Master File Directory
	// format 1: linked list of 2.  Variant 2: only 1 block
	xxdp_blocklist_t *mfd_blocklist;

	// blocks used by User File Directory
	xxdp_blocklist_t *ufd_blocklist;
	int file_count;
	xxdp_file_t *file[XXDP_MAX_FILES_PER_IMAGE];
} xxdp_filesystem_t;

#if !defined(__XXDP_C_) && !defined(_XXDP_RADI_C_)
extern char * xxdp_fileorder[] ;
extern xxdp_radi_t xxdp_radi[];
#endif


//int xxdp_init(image_t *img);


// before first use. Link with image data buffer
xxdp_filesystem_t *xxdp_filesystem_create(dec_device_t dec_device, uint8_t **image_data_ptr, uint32_t *image_data_size_ptr,
		boolarray_t *changedblocks,	int expandable) ;

void xxdp_filesystem_destroy(xxdp_filesystem_t *_this) ;

void xxdp_filesystem_init(xxdp_filesystem_t *_this) ;

// analyse an image
int xxdp_filesystem_parse(xxdp_filesystem_t *_this);

int xxdp_filesystem_add_file(xxdp_filesystem_t *_this, int special, char *hostfname, time_t hostfdate,
		uint8_t *data, uint32_t data_size) ;


int xxdp_filesystem_layout(xxdp_filesystem_t *_this) ;

// write filesystem into image
int xxdp_filesystem_render(xxdp_filesystem_t *_this);


void xxdp_filesystem_print_dir(xxdp_filesystem_t *_this, FILE *stream) ;
void xxdp_filesystem_print_blocks(xxdp_filesystem_t *_this, FILE *stream) ;

char *xxdp_filename_to_host(char *filnam, char *ext) ;
char *xxdp_filename_from_host(char *hostfname, char *filnam, char *ext) ;


#endif /* XXDP_H_ */
