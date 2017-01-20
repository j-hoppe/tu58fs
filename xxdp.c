/* xxdp.c - handles the XXDP filesystem on an image.
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
 *
 * The logical structure of the DOS-11 file system
 * is represented by linked data structures.
 * The logical filesystem is indepented of the physical image,
 * file content and blocklist are hold in own buffers.
 *
 * API
 * _init():
 * Clear all data from the filesystem and preload
 * layout parameters (length of preallocated areas, interleave etc.)
 * from device-specific "Random Access Device Information" table.
 *
 * _parse():
 *  build the logical filesystem from a physical binary image.
 *  After that, user files and boot files can be read.
 *
 *  _add_boot(), _add_file():
 *  add boot files and user files to the logical image,
 *  (allocate blocks and update blocklists)
 *
 *  _render()
 *  Produce the binary image from logical filesystem.
 *
 *  Created on: 12.01.2017
 *      Author: root
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <time.h>
#include <utime.h>
#include <fcntl.h>

#include "utils.h"
#include "boolarray.h"
#include "image.h"	// only for old xxdp_init()
#include "xxdp.h"	// own

// pseudo filenames
#define XXDP_BOOTBLOCK_FILENAME	"$BOOT.BLK" // valid XXDP file names
#define XXDP_MONITOR_FILENAME	"$MONI.TOR"

// sort order for files. For regexes the . must be escaped by \.
// and a * is .*"
char * xxdp_fileorder[] = { "XXDPSM\\.SYS", "XXDPXM\\.SYS", "DRSSM\\.SYS", "DRSXM\\.SYS", // 1st on disk
		".*\\.SYS", // the drivers
		"START\\..*", // startup script
		"HELP\\..*", // help texts
		".*\\.CCC",  // other chain files
		".*\\.BIC",  // *.bin and *.bic
		".*\\.BIN",  // *.bin and *.bic
		// then all other stuff
		NULL };

/*************************************************************
 * low level operators
 *************************************************************/

// derefenced for daily use
#define IMAGE_SIZE(_this) (*((_this)->image_size_ptr))
#define IMAGE_DATA(_this) (*((_this)->image_data_ptr))
// ptr to first byte of block
#define IMAGE_BLOCK(_this, blocknr) (IMAGE_DATA(_this) + (XXDP_BLOCKSIZE *(blocknr)))

// fetch/write a 16bit word in the image
// LSB first
static uint16_t xxdp_image_get_word(xxdp_filesystem_t *_this, blocknr_t blocknr, uint8_t wordnr) {
	uint32_t idx = XXDP_BLOCKSIZE * blocknr + (wordnr * 2);
	assert(idx >= 0 && (idx+1) < IMAGE_SIZE(_this));

	return IMAGE_DATA(_this)[idx] | (IMAGE_DATA(_this)[idx + 1] << 8);
}

static void xxdp_image_set_word(xxdp_filesystem_t *_this, blocknr_t blocknr, uint8_t wordnr,
		uint16_t val) {
	uint32_t idx = XXDP_BLOCKSIZE * blocknr + (wordnr * 2);
	assert(idx >= 0 && (idx+1) < IMAGE_SIZE(_this));

	IMAGE_DATA(_this)[idx] = val & 0xff;
	IMAGE_DATA(_this)[idx + 1] = (val >> 8) & 0xff;
	//fprintf(stderr, "set 0x%x to 0x%x\n", idx, val) ;
}

// scan linked list at block # 'start'
// bytes 0,1 = 1st word in block are # of next. Last blck has link "0".
static int xxdp_blocklist_get(xxdp_filesystem_t *_this, xxdp_blocklist_t *bl, blocknr_t start) {
	blocknr_t blocknr = start;
	unsigned n = 0; // count blocks
	do {
		bl->blocknr[n++] = blocknr;
		// follwo link to next block
		blocknr = xxdp_image_get_word(_this, blocknr, 0);
	} while (n < XXDP_MAX_BLOCKS_PER_LIST && blocknr > 0);
	bl->count = n;
	if (blocknr) {
		fprintf(ferr, "xxdp_blocklist_get(): block list too long or recursion");
		return -1;
	}
	return 0;
}

static void xxdp_blocklist_set(xxdp_filesystem_t *_this, xxdp_blocklist_t *bl) {
	unsigned i;
	for (i = 0; i < (bl->count - 1); i++)
		// write link field of each block with next
		xxdp_image_set_word(_this, bl->blocknr[i], 0, bl->blocknr[i + 1]);
	// clear link field of last block
	xxdp_image_set_word(_this, bl->blocknr[bl->count - 1], 0, 0);
}

// get count of used blocks
static int xxdp_bitmap_count(xxdp_filesystem_t *_this) {
	int result = 0;
	int i, j, k;
	for (i = 0; i < _this->bitmap->blocklist.count; i++) {
		blocknr_t map_blknr = _this->bitmap->blocklist.blocknr[i];
		int map_wordcount;
		map_wordcount = xxdp_image_get_word(_this, map_blknr, 2);
		for (j = 0; j < map_wordcount; j++) {
			uint16_t map_flags = xxdp_image_get_word(_this, map_blknr, j + 4);
			// 16 flags per word. count "1" bits
			if (map_flags == 0xffff)
				result += 16; // most of time
			else
				for (k = 0; k < 16; k++)
					if (map_flags & (1 << k))
						result++;
		}
	}
	return result;
}

// set file->changed from the changed block map
static void xxdp_filesystem_mark_files_as_changed(xxdp_filesystem_t *_this) {
	int i, j;
	for (i = 0; i < _this->file_count; i++) {
		xxdp_file_t *f = _this->file[i];
		f->changed = 0;
		if (_this->image_changed_blocks)
			for (j = 0; j < f->blocklist.count; j++) {
				int blknr = f->blocklist.blocknr[j];
				f->changed |= boolarray_bit_get(_this->image_changed_blocks, blknr);
			}
	}
}

/*************************************************************************
 * constructor / destructor
 *************************************************************************/

// before first use. Link with image data buffer
xxdp_filesystem_t *xxdp_filesystem_create(dec_device_t dec_device, uint8_t **image_data_ptr,
		uint32_t *image_size_ptr, boolarray_t *changedblocks, int expandable) {
	xxdp_filesystem_t *_this;
	int i;
	_this = malloc(sizeof(xxdp_filesystem_t));
	_this->dec_device = dec_device;
	// Random Access Device Information
	_this->radi = &(xxdp_radi[_this->dec_device]);

	// save pointer to variables defining image buffer data
	_this->image_data_ptr = image_data_ptr;
	_this->image_size_ptr = image_size_ptr;
	_this->image_changed_blocks = changedblocks;

	_this->expandable = expandable;

	// these are always there, static allocated
	_this->bootblock = malloc(sizeof(xxdp_multiblock_t));
	_this->bootblock->data = NULL;
	_this->bootblock->data_size = 0;
	_this->monitor = malloc(sizeof(xxdp_multiblock_t));
	_this->monitor->data = NULL;
	_this->monitor->data_size = 0;
	_this->bitmap = malloc(sizeof(xxdp_bitmap_t));
	_this->mfd_blocklist = malloc(sizeof(xxdp_blocklist_t));
	_this->ufd_blocklist = malloc(sizeof(xxdp_blocklist_t));

	// files vary: dynamic allocate
	_this->file_count = 0;
	for (i = 0; i < XXDP_MAX_FILES_PER_IMAGE; i++)
		_this->file[i] = NULL;
	xxdp_filesystem_init(_this);
	return _this;
}

void xxdp_filesystem_destroy(xxdp_filesystem_t *_this) {
	xxdp_filesystem_init(_this);
	free(_this->bootblock);
	free(_this->monitor);
	free(_this->bitmap);
	free(_this->mfd_blocklist);
	free(_this->ufd_blocklist);
	free(_this);
}

// free / clear all structures, set default values
// save "expandable" flag for _add_file() operation
void xxdp_filesystem_init(xxdp_filesystem_t *_this) {
	int i;

	// empty block bitmap
	_this->bitmap->blocklist.count = 0; // position not known
	for (i = 0; i < XXDP_MAX_BLOCKCOUNT; i++)
		_this->bitmap->used[i] = 0;

	// set device params
	_this->blockcount = _this->radi->device_blocks_num;
	if (_this->blockcount > XXDP_MAX_BLOCKCOUNT)
		// trunc large devices, only 64K blocks
		_this->blockcount = XXDP_MAX_BLOCKCOUNT;
	_this->preallocated_blockcount = _this->radi->prealloc_blocks_num;
	_this->bootblock->blocknr = _this->radi->boot_block;
	_this->bootblock->blockcount = 0; // may not exist
	_this->bootblock->filename = XXDP_BOOTBLOCK_FILENAME;
	_this->monitor->blocknr = _this->radi->monitor_block;
	_this->monitor->blockcount = 0; // may not exist

	_this->monitor->filename = XXDP_MONITOR_FILENAME;

	_this->mfd_blocklist->count = 0;
	_this->interleave = _this->radi->interleave;
	// which sort of MFD?
	if (_this->radi->mfd2 >= 0) {
		_this->mfd_variety = 1; // 2 blocks
		_this->mfd_blocklist->count = 2;
		_this->mfd_blocklist->blocknr[0] = _this->radi->mfd1;
		_this->mfd_blocklist->blocknr[1] = _this->radi->mfd2;
	} else {
		_this->mfd_variety = 2; // single block format
		_this->mfd_blocklist->count = 1;
		_this->mfd_blocklist->blocknr[0] = _this->radi->mfd1;
	}
	_this->ufd_blocklist->count = 0;
	if (_this->bootblock->data)
		free(_this->bootblock->data);
	_this->bootblock->data = NULL;
	_this->bootblock->data_size = 0;
	if (_this->monitor->data)
		free(_this->monitor->data);
	_this->monitor->data = NULL;
	_this->monitor->data_size = 0;

	for (i = 0; i < XXDP_MAX_FILES_PER_IMAGE; i++)
		if (_this->file[i]) {
			if (_this->file[i]->data)
				free(_this->file[i]->data);
			free(_this->file[i]);
			_this->file[i] = NULL;
		}
	_this->file_count = 0;
}

/**************************************************************
 * _parse()
 * convert byte array of image into logical objects
 **************************************************************/

// read MFD, produce MFD, Bitmap and UFD
static void parse_mfd(xxdp_filesystem_t *_this) {
	int n;
	blocknr_t blknr, mfdblknr;

	xxdp_blocklist_get(_this, _this->mfd_blocklist, _this->radi->mfd1);
	if (_this->mfd_blocklist->count == 2) {
		// 2 blocks. first predefined, fetch 2nd from image
		// Prefer MFD data over linked list scan, why?
		if (_this->mfd_variety != 1)
			fprintf(ferr, "MFD is 2 blocks, variety 1 expected, but variety %d defined",
					_this->mfd_variety);
		/*
		 _this->mfd_blocklist->count = 2;
		 _this->mfd_blocklist->blocknr[0] = _this->radi->mfd1;
		 _this->mfd_blocklist->blocknr[1] = xxdp_image_get_word(_this,
		 _this->mfd_blocklist->blocknr[0], 0);
		 */
		mfdblknr = _this->mfd_blocklist->blocknr[0];

		_this->interleave = xxdp_image_get_word(_this, mfdblknr, 1);
		// build bitmap,
		// word[2] = bitmap start block, word[3] = pointer to bitmap #1, always the same?
		// a 0 terminated list of bitmap blocks is in MFD1, word 2,3,...
		// Prefer MFD data over linked list scan, why?
		n = 0;
		do {
			assert(n + 3 < 255);
			blknr = xxdp_image_get_word(_this, mfdblknr, n + 3);
			if (blknr > 0) {
				_this->bitmap->blocklist.blocknr[n] = blknr;
				n++;
			}
		} while (blknr > 0);
		_this->bitmap->blocklist.count = n;

		mfdblknr = _this->mfd_blocklist->blocknr[1];
		// Get start of User File Directory from MFD2, word 2, then scan linked list
		blknr = xxdp_image_get_word(_this, mfdblknr, 2);
		xxdp_blocklist_get(_this, _this->ufd_blocklist, blknr);
	} else if (_this->mfd_blocklist->count == 1) {
		// var 2: "MFD1/2" RL01/2 ?
		if (_this->mfd_variety != 2)
			fprintf(ferr, "MFD is 1 blocks, variety 2 expected, but variety %d defined",
					_this->mfd_variety);

		mfdblknr = _this->mfd_blocklist->blocknr[0];

		// Build UFD block list
		blknr = xxdp_image_get_word(_this, mfdblknr, 1);
		xxdp_blocklist_get(_this, _this->ufd_blocklist, blknr);
		// verify len
		n = xxdp_image_get_word(_this, mfdblknr, 2);
		if (n != _this->ufd_blocklist->count)
			fprintf(ferr, "UFD block count is %u, but %d in MFD1/2",
					_this->ufd_blocklist->count, n);
		// best choice is len of disk list

		// Build Bitmap block list
		blknr = xxdp_image_get_word(_this, mfdblknr, 3);
		xxdp_blocklist_get(_this, &(_this->bitmap->blocklist), blknr);
		// verify len
		n = xxdp_image_get_word(_this, mfdblknr, 4);
		if (n != _this->bitmap->blocklist.count)
			fprintf(ferr, "Bitmap block count is %u, but %d in MFD1/2\n",
					_this->bitmap->blocklist.count, n);

		// total num of blocks
		n = _this->blockcount = xxdp_image_get_word(_this, mfdblknr, 7);
		if (n != _this->radi->device_blocks_num)
			fprintf(ferr, "Device blockcount is %u in RADI, but %d in MFD1/2\n",
					_this->radi->device_blocks_num, n);

		n = _this->preallocated_blockcount = xxdp_image_get_word(_this, mfdblknr, 8);
		if (n != _this->radi->prealloc_blocks_num)
			fprintf(ferr, "Device preallocated blocks are %u in RADI, but %d in MFD1/2\n",
					_this->radi->prealloc_blocks_num, n);

		n = _this->interleave = xxdp_image_get_word(_this, mfdblknr, 9);
		if (n != _this->radi->interleave)
			fprintf(ferr, "Device interleave is %u in RADI, but %d in MFD1/2\n",
					_this->radi->interleave, n);

		n = _this->monitor->blocknr = xxdp_image_get_word(_this, mfdblknr, 11);
		if (n != _this->radi->monitor_block)
			fprintf(ferr, "Monitor core start is %u in RADI, but %d in MFD1/2\n",
					_this->radi->monitor_block, n);
		_this->monitor->blockcount = _this->preallocated_blockcount - _this->monitor->blocknr;

		fprintf(ferr, "Warning: position of bad block file not yet evaluated\n");
	} else {
		fprintf(ferr, "Invalid block count in  MFD: %d\n", _this->mfd_blocklist->count);
		exit(1);
	}
}

// bitmap blocks known, produce "used[]" flag array
static void parse_bitmap(xxdp_filesystem_t *_this) {
	int i, j, k;
	blocknr_t blknr; // enumerates the block flags
	// assume consecutive bitmap blocks encode consecutive block numbers
	// what about map number?
	blknr = 0;
	for (i = 0; i < _this->bitmap->blocklist.count; i++) {
		int map_wordcount;
		blocknr_t map_blknr = _this->bitmap->blocklist.blocknr[i];
		blocknr_t map_start_blknr; // block of 1st map block
		// mapnr = xxdp_image_get_word(_this, blknr, 2);
		map_wordcount = xxdp_image_get_word(_this, map_blknr, 2);
		assert(map_wordcount == XXDP_BITMAP_WORDS_PER_MAP);
		// verify link to start
		map_start_blknr = xxdp_image_get_word(_this, map_blknr, 3);
		assert(map_start_blknr == _this->bitmap->blocklist.blocknr[0]);
		// hexdump(ferr, IMAGE_BLOCK(_this, map_blknr), 512, "Block %u = bitmap %u", map_blknr, i);
		for (j = 0; j < map_wordcount; j++) {
			uint16_t map_flags = xxdp_image_get_word(_this, map_blknr, j + 4);
			// 16 flags per word. LSB = lowest blocknr
			for (k = 0; k < 16; k++) {
				assert(blknr == (i * XXDP_BITMAP_WORDS_PER_MAP + j) * 16 + k);
				if (map_flags & (1 << k))
					_this->bitmap->used[blknr] = 1;
				else
					_this->bitmap->used[blknr] = 0;
				blknr++;
			}
		}
	}
	// blknr is now count of defined blocks
	// _this->bitmap->blockcount = blknr;
}

// read block[start] ... block[start+blockcount-1] into data[]
static void parse_multiblock(xxdp_filesystem_t *_this, xxdp_multiblock_t *multiblock,
		blocknr_t start, blocknr_t blockcount) {
	multiblock->blockcount = blockcount;
	multiblock->blocknr = start;
	multiblock->data_size = XXDP_BLOCKSIZE * blockcount;
	multiblock->data = malloc(multiblock->data_size);
	memcpy(multiblock->data, IMAGE_BLOCK(_this, start), multiblock->data_size);
}

// UFD blocks known, produce filelist
static void parse_ufd(xxdp_filesystem_t *_this) {
	int i, j;
	blocknr_t blknr; // enumerates the directory blocks
	uint32_t file_entry_start_wordnr;
	uint16_t w;
	for (i = 0; i < _this->ufd_blocklist->count; i++) {
		blknr = _this->ufd_blocklist->blocknr[i];
		// hexdump(ferr, IMAGE_BLOCK(_this, blknr), 512, "Block %u = UFD block %u", blknr, i);
		// 28 dir entries per block
		for (j = 0; j < XXDP_UFD_ENTRIES_PER_BLOCK; j++) {
			xxdp_file_t *f;
			file_entry_start_wordnr = 1 + j * XXDP_UFD_ENTRY_WORDCOUNT; // point to start of file entry in block
			// filename: 2*3 radix50 chars
			w = xxdp_image_get_word(_this, blknr, file_entry_start_wordnr + 0);
			if (w == 0)
				continue; // invalid entry
			// create file entry
			f = malloc(sizeof(xxdp_file_t));
			f->data = NULL; //
			f->filnam[0] = 0;
			f->changed = 0;
			strcat(f->filnam, rad50_decode(w));
			w = xxdp_image_get_word(_this, blknr, file_entry_start_wordnr + 1);
			strcat(f->filnam, rad50_decode(w));
			// extension: 13 chars
			w = xxdp_image_get_word(_this, blknr, file_entry_start_wordnr + 2);
			strcpy(f->ext, rad50_decode(w));

			w = xxdp_image_get_word(_this, blknr, file_entry_start_wordnr + 3);
			f->date = dos11date_decode(w);

			// start block, scan blocklist
			w = xxdp_image_get_word(_this, blknr, file_entry_start_wordnr + 5);
			xxdp_blocklist_get(_this, &(f->blocklist), w);

			// check: filelen?
			f->block_count = xxdp_image_get_word(_this, blknr, file_entry_start_wordnr + 6);
			if (f->block_count != f->blocklist.count)
				fprintf(ferr,
						"XXDP UFD read: file %s.%s: saved file size is %d, blocklist len is %d.\n",
						f->filnam, f->ext, f->block_count, f->blocklist.count);

			// check: lastblock?
			w = xxdp_image_get_word(_this, blknr, file_entry_start_wordnr + 7);

			if (_this->file_count >= XXDP_MAX_FILES_PER_IMAGE) {
				fprintf(ferr, "XXDP UFD read: more than %d files!\n",
				XXDP_MAX_FILES_PER_IMAGE);
				return;
			}
			_this->file[_this->file_count++] = f; //save
		}
	}
}

// load and allocate file data from blocklist
// da is read in 510 byte chunks, actual size not known
static void parse_file_data(xxdp_filesystem_t *_this, xxdp_file_t *f) {
	int i;
	int block_datasize = XXDP_BLOCKSIZE - 2; // data amount in block, after link word
	uint8_t *src, *dst;
	if (f->data)
		free(f->data);
	f->data_size = f->blocklist.count * block_datasize;
	f->data = malloc(f->data_size);
	dst = f->data;
	for (i = 0; i < f->blocklist.count; i++) {
		// cat data from all blocks
		int blknr = f->blocklist.blocknr[i];
		src = IMAGE_BLOCK(_this, blknr) + 2; // skip link
		memcpy(dst, src, block_datasize);
		dst += block_datasize;
		assert(dst <= f->data + f->data_size);
	}
}

// analyse the image, build filesystem data structure
// parameters already set by _reset()
// return: 0 = OK
int xxdp_filesystem_parse(xxdp_filesystem_t *_this) {
	int i, n;
	xxdp_filesystem_init(_this);

	// read bootblock 0. may consists of 00's
	parse_multiblock(_this, _this->bootblock, _this->bootblock->blocknr, 1);
	// read monitor: from defined start until end of preallocated area, about 32
	parse_multiblock(_this, _this->monitor, _this->monitor->blocknr,
			_this->preallocated_blockcount - _this->monitor->blocknr);

	parse_mfd(_this);
	parse_bitmap(_this);
	parse_ufd(_this);

	// read data for all user files
	for (i = 0; i < _this->file_count; i++)
		parse_file_data(_this, _this->file[i]);

	xxdp_filesystem_mark_files_as_changed(_this);

	return 0;
}

/**************************************************************
 * FileAPI
 * add files to logical data structure
 **************************************************************/

// special:
// -1: bootblock
// -2: monitor
// else regular file
// fname: filnam.ext
int xxdp_filesystem_add_file(xxdp_filesystem_t *_this, int special, char *hostfname,
		time_t hostfdate, uint8_t *data, uint32_t data_size) {
	if (special == -1) {
		if (data_size != XXDP_BLOCKSIZE) {
			fprintf(ferr, "Boot block not %d bytes\n", XXDP_BLOCKSIZE);
			return -1;
		}
		_this->bootblock->data_size = data_size;
		_this->bootblock->data = realloc(_this->bootblock->data, data_size);
		memcpy(_this->bootblock->data, data, data_size);
	} else if (special == -2) {
		// assume max len of monitor
		int n = XXDP_BLOCKSIZE * (_this->preallocated_blockcount - _this->monitor->blocknr + 1);
		if (data_size > n) {
			fprintf(ferr, "Monitor too big: is %d bytes, only %d allowed\n", data_size, n);
			return -1;
		}
		_this->monitor->data_size = data_size;
		_this->monitor->data = realloc(_this->monitor->data, data_size);
		memcpy(_this->monitor->data, data, data_size);
	} else {
		xxdp_file_t *f;
		char filnam[40], ext[40];
		char *s;
		int i;
		// regular file
		if (_this->file_count + 1 >= XXDP_MAX_FILES_PER_IMAGE) {
			fprintf(ferr, "Too many files, only %d allowed\n", XXDP_MAX_FILES_PER_IMAGE);
			return -1;
		}

		// make filename.extension to "FILN  .E  "
		xxdp_filename_from_host(hostfname, filnam, ext);

		// duplicate file name? Likely! because of trunc to six letters
		for (i = 0; i < _this->file_count - 1; i++) {
			xxdp_file_t *f1 = _this->file[i];
			if (!strcasecmp(filnam, f1->filnam) && !strcasecmp(ext, f1->ext)) {
				fprintf(ferr, "Duplicate filename %s.%s", filnam, ext);
				return -1;
			}
		}
		// now insert
		f = malloc(sizeof(xxdp_file_t));
		_this->file[_this->file_count++] = f;
		f->data_size = data_size;
		f->data = malloc(data_size);
		memcpy(f->data, data, data_size);
		strcpy(f->filnam, filnam);
		strcpy(f->ext, ext);

		f->date = *localtime(&hostfdate);
		// only range 1970..1999 allowed
		if (f->date.tm_year < 70)
			f->date.tm_year = 70;
		else if (f->date.tm_year > 99)
			f->date.tm_year = 99;
	}
}

/**************************************************************
 * render
 * create an image from logical datas structure
 **************************************************************/
#define NEEDEDBLOCKS(blocksize,data_size) ( ((data_size)+(blocksize)-1) / (blocksize) )

// calculate blocklists for monitor, bitmap,mfd, ufd and files
// total blockcount may be enlarged
int xxdp_filesystem_layout(xxdp_filesystem_t *_this) {
	int i, j, n;
	int overflow;
	blocknr_t blknr;

	_this->blockcount = _this->radi->device_blocks_num;
	// may have been changed by previous run

	// mark preallocated blocks in bitmap
	// this covers boot, monitor, bitmap, mfd and default sized ufd
	memset(_this->bitmap->used, 0, sizeof(_this->bitmap->used));
	for (i = 0; i < _this->preallocated_blockcount; i++)
		_this->bitmap->used[i] = 1;

	// BOOT
	if (_this->bootblock->data_size) {
		_this->bootblock->blockcount = 1;
		blknr = _this->radi->boot_block; // set start
		_this->bootblock->blocknr = blknr;
	} else
		_this->bootblock->blockcount = 0;

	// MONITOR
	if (_this->monitor->data_size) {
		n = NEEDEDBLOCKS(XXDP_BLOCKSIZE, _this->monitor->data_size);
		blknr = _this->radi->monitor_block; // set start
		_this->monitor->blocknr = blknr;
		_this->monitor->blockcount = n;
	} else
		_this->monitor->blockcount = 0;

	// BITMAP
	n = _this->radi->bitmaps_num;
	blknr = _this->radi->bitmap_block_1; // set start
	for (i = 0; i < n; i++) { // enumerate sequential
		_this->bitmap->used[blknr] = 1;
		_this->bitmap->blocklist.blocknr[i] = blknr++;
	}
	_this->bitmap->blocklist.count = n;

	// MFD
	if (_this->mfd_variety == 1) {
		_this->mfd_blocklist->count = 2;
		_this->mfd_blocklist->blocknr[0] = _this->radi->mfd1;
		_this->mfd_blocklist->blocknr[1] = _this->radi->mfd2;
		_this->bitmap->used[_this->radi->mfd1] = 1;
		_this->bitmap->used[_this->radi->mfd2] = 1;
	} else if (_this->mfd_variety == 2) {
		_this->mfd_blocklist->count = 1;
		_this->mfd_blocklist->blocknr[0] = _this->radi->mfd1;
		_this->bitmap->used[_this->radi->mfd1] = 1;
	} else {
		fprintf(ferr, "MFD variety must be 1 or 2\n");
		exit(1);
	}
	// UFD
	// starts in preallocated area, may extend into freespace
	n = NEEDEDBLOCKS(XXDP_UFD_ENTRIES_PER_BLOCK, _this->file_count);
	if (n < _this->radi->ufd_blocks_num)
		n = _this->radi->ufd_blocks_num; // RADI defines minimum
	_this->ufd_blocklist->count = n;
	// last UFD half filled
	blknr = _this->radi->ufd_block_1;	// start
	i = 0;
	// 1) fill UFD into preallocated space
	while (i < n && i < _this->radi->ufd_blocks_num) {
		_this->bitmap->used[blknr] = 1;
		_this->ufd_blocklist->blocknr[i++] = blknr++;
	}
	// 2) continue in free space, if larger than RADI defines
	blknr = _this->preallocated_blockcount; // 1st in free space
	while (i < n) {
		_this->bitmap->used[blknr] = 1;
		_this->ufd_blocklist->blocknr[i++] = blknr++;
	}
	// blknr now 1st block behind UFD.

	// FILES

	// files start in free space
	if (blknr < _this->preallocated_blockcount)
		blknr = _this->preallocated_blockcount;

	overflow = 0;
	for (i = 0; !overflow && i < _this->file_count; i++) {
		xxdp_file_t *f = _this->file[i];
		// amount of 510 byte blocks
		n = NEEDEDBLOCKS(XXDP_BLOCKSIZE-2, f->data_size);
		for (j = 0; !overflow && j < n; j++) {
			if (j >= sizeof(f->blocklist.blocknr)) {
				fprintf(ferr, "File %s.%s too large, uses more than %d blocks", f->filnam,
						f->ext, (int) sizeof(f->blocklist.blocknr));
				overflow = 1;
			} else if ((int) blknr + 1 >= XXDP_MAX_BLOCKCOUNT) {
				fprintf(ferr, "File system overflow, can hold max %d blocks.",
				XXDP_MAX_BLOCKCOUNT);
				overflow = 1;
			} else {
				_this->bitmap->used[blknr] = 1;
				f->blocklist.blocknr[j] = blknr++;
			}
		}
		f->block_count = n;
		f->blocklist.count = n;
		if (overflow)
			return -1;
	}

	// expand file system size if needed.
	// later check wether physical image can be expanded.
	if (blknr >= _this->blockcount)
		_this->blockcount = blknr;

	//

	return 0; // OK
}

static void render_multiblock(xxdp_filesystem_t *_this, xxdp_multiblock_t *multiblock) {
	uint8_t *dst = IMAGE_BLOCK(_this, multiblock->blocknr);
	// write into sequential blocks.
	memcpy(dst, multiblock->data, multiblock->data_size);
}

// write the bitmap words of all used[] blocks
// blocklist already calculated
static void render_bitmap(xxdp_filesystem_t *_this) {
	blocknr_t blknr;

	// link blocks
	xxdp_blocklist_set(_this, &(_this->bitmap->blocklist));

	for (blknr = 0; blknr < _this->blockcount; blknr++) {
		// which bitmap block resides the flag in?
		int map_blkidx = blknr / (XXDP_BITMAP_WORDS_PER_MAP * 16); // # in list
		int map_blknr = _this->bitmap->blocklist.blocknr[map_blkidx]; // abs pos bitmap blk
		int map_flags_bitnr = blknr % (XXDP_BITMAP_WORDS_PER_MAP * 16);

		// set metadata for the whole bitmap block only if first flag is processed
		if (map_flags_bitnr == 0) {
			xxdp_image_set_word(_this, map_blknr, 1, map_blkidx + 1); // "map number":  enumerates map blocks
			xxdp_image_set_word(_this, map_blknr, 2, XXDP_BITMAP_WORDS_PER_MAP); // 60
			xxdp_image_set_word(_this, map_blknr, 3, _this->bitmap->blocklist.blocknr[0]); // "link to first map"
		}

		// set a single block flag
		if (_this->bitmap->used[blknr]) {
			// word and bit in this block containing the flag
			int map_flags_wordnr = map_flags_bitnr / 16;
			int map_flag_bitpos = map_flags_bitnr % 16;
			uint16_t map_flags = xxdp_image_get_word(_this, map_blknr, map_flags_wordnr + 4);
			map_flags |= (1 << map_flag_bitpos);
			xxdp_image_set_word(_this, map_blknr, map_flags_wordnr + 4, map_flags);
		}
	}
}

// blocklist already calculated
static void render_mfd(xxdp_filesystem_t *_this) {
	int i, n;
	blocknr_t blknr;
	// link blocks
	xxdp_blocklist_set(_this, _this->mfd_blocklist);
	if (_this->mfd_variety == 1) {
		// two block MFD1, MFD2
		assert(_this->mfd_blocklist->count == 2);
		// write MFD1
		blknr = _this->mfd_blocklist->blocknr[0];
		xxdp_image_set_word(_this, blknr, 1, _this->interleave);
		xxdp_image_set_word(_this, blknr, 2, _this->bitmap->blocklist.blocknr[0]);
		// direct list of bitmap blocks
		n = _this->bitmap->blocklist.count;
		assert(n < 252); // space for 256 - 3 - 2 = 251 bitmap blocks
		for (i = 0; i < n; i++)
			xxdp_image_set_word(_this, blknr, i + 3, _this->bitmap->blocklist.blocknr[i]);
		// terminate list
		xxdp_image_set_word(_this, blknr, n + 3, 0);

		// write MFD2
		blknr = _this->mfd_blocklist->blocknr[1];
		xxdp_image_set_word(_this, blknr, 1, 0401); // UIC[1,1]
		xxdp_image_set_word(_this, blknr, 2, _this->ufd_blocklist->blocknr[0]); // start of UFDs
		xxdp_image_set_word(_this, blknr, 3, XXDP_UFD_ENTRY_WORDCOUNT);
		xxdp_image_set_word(_this, blknr, 4, 0);
	} else if (_this->mfd_variety == 2) {
		// MFD1/2
		assert(_this->mfd_blocklist->count == 1);
		blknr = _this->mfd_blocklist->blocknr[0];
		xxdp_image_set_word(_this, blknr, 1, _this->ufd_blocklist->blocknr[0]); // ptr to 1st UFD blk
		xxdp_image_set_word(_this, blknr, 2, _this->ufd_blocklist->count);
		xxdp_image_set_word(_this, blknr, 3, _this->bitmap->blocklist.blocknr[0]); // ptr to bitmap
		xxdp_image_set_word(_this, blknr, 4, _this->bitmap->blocklist.count);
		xxdp_image_set_word(_this, blknr, 5, blknr); // to self
		xxdp_image_set_word(_this, blknr, 6, 0);
		xxdp_image_set_word(_this, blknr, 7, _this->blockcount); // # of supported blocks
		xxdp_image_set_word(_this, blknr, 8, _this->preallocated_blockcount);
		xxdp_image_set_word(_this, blknr, 9, _this->interleave);
		xxdp_image_set_word(_this, blknr, 10, 0);
		xxdp_image_set_word(_this, blknr, 11, _this->monitor->blocknr); // 1st of monitor core img
		xxdp_image_set_word(_this, blknr, 12, 0);
		// bad sector position: needs to be define in RADI for each device
		xxdp_image_set_word(_this, blknr, 13, 0);
		xxdp_image_set_word(_this, blknr, 14, 0);
		xxdp_image_set_word(_this, blknr, 15, 0);
		xxdp_image_set_word(_this, blknr, 16, 0);
	} else {
		fprintf(ferr, "MFD variety must be 1 or 2\n");
		exit(1);
	}
}

static void render_ufd(xxdp_filesystem_t *_this) {
	int i, n;
	// link blocks
	xxdp_blocklist_set(_this, _this->ufd_blocklist);
	//
	for (i = 0; i < _this->file_count; i++) {
		char buff[80];
		blocknr_t ufd_blknr = _this->radi->ufd_block_1 + (i / XXDP_UFD_ENTRIES_PER_BLOCK);
		// word nr of cur entry in cur block. skip link word.
		int ufd_word_offset = 1 + (i % XXDP_UFD_ENTRIES_PER_BLOCK) * XXDP_UFD_ENTRY_WORDCOUNT;
		xxdp_file_t *f = _this->file[i];

		xxdp_blocklist_set(_this, &f->blocklist);

		// filename chars 0..2
		strncpy(buff, f->filnam, 3);
		buff[3] = 0;
		xxdp_image_set_word(_this, ufd_blknr, ufd_word_offset + 0, rad50_encode(buff));
		// filename chars 3..5
		if (strlen(f->filnam) < 4)
			buff[0] = 0;
		else
			strncpy(buff, f->filnam + 3, 3);
		buff[3] = 0;
		xxdp_image_set_word(_this, ufd_blknr, ufd_word_offset + 1, rad50_encode(buff));
		// ext
		xxdp_image_set_word(_this, ufd_blknr, ufd_word_offset + 2, rad50_encode(f->ext));

		xxdp_image_set_word(_this, ufd_blknr, ufd_word_offset + 3, dos11date_encode(f->date));

		xxdp_image_set_word(_this, ufd_blknr, ufd_word_offset + 4, 0); // ACT-11 logical end
		xxdp_image_set_word(_this, ufd_blknr, ufd_word_offset + 5, f->blocklist.blocknr[0]); // 1st block
		xxdp_image_set_word(_this, ufd_blknr, ufd_word_offset + 6, f->blocklist.count); // file length in blocks
		n = f->blocklist.blocknr[f->blocklist.count - 1];
		xxdp_image_set_word(_this, ufd_blknr, ufd_word_offset + 7, n); // last block
		xxdp_image_set_word(_this, ufd_blknr, ufd_word_offset + 8, 0); // ACT-11 logical 52
	}
}

// write file->data[] into blocks of blocklist
static void render_file_data(xxdp_filesystem_t *_this, xxdp_file_t *f) {
	int bytestocopy = f->data_size;
	int i;
	uint8_t *src, *dst;
	src = f->data;
	for (i = 0; i < f->blocklist.count; i++) {
		int blknr = f->blocklist.blocknr[i];
		int block_datasize; // data amount in this block
		assert(bytestocopy);
		dst = IMAGE_BLOCK(_this, blknr) + 2; // write behind link word

		// data amount =n block without link word
		block_datasize = XXDP_BLOCKSIZE - 2;
		// default: transfer full block
		if (bytestocopy < block_datasize) // EOF?
			block_datasize = bytestocopy;
		memcpy(dst, src, block_datasize);
		src += block_datasize;
		bytestocopy -= block_datasize;
		assert(src <= f->data + f->data_size);
	}
	assert(bytestocopy == 0);
}

// write filesystem into image
// Assumes all file data and blocklists are valid
// return: 0 = OK
int xxdp_filesystem_render(xxdp_filesystem_t *_this) {
	int i;
	int needed_size = (int) _this->blockcount * XXDP_BLOCKSIZE;

	// calc blocklists and sizes
	if (xxdp_filesystem_layout(_this))
		return -1;

	// if autosize allowed: enlarge image
	if (*(_this->image_data_ptr) == NULL) // correct size if no buffer at all
		*(_this->image_size_ptr) = 0;
	if (needed_size > *(_this->image_size_ptr)) {
		// more bytes needed than image provides. Expand?
		if (!_this->expandable) {
			fprintf(ferr, "Image only %d bytes large, filesystem needs %d *%d = %d.\n",
					*(_this->image_size_ptr), _this->blockcount, XXDP_BLOCKSIZE, needed_size);
			return -1;
		}
		// enlarge, and update user pointers

		*(_this->image_data_ptr) = realloc(*(_this->image_data_ptr), needed_size);
		*(_this->image_size_ptr) = needed_size;
	}
	// format media, all 0's
	memset(*(_this->image_data_ptr), 0, *(_this->image_size_ptr));

	render_multiblock(_this, _this->bootblock);
	render_multiblock(_this, _this->monitor);

	render_bitmap(_this);
	render_mfd(_this);
	render_ufd(_this);

	// read data for all user files
	for (i = 0; i < _this->file_count; i++)
		render_file_data(_this, _this->file[i]);
	return 0;
}

/**************************************************************
 * Print structures
 **************************************************************/
char *xxdp_date_text(struct tm t) {
	char *mon[] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV",
			"DEC" };
	static char buff[80];
	sprintf(buff, "%2d-%3s-%02d", t.tm_mday, mon[t.tm_mon], t.tm_year);
	return buff;
}

// output like XXDP2.5
// ENTRY# FILNAM.EXT        DATE          LENGTH  START   VERSION
//
//     1  XXDPSM.SYS       1-MAR-89         29    000050   E.0
//     2  XXDPXM.SYS       1-MAR-89         39    000105
char *xxdp_dir_line(xxdp_filesystem_t *_this, int fileidx) {
	static char buff[80];
	xxdp_file_t *f = _this->file[fileidx];
	if (fileidx < 0)
		return "ENTRY# FILNAM.EXT        DATE          LENGTH  START   VERSION";
	sprintf(buff, "%5d  %6s.%-3s%15s%11d    %06o", fileidx + 1, f->filnam, f->ext,
			xxdp_date_text(f->date), f->blocklist.count, f->blocklist.blocknr[0]);
	return buff;
}

// print a DIR like XXDP
void xxdp_filesystem_print_dir(xxdp_filesystem_t *_this, FILE *stream) {
	int fileidx;
	// header
	fprintf(stream, "%s\n", xxdp_dir_line(_this, -1));
	fprintf(stream, "\n");
	for (fileidx = 0; fileidx < _this->file_count; fileidx++)
		fprintf(stream, "%s\n", xxdp_dir_line(_this, fileidx));

	fprintf(stream, "\n");
	fprintf(stream, "FREE BLOCKS: %d\n", _this->blockcount - xxdp_bitmap_count(_this));
}

void xxdp_filesystem_print_blocks(xxdp_filesystem_t *_this, FILE *stream) {
	blocknr_t blknr;
	int i, j, n;
	char line[256];
	char buff[256];
	int used;
	fprintf(stream, "Filesystem has %d blocks, usage:\n", _this->blockcount);
	for (blknr = 0; blknr < _this->blockcount; blknr++) {
		line[0] = 0;
		if (blknr == 0) {
			sprintf(buff, " BOOTBLOCK \"%s\"", _this->bootblock->filename);
			strcat(line, buff);
		}

		// monitor
		n = _this->monitor->blockcount;
		i = blknr - _this->monitor->blocknr;
		if (i >= 0 && i < n) {
			sprintf(buff, " MONITOR \"%s\" - %d/%d", _this->monitor->filename, i, n);
			strcat(line, buff);
		}

		// search mfd1, mfd2
		n = _this->mfd_blocklist->count;
		for (i = 0; i < n; i++)
			if (_this->mfd_blocklist->blocknr[i] == blknr) {
				sprintf(buff, " MFD - %d/%d", i, n);
				strcat(line, buff);
			}
		// search ufd
		n = _this->ufd_blocklist->count;
		for (i = 0; i < n; i++)
			if (_this->ufd_blocklist->blocknr[i] == blknr) {
				sprintf(buff, " UFD - %d/%d", i, n);
				strcat(line, buff);
			}
		// search bitmap
		n = _this->bitmap->blocklist.count;
		for (i = 0; i < n; i++)
			if (_this->bitmap->blocklist.blocknr[i] == blknr) {
				sprintf(buff, " BITMAP - %d/%d", i, n);
				strcat(line, buff);
			}
		// search file
		for (i = 0; i < _this->file_count; i++) {
			xxdp_file_t *f = _this->file[i];
			n = f->blocklist.count;
			for (j = 0; j < n; j++)
				if (f->blocklist.blocknr[j] == blknr) {
					sprintf(buff, " file #%d: \"%s.%s\" - %d/%d", i, f->filnam, f->ext, j, n);
					strcat(line, buff);
				}
		}
		// block marked in bitmap?
		used = _this->bitmap->used[blknr];
		if ((!used && line[0]) || (used && !line[0])) {
			sprintf(buff, " Bitmap mismatch, marked as %s!", used ? "USED" : "NOT USED");
			strcat(line, buff);
		}
		if (line[0]) {
			int offset = (IMAGE_BLOCK(_this, blknr) - IMAGE_BLOCK(_this, 0));
			fprintf(stream, "%5d @ 0x%06x = %#08o:  %s\n", blknr, offset, offset, line);
		}
	}
	n = xxdp_bitmap_count(_this);
	fprintf(stream, "Blocks marked as \"used\" in bitmap: %d. Free: %d - %d = %d.\n", n,
			_this->blockcount, n, _this->blockcount - n);
}

/* convert filenames and timestamps */
char *xxdp_filename_to_host(char *filnam, char *ext) {
	static char result[80];
	char _filnam[80], _ext[80];
	// remove surrounding spaces.
	// all XXDP filename chars are valid host chars (" ", "$", "%")
	// return concat "filnam.ext"
	strcpy(_filnam, strtrim(filnam));
	strcpy(_ext, strtrim(ext));
	if (!strlen(_filnam))
		strcpy(_filnam, "_");
	strcpy(result, _filnam);
	if (strlen(_ext)) {
		strcat(result, ".");
		strcat(result, _ext);
	}
	return result;
}

// result ist filnam.ext, without spaces
// "filname" and "ext" contain components WITH spaces, if != NULL
// "bla.foo.c" => "BLA.FO", "C  ", resulr = "BLA.FO.C"
char *xxdp_filename_from_host(char *hostfname, char *filnam, char *ext) {
	static char result[80];
	char pathbuff[4096];
	char _filnam[7], _ext[4];
	char *s, *t, *dotpos;

	strcpy(pathbuff, hostfname);

	// upcase and replace forbidden characters
	for (s = pathbuff; *s; s++)
		if (*s == '_')
			*s = ' ';
		else {
			*s = toupper(*s);
			if (!strchr(" ABCDEFGHIJKLMNOPQRSTUVWXYZ$.%0123456789", *s))
				*s = '%';
		}

	// search last "."
	dotpos = NULL;
	for (s = pathbuff; *s; s++)
		if (*s == '.')
			dotpos = s;

	if (dotpos)
		*dotpos = 0; // sep words

	// extract 6 char filnam
	s = pathbuff;
	t = _filnam;
	while (*s && (s - pathbuff) < 6)
		*t++ = *s++;
	*t = 0;

	// extract 3 char ext
	t = _ext;
	if (dotpos) {
		s = dotpos + 1;
		while (s && *s && (s - dotpos - 1) < 3)
			*t++ = *s++;
	}
	*t = 0;

	// return space-padded components
	if (filnam) // length MUST be > 7
		sprintf(filnam, "%-6s", _filnam);
	if (ext) // length MUST be > 4
		sprintf(ext, "%-3s", _ext);

	strcpy(result, _filnam);
	if (strlen(_ext)) {
		strcat(result, ".");
		strcat(result, _ext);
	}

	return result;
}
