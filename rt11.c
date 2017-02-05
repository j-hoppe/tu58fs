/*
 *  rt11.c: Handles the RT11 filesystem on an image.
 *
 *  Copyright (c) 2017, Joerg Hoppe j_hoppe@t-online.de, www.retrocmp.com
 *  (C) 2005-2016 Donald N North <ak6dn_at_mindspring_dot_com>
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
 *  Created on: 22.01.2017
 *
 * The RT-11 filesystems allows extra bytes in directory entries
 * and a "prefix" for each files.
 * These are saved as <name>.<ext>.extradir and .prefix
 * extra data are hold in rt11_file_t
 *
 *  A RT-11 filesystem is generated on INIT with a fix amount of directory segments
 *  "render()" calculates these to allow 1.5 * the amount of new possible files of average length
 *  in the free list.
 *
 *	All info taken from AA-PD6PA-TC_RT-11_Volume_and_File_Formats_Manual_Aug91.pdf
 *  = VFFM91
 */
#define _RT11_C_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>

#include "error.h"
#include "image.h"
#include "rt11.h"	// own

// sort order for files. For regexes the '.' must be escaped by '\'.
// and a * is .*"
char * rt11_fileorder[] = {
		//  reproduce test tape
		"RT11.*\\.SYS", "DD\\.SYS", "SWAP\\.SYS", "TT\\.SYS", "DL\\.SYS", "STARTS\\.COM",
		"DIR\\.SAV", "DUP\\.SAV",
		NULL };

#ifdef DEFAULTBOOTLOADER
// content of boot block, if volume is not bootable
// == no COPY/BOOT issued.
// device independent, created with Rt11 V5.3 INIT

// DO NOT USE!
// Create default bootloader with RT11 INIT => shared dir is updated
// no $BOOT.BLK on shared dir => all 00s in bootblock, no file existant
uint8_t rt11_nobootblock[512] = { //
	0xA0, 0x00, 0x05, 0x00, 0x04, 0x01, 0x00, 0x00,//
	0x00, 0x00, 0x10, 0x43, 0x10, 0x9C, 0x00, 0x01,//
	0x37, 0x08, 0x24, 0x00, 0x0D, 0x00, 0x00, 0x00,//
	0x00, 0x0A, 0x3F, 0x42, 0x4F, 0x4F, 0x54, 0x2D,//
	0x55, 0x2D, 0x4E, 0x6F, 0x20, 0x62, 0x6F, 0x6F,//
	0x74, 0x20, 0x6F, 0x6E, 0x20, 0x76, 0x6F, 0x6C,//
	0x75, 0x6D, 0x65, 0x0D, 0x0A, 0x0A, 0x80, 0x00,//
	0xDF, 0x8B, 0x74, 0xFF, 0xFD, 0x80, 0x1F, 0x94,//
	0x76, 0xFF, 0xFA, 0x80, 0xFF, 0x01, 0x00, 0x00,//
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
#endif

/*************************************************************
 * low level operators
 *************************************************************/

// ptr to first byte of block
#define IMAGE_BLOCKNR2PTR(_this,blocknr) ((_this)->image_data + (RT11_BLOCKSIZE *(blocknr)))
// convert pointer in image to block
#define IMAGE_PTR2BLOCKNR(_this,ptr) ( ((uint8_t*)(ptr) - (_this)->image_data) / RT11_BLOCKSIZE)
// offset in block in bytes
#define IMAGE_PTR2BLOCKOFFSET(_this,ptr) ( ((uint8_t*)	(ptr) - (_this)->image_data) % RT11_BLOCKSIZE)

#define IMAGE_GET_WORD(ptr) (  (uint16_t) ((uint8_t *)(ptr))[0]  |  (uint16_t) ((uint8_t *)(ptr))[1] << 8  )
#define IMAGE_PUT_WORD(ptr,w) ( ((uint8_t *)(ptr))[0] = (w) & 0xff, ((uint8_t *)(ptr))[1] = ((w) >> 8) & 0xff  )

// fetch/write a 16bit word in the image
// LSB first
static uint16_t rt11_image_get_word_at(rt11_filesystem_t *_this, rt11_blocknr_t blocknr,
		uint32_t byte_offset) {
	uint32_t idx = RT11_BLOCKSIZE * blocknr + byte_offset;
	assert(idx >= 0 && (idx + 1) < _this->image_size);

	return _this->image_data[idx] | (_this->image_data[idx + 1] << 8);
}

static void rt11_image_set_word_at(rt11_filesystem_t *_this, rt11_blocknr_t blocknr,
		uint32_t byte_offset, uint16_t val) {
	uint32_t idx = RT11_BLOCKSIZE * blocknr + byte_offset;
	assert(idx >= 0 && (idx + 1) < _this->image_size);

	_this->image_data[idx] = val & 0xff;
	_this->image_data[idx + 1] = (val >> 8) & 0xff;
	//fprintf(ferr, "set 0x%x to 0x%x\n", idx, val) ;
}

static void stream_init(rt11_stream_t *stream) {
	stream->blocknr = 0;
	stream->byte_offset = 0;
	stream->data = NULL;
	stream->data_size = 0;
	stream->name[0] = 0;
}

static rt11_stream_t *stream_create() {
	rt11_stream_t *result = malloc(sizeof(rt11_stream_t));
	stream_init(result);
	return result;
}

// read block[start] ... block[start+blockcount-1] into data[]
static void stream_parse(rt11_filesystem_t *_this, rt11_stream_t *stream, rt11_blocknr_t start,
		uint32_t byte_offset, uint32_t data_size) {
	stream->blocknr = start;
	stream->byte_offset = byte_offset;
	stream->data_size = data_size;
	stream->data = malloc(stream->data_size);
	memcpy(stream->data, IMAGE_BLOCKNR2PTR(_this, start) + byte_offset, stream->data_size);
	stream->name[0] = 0; // must be set by caller
}

// write stream to image
static void stream_render(rt11_filesystem_t *_this, rt11_stream_t *stream) {
	uint8_t *dst = IMAGE_BLOCKNR2PTR(_this,stream->blocknr) + stream->byte_offset;
	memcpy(dst, stream->data, stream->data_size);
}

// read block[start] ... block[start+blockcount-1] into data[]
static void stream_destroy(rt11_stream_t *stream) {
	if (stream) {
		if (stream->data)
			free(stream->data);
		free(stream);
	}
}

rt11_file_t *rt11_file_create() {
	rt11_file_t *file = malloc(sizeof(rt11_file_t));
	file->data = NULL;
	file->dir_ext = NULL;
	file->prefix = NULL;
	file->filnam[0] = 0;
	file->ext[0] = 0;
	file->block_count = 0;
	memset(&file->date, 0, sizeof(struct tm));
	file->fixed = 0;
	file->readonly = 0;
	return file;
}

static void rt11_file_destroy(rt11_file_t * file) {
	if (file) {
		stream_destroy(file->data);
		stream_destroy(file->dir_ext);
		stream_destroy(file->prefix);
		free(file);
	}
}

// needed dir segments for given count of entries
// usable in 1 segment : 2 blocks - 5 header words
// entry size = 7 words + dir_entry_extra_bytes
static int rt11_dir_entries_per_segment(rt11_filesystem_t *_this) {
	// without extra bytes: 72 [VFFM91] 1-15
	int result = (2 * RT11_BLOCKSIZE - 2 * 5) / (2 * 7 + _this->dir_entry_extra_bytes);
	// in a segment 3 entries spare, including end-of-segment
	result -= 3;
	return result;
}
static int rt11_dir_needed_segments(rt11_filesystem_t *_this, int file_count) {
	// without extra bytes: 72 [VFFM91] 1-15
	int entries_per_seg = rt11_dir_entries_per_segment(_this);
	file_count++; // one more for the mandatory "empty space" file entry
	// round up to whole segments
	return (file_count + entries_per_seg - 1) / entries_per_seg;
}

static void rt11_filesystem_mark_filestream_as_changed(rt11_filesystem_t *_this,
		rt11_stream_t *stream) {
	rt11_blocknr_t blknr, blkend;
	if (!stream)
		return;
	stream->changed = 0;
	if (_this->image_changed_blocks) {
		blkend = stream->blocknr + NEEDED_BLOCKS(RT11_BLOCKSIZE, stream->data_size);
		for (blknr = stream->blocknr; !stream->changed && blknr < blkend; blknr++)
			stream->changed |= BOOLARRAY_BIT_GET(_this->image_changed_blocks, blknr);
//		stream->changed |= boolarray_bit_get(_this->image_changed_blocks, blknr);
		// possible optimization: boolarray is tested sequentially
	}
}

static void rt11_filesystem_mark_filestreams_as_changed(rt11_filesystem_t *_this) {
	int i;
	rt11_blocknr_t blknr;
	// boolarray_print_diag(_this->image_changed_blocks,stderr, _this->blockcount, "RT11") ;
	// bootblock
	rt11_filesystem_mark_filestream_as_changed(_this, _this->bootblock);
	rt11_filesystem_mark_filestream_as_changed(_this, _this->monitor);

	// Homeblock changed?
	_this->struct_changed = BOOLARRAY_BIT_GET(_this->image_changed_blocks, 1);
	// any dir entries changed?
	for (blknr = _this->first_dir_blocknr;
			blknr < _this->first_dir_blocknr + 2 * _this->dir_total_seg_num; blknr++)
		_this->struct_changed |= BOOLARRAY_BIT_GET(_this->image_changed_blocks, blknr);

	// rt11_filesystem_mark_filestream_as_changed(_this, _this->monitor);
	for (i = 0; i < _this->file_count; i++) {
		rt11_file_t *f = _this->file[i];
		rt11_filesystem_mark_filestream_as_changed(_this, f->prefix);
		rt11_filesystem_mark_filestream_as_changed(_this, f->data);
	}
}

/*************************************************************************
 * constructor / destructor
 *************************************************************************/

rt11_filesystem_t *rt11_filesystem_create(device_type_t dec_device, uint8_t *image_data,
		uint32_t image_size, boolarray_t *changedblocks) {
	int i;
	rt11_filesystem_t *_this;

	_this = malloc(sizeof(rt11_filesystem_t));
	_this->dec_device = dec_device;
	_this->device_info = (device_info_t*) search_tagged_array(device_info_table,
			sizeof(device_info_t), _this->dec_device);
	assert(_this->device_info);

	// Search Random Access Device Information
	_this->radi = (rt11_radi_t*) search_tagged_array(rt11_radi, sizeof(rt11_radi_t),
			_this->dec_device);
	assert(_this->radi);

	// save pointer to variables defining image buffer data
	_this->image_data = image_data;
	_this->image_size = image_size;
	_this->image_changed_blocks = changedblocks;

	// these are always there, static allocated
#ifdef DEFAULTBOOTLOADER
	_this->nobootblock = malloc(sizeof(rt11_stream_t)); // fix bootblock for no bootable volumes
	stream_init(_this->nobootblock);
#endif
	_this->bootblock = malloc(sizeof(rt11_stream_t));
	stream_init(_this->bootblock);
	_this->monitor = malloc(sizeof(rt11_stream_t));
	stream_init(_this->monitor);

	// files vary: dynamic allocate
	_this->file_count = 0;
	for (i = 0; i < RT11_MAX_FILES_PER_IMAGE; i++)
		_this->file[i] = NULL;
	rt11_filesystem_init(_this);
	return _this;
}

void rt11_filesystem_destroy(rt11_filesystem_t *_this) {
	rt11_filesystem_init(_this); // free files
#ifdef DEFAULTBOOTLOADER
			free(_this->nobootblock);
#endif

	free(_this->bootblock);
	free(_this->monitor);
	free(_this);
}

// free / clear all structures, set default values
void rt11_filesystem_init(rt11_filesystem_t *_this) {
	int i;
	// set device params

	// image may be variable sized !
	_this->blockcount = NEEDED_BLOCKS(RT11_BLOCKSIZE, _this->image_size);

	/*
	 _this->blockcount = _this->radi->block_count;
	 */

	if (_this->blockcount < 0) {
		fprintf(stderr,
				"rt11_filesystem_init(): RT-11 blockcount for device %s not yet defined!",
				_this->device_info->device_name);
		exit(1);
	}

	// trunc large devices, only 64K blocks addressable = 32MB
	// no support for partitioned disks at the moment
	assert(_this->blockcount <= RT11_MAX_BLOCKCOUNT);

#ifdef DEFAULTBOOTLOADER
	// link default bootblock to const byte array
	_this->nobootblock->blocknr = 0;
	_this->nobootblock->byte_offset = 0;
	_this->nobootblock->data = rt11_nobootblock;
	_this->nobootblock->data_size = 512;
#endif

	if (_this->bootblock->data)
		free(_this->bootblock->data);
	stream_init(_this->bootblock);

	if (_this->monitor->data)
		free(_this->monitor->data);
	stream_init(_this->monitor);

	for (i = 0; i < RT11_MAX_FILES_PER_IMAGE; i++) {
		rt11_file_destroy(_this->file[i]);
		_this->file[i] = NULL;
	}
	_this->file_count = 0;

	// defaults for home block, according to [VFFM91], page 1-3
	_this->pack_cluster_size = 1;
	_this->first_dir_blocknr = 6;
	strcpy(_this->system_version, "V3A");
	strcpy(_this->volume_id, "RT11A       ");
	strcpy(_this->owner_name, "            ");
	strcpy(_this->system_id, "DECRT11A    ");
	_this->dir_entry_extra_bytes = 0;
	_this->homeblock_chksum = 0;
	_this->struct_changed = 0;
}

// calculate ratio between directory segments and data blocks
// Pre: files filled in
// output:
//	used_blocks
//  dir_total_seg_num
//	dir_max_seg_nr
// 2 Modi:
// a) test_data_size == 0: calc on base of file[], change file system
// b) test_data_size > 0: check wether file of length "test_data_size" would fit onto
//	the existng volume
static int rt11_filesystem_calc_block_use(rt11_filesystem_t *_this, int test_data_size) {
	int i;
	int dir_max_seg_nr;
	int used_file_blocks;
	int available_blocks;

	if (_this->dir_entry_extra_bytes > 16) {
		fprintf(stderr, "Extra bytes in directory %d is > 16 ... how much is allowed?\n",
				_this->dir_entry_extra_bytes);
		exit(1);
	}

	// 1) calc segments & blocks needed for existing files
	used_file_blocks = 0;
	for (i = 0; i < _this->file_count; i++) {
		// round file sizes up to blocks
		// prefix size and data size already sum'd up to file->block_count
		used_file_blocks += _this->file[i]->block_count;
	}
	if (test_data_size)
		used_file_blocks += NEEDED_BLOCKS(RT11_BLOCKSIZE, test_data_size);

	// total blocks available for dir and data
	// On disk supporting STd144 bad secotr info,
	// "available blocks" should not be calculated from total disk size,
	// but from usable blockcount of "rt11_radi".
	// Difficulties in case of enlarged images!
	available_blocks = _this->blockcount - _this->first_dir_blocknr; // boot, home, 2..5 used
	if (test_data_size)
		dir_max_seg_nr = rt11_dir_needed_segments(_this, _this->file_count + 1);
	else
		dir_max_seg_nr = rt11_dir_needed_segments(_this, _this->file_count);
	if (available_blocks < used_file_blocks + 2 * dir_max_seg_nr) {
		// files do not fit on volume
		if (!test_data_size)
			_this->free_blocks = 0; // can't be negative
		return error_set(ERROR_FILESYSTEM_OVERFLOW, "rt11_filesystem_calc_block_use");
	}
	if (test_data_size)
		return ERROR_OK;

	/* end of test mode */
	// now modify file system
	_this->dir_max_seg_nr = dir_max_seg_nr;
	_this->used_file_blocks = used_file_blocks;

	_this->free_blocks = available_blocks - _this->used_file_blocks - 2 * _this->dir_max_seg_nr;
	// now used_blocks,free_blocks, dir_total_seg_num valid.

	/* Plan use of remaining free_space
	 * how many files would be allocated in the remaining free space?
	 * derive from average file size, but allow 1.5 * as much
	 * Most critical test situations:
	 * All dir segments full, and only 2 block in file area left.
	 * assigned these blocks to 1 more file would need a new dir segment,
	 * which would need these 2 blocks too.
	 * If 3 blocks are left: 2 can be used for additional dir segmetn,
	 * and 1 for new file.
	 */
	if (_this->file_count == 0) {
		// if disk empty: start with only 1 segment
		_this->dir_max_seg_nr = 1;
		_this->dir_total_seg_num = 1;
	} else {
		// planning params have prefix "_"
		int avg_file_blocks; // average filesize in blocks
		int _new_file_count; // planned count of additional files
		int _used_file_blocks; // planned block requirement for all files (existing + planned)
		int _dir_total_seg_num; // planned dir segment requirement for all files (existing + planned)
		avg_file_blocks = _this->used_file_blocks / _this->file_count;
		if (avg_file_blocks < 1)
			avg_file_blocks = 1;
		// 1st estimate for possible new files.
		// Assume they have average size.
		// too big, since additional dir segments reduce free space
		_new_file_count = _this->free_blocks / avg_file_blocks + 1;
		// reduce amount of planned files, until files+dir fit on disk
		do {
			_new_file_count--;
			_used_file_blocks = _this->used_file_blocks + _new_file_count * avg_file_blocks;
			// plan for 50% more file count
			_dir_total_seg_num = rt11_dir_needed_segments(_this,
					_this->file_count + (_new_file_count * 3) / 2);
		} while (_new_file_count
				&& available_blocks < _used_file_blocks + 2 * _dir_total_seg_num);
		// solution found: save
		if (_dir_total_seg_num > 31)
			_dir_total_seg_num = 31;
		_this->dir_total_seg_num = _dir_total_seg_num;
	}
	// calculate free blocks again
	assert(available_blocks >= _this->used_file_blocks + 2 * _this->dir_total_seg_num);
	_this->free_blocks = available_blocks - _this->used_file_blocks
			- 2 * _this->dir_total_seg_num;

	return ERROR_OK;
}

/**************************************************************
 * _parse()
 * convert byte array of image into logical objects
 **************************************************************/

static void parse_homeblock(rt11_filesystem_t *_this) {
	uint16_t w;
	char *s;
	int i;
	int sum;

	// bad block bitmap not needed

	// INIT/RESTORE area: ignore
	// BUP ignored

	_this->pack_cluster_size = rt11_image_get_word_at(_this, 1, 0722);
	_this->first_dir_blocknr = rt11_image_get_word_at(_this, 1, 0724);
	if (_this->first_dir_blocknr != 6)
		fprintf(ferr, "first_dir_blocknr expected 6, is %d\n", _this->first_dir_blocknr);
	_this->first_dir_blocknr = rt11_image_get_word_at(_this, 1, 0724);
	w = rt11_image_get_word_at(_this, 1, 0726);
	strcpy(_this->system_version, rad50_decode(w));
	// 12 char volume id. V3A, or V05, ...
	s = IMAGE_BLOCKNR2PTR(_this, 1) + 0730;
	strncpy(_this->volume_id, s, 12);
	_this->volume_id[12] = 0;
	// 12 char owner name
	s = IMAGE_BLOCKNR2PTR(_this, 1) + 0744;
	strncpy(_this->owner_name, s, 12);
	_this->owner_name[12] = 0;
	// 12 char system id
	s = IMAGE_BLOCKNR2PTR(_this, 1) + 0760;
	strncpy(_this->system_id, s, 12);
	_this->system_id[12] = 0;
	_this->homeblock_chksum = rt11_image_get_word_at(_this, 1, 0776);
	// verify checksum. But found a RT-11 which writes 0000 here?
	for (sum = i = 0; i < 0776; i += 2)
		sum += rt11_image_get_word_at(_this, 1, i);
	sum &= 0xffff;
	/*
	 if (sum != _this->homeblock_chksum)
	 fprintf(ferr, "Home block checksum error: is 0x%x, expected 0x%x\n", sum,
	 (int) _this->homeblock_chksum);
	 */
}

// popint to start of  directory segment [i]
// segment[0] at directory_startblock (6), and 1 segment = 2 blocks. i starts with 1.
#define DIR_SEGMENT(_this,i)  ( (uint16_t *) ((_this)->image_data+ (_this)->first_dir_blocknr*RT11_BLOCKSIZE +(((i)-1)*2*RT11_BLOCKSIZE)) )

static void parse_directory(rt11_filesystem_t *_this) {
	uint16_t *ds; // ptr to start of directory segment
	uint32_t ds_nr = 0; // runs from 1
	uint32_t ds_next_nr = 0;
	uint32_t w;
	uint16_t de_data_blocknr; // start blocknumber for file data

	uint16_t *de; // directory entry in current directory segment
	uint16_t de_nr; // directory entry_nr, runs from 0
	uint16_t de_len; // total size of directory entry in words
	// by this segment begins.

	/*** iterate directory segments ***/
	_this->used_file_blocks = 0;
	_this->free_blocks = 0;

	ds_nr = 1;
	do {
		ds = DIR_SEGMENT(_this, ds_nr);
		// read 5 word directory segment header
		w = IMAGE_GET_WORD(ds + 0);
		if (ds_nr == 1)
			_this->dir_total_seg_num = w;
		else if (w != _this->dir_total_seg_num) {
			fprintf(ferr,
					"parse_directory(): ds_header_total_seg_num in entry %d different from entry 1\n",
					ds_nr);
		}
		if (ds_nr == 1)
			_this->dir_max_seg_nr = IMAGE_GET_WORD(ds + 2);
		ds_next_nr = IMAGE_GET_WORD(ds + 1); // nr of next segment
		if (ds_next_nr > _this->dir_max_seg_nr)
			fprintf(ferr, "parse_directory(): next segment nr %d > maximum %d\n", ds_next_nr,
					_this->dir_max_seg_nr);
		de_data_blocknr = IMAGE_GET_WORD(ds + 4);
		if (ds_nr == 1) {
			_this->dir_entry_extra_bytes = IMAGE_GET_WORD(ds + 3);
			_this->file_space_blocknr = de_data_blocknr; // 1st dir entry
		}
		//

		/*** iterate directory entries in segment ***/
		de_len = 7 + _this->dir_entry_extra_bytes / 2;
		de_nr = 0;
		de = ds + 5; // 1st entry 5 words after segment start
		while (!(IMAGE_GET_WORD(de) & RT11_DIR_EEOS)) { // end of segment?
			uint16_t de_status = IMAGE_GET_WORD(de);
			if (de_status & RT11_FILE_EMPTY) { // skip empty entries
				w = IMAGE_GET_WORD(de + 4);
				_this->free_blocks += w;
			} else if (de_status & RT11_FILE_EPERM) { // only permanent files
				// new file! read dir entry
				rt11_file_t *f = rt11_file_create();
				f->status = de_status;
				// filnam: 6 chars
				w = IMAGE_GET_WORD(de + 1);
				strcat(f->filnam, rad50_decode(w));
				w = IMAGE_GET_WORD(de + 2);
				strcat(f->filnam, rad50_decode(w));
				// extension: 3 chars
				w = IMAGE_GET_WORD(de + 3);
				strcpy(f->ext, rad50_decode(w));
				// blocks in data stream
				f->block_nr = de_data_blocknr; // startblock on disk
				f->block_count = IMAGE_GET_WORD(de + 4);
				_this->used_file_blocks += f->block_count;
				// fprintf(stderr, "parse %s.%s, %d blocks @ %d\n", f->filnam, f->ext,	f->block_count, f->block_nr);
				// ignore job/channel
				// creation date
				w = IMAGE_GET_WORD(de + 6);
				// 5 bit year, 2 bit "age". Year since 1972
				f->date.tm_year = 72 + (w & 0x1f) + 32 * ((w >> 14) & 3);
				f->date.tm_mday = (w >> 5) & 0x1f;
				f->date.tm_mon = ((w >> 10) & 0x0f) - 1;
				// "readonly", if either EREAD or EPROT)
				f->readonly = 0;
				if (f->status & (RT11_FILE_EREAD))
					f->readonly = 1;
				if (f->status & (RT11_FILE_EPROT))
					f->readonly = 1;

				// Extract extra bytes in directory entry as stream ...
				if (_this->dir_entry_extra_bytes) {
					assert(f->dir_ext == NULL);
					f->dir_ext = stream_create();
					stream_parse(_this, f->dir_ext,
					/*start block*/IMAGE_PTR2BLOCKNR(_this, de + 7),
					/* byte_offset*/IMAGE_PTR2BLOCKOFFSET(_this, de + 7),
							_this->dir_entry_extra_bytes);
					strcpy(f->dir_ext->name, RT11_STREAMNAME_DIREXT);
					// generate only a stream if any bytes set <> 00
					if (is_memset(f->dir_ext->data, 0, f->dir_ext->data_size)) {
						stream_destroy(f->dir_ext);
						f->dir_ext = NULL;
					}
				}

				if (_this->file_count >= RT11_MAX_FILES_PER_IMAGE) {
					fprintf(ferr, "parse_directory(): more than %d files!\n",
					RT11_MAX_FILES_PER_IMAGE);
					return;
				}
				_this->file[_this->file_count++] = f; //save
			}

			// advance file start block in data area, also for empty entries
			de_data_blocknr += IMAGE_GET_WORD(de + 4);

			// next dir entry
			de_nr++;
			de += de_len;
			if (de - ds > 2 * RT11_BLOCKSIZE) // 1 segment = 2 blocks
				fprintf(ferr, "parse_directory(): list of entries exceeds %d bytes\n",
						2 * RT11_BLOCKSIZE);
		}

		// next segment
		ds_nr = ds_next_nr;
	} while (ds_nr > 0);
}

// parse prefix and data blocks
static void parse_file_data(rt11_filesystem_t *_this) {
	int i;
	rt11_blocknr_t prefix_block_count;
	uint8_t *data_ptr;

	for (i = 0; i < _this->file_count; i++) {
		rt11_file_t *f = _this->file[i];
		// fprintf(stderr, "%d %s.%s\n", i, f->filnam, f->ext) ;
		// data area may have "prefix" block.
		// format not mandatory, use DEC recommendation
		if (f->status & RT11_FILE_EPRE) {
			data_ptr = IMAGE_BLOCKNR2PTR(_this, f->block_nr);
			prefix_block_count = *data_ptr; // first byte in block
			// DEC: low byte of first word = blockcount
			assert(f->prefix == NULL);
			f->prefix = stream_create();
			// stream is everything behind first word
			stream_parse(_this, f->prefix, f->block_nr, 2,
					prefix_block_count * RT11_BLOCKSIZE - 2);
			strcpy(f->prefix->name, RT11_STREAMNAME_PREFIX);
		} else
			prefix_block_count = 0;

		// after prefix: remaining blocks are data
		assert(f->data == NULL);
		f->data = stream_create();
		stream_parse(_this, f->data, f->block_nr + prefix_block_count, 0,
				(f->block_count - prefix_block_count) * RT11_BLOCKSIZE);
	}
}

// analyse the image, build filesystem data structure
// parameters already set by _reset()
// return: 0 = OK
int rt11_filesystem_parse(rt11_filesystem_t *_this) {
	rt11_filesystem_init(_this);

	// read bootblock at 0, length 1 block. may consists of 00's
	// allocates bootblock->data
	stream_parse(_this, _this->bootblock, 0, 0, RT11_BLOCKSIZE);

	// read more boot code from blocks 2..5, length 4 block.
	stream_parse(_this, _this->monitor, 2, 0, 4 * RT11_BLOCKSIZE);

	parse_homeblock(_this);
	parse_directory(_this);
	parse_file_data(_this);
	// rt11_filesystem_print_diag(_this, stderr);

	// test: set a block bit, must result in "changed file
	// boolarray_bit_set(_this->image_changed_blocks, 0100) ;

	// mark file->data , ->prefix as changed, for changed image blocks
	rt11_filesystem_mark_filestreams_as_changed(_this);

	return ERROR_OK;
}

/**************************************************************
 * render
 * create an binary image from logical data structure
 **************************************************************/

// calculate blocklists for monitor, bitmap,mfd, ufd and files
// total blockcount may be enlarged
// Pre: files filled in
static int rt11_filesystem_layout(rt11_filesystem_t *_this) {
	int i;
	int file_start_blocknr;

	if (_this->bootblock->data_size) {
		_this->bootblock->blocknr = 0;
		if (_this->bootblock->data_size != RT11_BLOCKSIZE)
			return error_set(ERROR_FILESYSTEM_FORMAT, "bootblock has illegal size of %d bytes.",
					_this->bootblock->data_size);
	}
	if (_this->monitor->data_size) {
		_this->monitor->blocknr = 2; // 2..5
		if (_this->monitor->data_size > 4 * RT11_BLOCKSIZE)
			return error_set(ERROR_FILESYSTEM_FORMAT, "monitor has illegal size of %d bytes.",
					_this->monitor->data_size);
	}

	if (rt11_filesystem_calc_block_use(_this, 0))
		return error_code;
	// free, used blocks, dir_total_seg_num now set

	// file area begins after directory segment list
	file_start_blocknr = _this->first_dir_blocknr + 2 * _this->dir_total_seg_num;
	_this->file_space_blocknr = file_start_blocknr;
	for (i = 0; i < _this->file_count; i++) {
		rt11_file_t *f = _this->file[i];
		f->block_nr = file_start_blocknr;
		// set start of prefix and data
		if (f->prefix) {
			f->prefix->blocknr = file_start_blocknr;
			// prefix needs 1 extra word for blockcount
			f->prefix->byte_offset = 2;
			file_start_blocknr += NEEDED_BLOCKS(RT11_BLOCKSIZE, f->prefix->data_size + 2);
		}
		if (f->data) {
			f->data->blocknr = file_start_blocknr;
			file_start_blocknr += NEEDED_BLOCKS(RT11_BLOCKSIZE, f->data->data_size);
		}
		// f->block_count set in file_stream_add()
		assert(file_start_blocknr - f->block_nr == f->block_count);
	}
	// save begin of free space for _render()
	_this->render_free_space_blocknr = file_start_blocknr;

	return ERROR_OK;
}

static void render_homeblock(rt11_filesystem_t *_this) {
	uint8_t *homeblk = IMAGE_BLOCKNR2PTR(_this, 1);
	uint16_t w;
	char *s;
	int i, sum;

	memset(homeblk, 0, RT11_BLOCKSIZE);
	// write the bad block replacement table
	// no idea about it, took from TU58 and RL02 image and from Don North
	rt11_image_set_word_at(_this, 1, 0, 0000000);
	rt11_image_set_word_at(_this, 1, 2, 0170000);
	rt11_image_set_word_at(_this, 1, 4, 0007777);

	// rest until 0203 was found to be 0x43 (RL02) or 0x00 ?

	// INITIALIZE/RESTORE data area 0204-0251 == 0x084-0xa9
	// leave blank

	// BUP information area 0252-0273 == 0xaa-0xbb found as 00's

	rt11_image_set_word_at(_this, 1, 0722, _this->pack_cluster_size);
	rt11_image_set_word_at(_this, 1, 0724, _this->first_dir_blocknr);

	w = rad50_encode(_this->system_version);
	rt11_image_set_word_at(_this, 1, 0726, w);

	// 12 char volume id. V3A, or V05, ...
	s = IMAGE_BLOCKNR2PTR(_this, 1) + 0730;
	// always 12 chars long, right padded with spaces
	strcpy(s, strrpad(_this->volume_id, 12, ' '));

	// 12 char owner name
	s = IMAGE_BLOCKNR2PTR(_this, 1) + 0744;
	strcpy(s, strrpad(_this->owner_name, 12, ' '));

	// 12 char system id
	s = IMAGE_BLOCKNR2PTR(_this, 1) + 0760;
	strcpy(s, strrpad(_this->system_id, 12, ' '));

	// build checksum over all words
	for (sum = i = 0; i < 0776; i += 2)
		sum += rt11_image_get_word_at(_this, 1, i);
	sum &= 0xffff;
	_this->homeblock_chksum = sum;
	rt11_image_set_word_at(_this, 1, 0776, sum);
}

// write file f into segment ds_nr and entry de_nr
// if f = NULL: write free chain entry
// must be called with ascending de_nr
static int render_directory_entry(rt11_filesystem_t *_this, rt11_file_t *f, int ds_nr,
		int de_nr) {
	uint16_t *ds = DIR_SEGMENT(_this, ds_nr); // ptr to dir segment in image
	uint16_t *de; // ptr to dir entry in image
	int dir_entry_word_count = 7 + (_this->dir_entry_extra_bytes / 2);
	uint16_t w;
	char buff[80];
	if (de_nr == 0) {
		// 1st entry in segment: write 5 word header
		IMAGE_PUT_WORD(ds + 0, _this->dir_total_seg_num);
		if (ds_nr == _this->dir_max_seg_nr)
			IMAGE_PUT_WORD(ds + 1, 0); // last segment
		else
			IMAGE_PUT_WORD(ds + 1, ds_nr + 1); // link to next segment
		IMAGE_PUT_WORD(ds + 2, _this->dir_max_seg_nr);
		IMAGE_PUT_WORD(ds + 3, _this->dir_entry_extra_bytes);
		if (f)
			IMAGE_PUT_WORD(ds + 4, f->block_nr); // start of first file on disk
		else
			// end marker at first entry
			IMAGE_PUT_WORD(ds + 4, _this->file_space_blocknr);
	}
	// write dir_entry
	de = ds + 5 + de_nr * dir_entry_word_count;
	//fprintf(stderr, "ds_nr=%d, de_nr=%d, ds in img=0x%lx, de in img =0x%lx\n", ds_nr, de_nr,
	//		(uint8_t*) ds - *_this->image_data_ptr, (uint8_t*) de - *_this->image_data_ptr);
	if (f == NULL) {
		// write start of free chain: space after last file
		IMAGE_PUT_WORD(de + 0, RT11_FILE_EMPTY);
		// after INIT free space has the name " EMPTY.FIL"
		IMAGE_PUT_WORD(de + 1, rad50_encode(" EM"));
		IMAGE_PUT_WORD(de + 2, rad50_encode("PTY"));
		IMAGE_PUT_WORD(de + 3, rad50_encode("FIL"));
		IMAGE_PUT_WORD(de + 4, _this->free_blocks); // block count
		IMAGE_PUT_WORD(de + 5, 0); // job/channel
		IMAGE_PUT_WORD(de + 6, 0); // INIT sets a creation date ... don't need to!
	} else {
		// regular file
		// status
		w = RT11_FILE_EPERM;
		if (f->readonly)
			w |= RT11_FILE_EREAD | RT11_FILE_EPROT;
		if (f->prefix)
			w |= RT11_FILE_EPRE;
		IMAGE_PUT_WORD(de + 0, w);

		// filename chars 0..2
		strncpy(buff, f->filnam, 3);
		buff[3] = 0;
		IMAGE_PUT_WORD(de + 1, rad50_encode(buff));

		// filename chars 3..5
		if (strlen(f->filnam) < 4)
			buff[0] = 0;
		else
			strncpy(buff, f->filnam + 3, 3);
		buff[3] = 0;
		IMAGE_PUT_WORD(de + 2, rad50_encode(buff));
		// ext
		IMAGE_PUT_WORD(de + 3, rad50_encode(f->ext));
		// total file len
		IMAGE_PUT_WORD(de + 4, f->block_count);
		// clr job/channel
		IMAGE_PUT_WORD(de + 5, 0);
		//date. do not set "age", as it is not evaluated by DEC software.
		// year already in range 1972..1999
		w = f->date.tm_year - 72;
		w |= f->date.tm_mday << 5;
		w |= (f->date.tm_mon + 1) << 10;
		IMAGE_PUT_WORD(de + 6, w);
		if (f->dir_ext) {
			// write bytes from "dir extension" stream into directory entry
			if (f->dir_ext->data_size > _this->dir_entry_extra_bytes)
				return error_set(ERROR_FILESYSTEM_OVERFLOW,
						"render_directory(): file %s dir_ext size %d > extra bytes in dir %d\n",
						f->filnam, f->dir_ext->data_size, _this->dir_entry_extra_bytes);
			memcpy(de + 7, f->dir_ext->data, f->dir_ext->data_size);
		}
	}
	// write end-of-segment marker behind dir_entry.
	IMAGE_PUT_WORD(de + dir_entry_word_count, RT11_DIR_EEOS);
	// this is overwritten by next entry; and remains if last entry in segment
	return ERROR_OK;
}

// Pre: all files are arrange as gap-less stream, with only empty segment
// after last file.
static int render_directory(rt11_filesystem_t *_this) {
	int i;
	int dir_entries_per_segment = rt11_dir_entries_per_segment(_this); // cache
	int ds_nr; // # of segment
	int de_nr; // # of entry
	for (i = 0; i < _this->file_count; i++) {
		rt11_file_t *f = _this->file[i];
		// which segment?
		ds_nr = (i / dir_entries_per_segment) + 1; // runs from 1
		// which entry in the segment?
		de_nr = i % dir_entries_per_segment; // runs from 0

		render_directory_entry(_this, f, ds_nr, de_nr);
	}
	// last entry: start of empty free chain
	ds_nr = _this->file_count / dir_entries_per_segment + 1;
	de_nr = _this->file_count % dir_entries_per_segment;
	render_directory_entry(_this, NULL, ds_nr, de_nr);

	return ERROR_OK;
}

// write file data into image
static void render_file_data(rt11_filesystem_t *_this) {
	int i;
	for (i = 0; i < _this->file_count; i++) {
		rt11_file_t *f = _this->file[i];
		if (f->prefix) { 		// prefix block?
			// low byte of 1st word on volume is blockcount,
			uint16_t prefix_block_count = NEEDED_BLOCKS(RT11_BLOCKSIZE,
					f->prefix->data_size + 2);
			if (prefix_block_count > 255) {
				fprintf(stderr, "Render: Prefix of file \"%s.%s\" = %d blocks, maximum 255\n",
						f->filnam, f->ext, prefix_block_count);
				exit(1);
			}

			IMAGE_PUT_WORD(IMAGE_BLOCKNR2PTR(_this,f->prefix->blocknr), prefix_block_count);
			// start block and byte offset 2 already set by layout()
			stream_render(_this, f->prefix);
		}
		if (f->data)
			stream_render(_this, f->data);
	}
}

// write filesystem into image
// Assumes all file data and blocklists are valid
// return: 0 = OK
int rt11_filesystem_render(rt11_filesystem_t *_this) {

	// format media, all 0's
	memset(_this->image_data, 0, _this->image_size);

	if (rt11_filesystem_layout(_this))
		return error_code; // oversized

	if (_this->bootblock->data_size)
		stream_render(_this, _this->bootblock);
#ifdef DEFAULTBOOTLOADER
	else
	// volume not bootable
	stream_render(_this, _this->nobootblock);
#endif
	if (_this->monitor->data_size)
		stream_render(_this, _this->monitor);
	render_homeblock(_this);
	if (render_directory(_this))
		return error_code;
	render_file_data(_this);

	return ERROR_OK;
}

/**************************************************************
 * FileAPI
 * add / get files in logical data structure
 **************************************************************/

// fill the pseudo file with textual volume information
static void rt11_filesystem_render_volumeinfo(rt11_filesystem_t *_this, rt11_file_t *f) {
	// static stream instance as buffer for name=value text
	static rt11_stream_t stream_buffer;
	static uint8_t text_buffer[4096 + RT11_MAX_FILES_PER_IMAGE * 80]; // listing for all files
	char line[1024];
	int i;
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);

	text_buffer[0] = 0;
	sprintf(line, "# %s.%s - info about RT-11 volume on %s device.\n", f->filnam, f->ext,
			_this->device_info->device_name);
	strcat(text_buffer, line);
	sprintf(line, "# Produced by TU58FS at %d-%d-%d %d:%d:%d\n", tm.tm_year + 1900,
			tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	strcat(text_buffer, line);
	sprintf(line, "\npack_cluster_size=%d\n", _this->pack_cluster_size);
	strcat(text_buffer, line);

	sprintf(line, "\n# Block number of first directory segment\nfirst_dir_blocknr=%d\n",
			_this->first_dir_blocknr);
	strcat(text_buffer, line);

	sprintf(line, "\nsystem_version=%s\n", _this->system_version);
	strcat(text_buffer, line);

	sprintf(line, "\nvolume_id=%s\n", _this->volume_id);
	strcat(text_buffer, line);

	sprintf(line, "\nowner_name=%s\n", _this->owner_name);
	strcat(text_buffer, line);

	sprintf(line, "\nsystem_id=%s\n", _this->system_id);
	strcat(text_buffer, line);

	sprintf(line, "\n# number of %d byte blocks on volume\nblock_count=%d\n",
	RT11_BLOCKSIZE, _this->blockcount);
	strcat(text_buffer, line);

	sprintf(line, "\n# number of extra bytes per directory entry\ndir_entry_extra_bytes=%d\n",
			_this->dir_entry_extra_bytes);
	strcat(text_buffer, line);

	sprintf(line, "\n# Total number of segments in this directory (can hold %d files) \n"
			"dir_total_seg_num=%d\n",
			rt11_dir_entries_per_segment(_this) * _this->dir_total_seg_num,
			_this->dir_total_seg_num);
	strcat(text_buffer, line);

	sprintf(line, "\n# Number of highest dir segment in use\ndir_max_seg_nr=%d\n",
			_this->dir_max_seg_nr);
	strcat(text_buffer, line);

	sprintf(line, "\n# Start block of file area = %d\n", _this->file_space_blocknr);
	strcat(text_buffer, line);

	for (i = 0; i < _this->file_count; i++) {
		rt11_file_t *f = _this->file[i];
		sprintf(line, "\n# File %2d \"%s.%s\".", i, f->filnam, f->ext);
		strcat(text_buffer, line);
		if (f->prefix) {
			sprintf(line, " Prefix %d = 0x%X bytes, start block %d @ 0x%X.",
					f->prefix->data_size, f->prefix->data_size, f->prefix->blocknr,
					f->prefix->blocknr * RT11_BLOCKSIZE);
			strcat(text_buffer, line);
		} else
			strcat(text_buffer, " No prefix.");
		if (f->data) {
			sprintf(line, " Data %d = 0x%X bytes, start block %d @ 0x%X.", f->data->data_size,
					f->data->data_size, f->data->blocknr, f->data->blocknr * RT11_BLOCKSIZE);
			strcat(text_buffer, line);
		} else
			strcat(text_buffer, " No data.");
	}
	strcat(text_buffer, "\n");

	stream_init(&stream_buffer);
	stream_buffer.data = text_buffer;
	stream_buffer.data_size = strlen(text_buffer);
	assert(stream_buffer.data_size < sizeof(text_buffer)); // buffer overun?
	f->data = &stream_buffer;
	// VOLUM INF is "changed", if home block or directories changed
	f->data->changed = _this->struct_changed;
}

// special:
// -1: bootblock
// -2 : boot monitor
// -3: volume information text file
// fname: filnam.ext
//  Optional some of the hoe block data could be hold in a textfile
// with "name=value" entries, and a name of perhaps "$META.DAT"
// This would include a "BOOT=file name entry" for the "INIT/BOOT" monitor.
// A RT-11 file may consist of three different datastreams
// 1. file data, as usual
// 2. dat in a special "prefix" block
// 3. data in extended directory entries.
// if "hostfname" ends with  RT11_FILENAME_DIREXT_extension,
//	it is interpreted to contain data for directory extension
// if "hostfname" ends with  RT11_FILENAME_PREFIX_extension,
//	it is interpreted to contain data for the "prefix" blocks
//
// result: -1  = volume overflow
int rt11_filesystem_file_stream_add(rt11_filesystem_t *_this, char *hostfname, char *streamcode,
		time_t hostfdate, mode_t hostmode, uint8_t *data, uint32_t data_size) {
	// fprintf(stderr, "rt11_filesystem_file_stream_add(%s)\n", hostfname);
	if (!strcasecmp(hostfname, RT11_VOLUMEINFO_FILNAM "." RT11_VOLUMEINFO_EXT)) {
		// evaluate parameter file ?
	} else if (!strcasecmp(hostfname, RT11_BOOTBLOCK_FILNAM "." RT11_BOOTBLOCK_EXT)) {
		if (!streamcode) { // some tests produced "$BOOT.BLK.dirext" !
			if (data_size != RT11_BLOCKSIZE)
				return error_set(ERROR_FILESYSTEM_FORMAT, "Boot block not %d bytes",
				XXDP_BLOCKSIZE);
			_this->bootblock->data_size = data_size;
			_this->bootblock->data = realloc(_this->bootblock->data, data_size);
			memcpy(_this->bootblock->data, data, data_size);
		}
	} else if (!strcasecmp(hostfname, RT11_MONITOR_FILNAM "." RT11_MONITOR_EXT)) {
		if (!streamcode) {
			if (data_size > 4 * RT11_BLOCKSIZE)
				return error_set(ERROR_FILESYSTEM_FORMAT,
						"Monitor block too big, has %d bytes, max %d", data_size,
						4 * RT11_BLOCKSIZE);
			_this->monitor->data_size = data_size;
			_this->monitor->data = realloc(_this->monitor->data, data_size);
			memcpy(_this->monitor->data, data, data_size);
		}
	} else {
		// one of 3 streams of a regular file
		//1. find file
		rt11_file_t *f;
		char filnam[40], ext[40];
		char *s;
		int i;
		rt11_stream_t **streamptr;
		// regular file
		if (_this->file_count + 1 >= RT11_MAX_FILES_PER_IMAGE)
			return error_set(ERROR_FILESYSTEM_OVERFLOW, "Too many files, only %d allowed",
			RT11_MAX_FILES_PER_IMAGE);

		// check wether a new file of "data_size" bytes would fit onto volume
		if (rt11_filesystem_calc_block_use(_this, data_size))
			return error_set(ERROR_FILESYSTEM_OVERFLOW,
					"Disk full, file \"%s\" with %d bytes too large", hostfname, data_size);

		// make filename.extension to "FILN  .E  "
		rt11_filename_from_host(hostfname, filnam, ext);

		// find file with this name. Duplicate check later
		// duplicate file name? Likely! because of trunc to six letters
		f = NULL;
		for (i = 0; i < _this->file_count; i++) {
			rt11_file_t *f1 = _this->file[i];
			if (!strcasecmp(filnam, f1->filnam) && !strcasecmp(ext, f1->ext))
				f = f1; // file exists, earlier another stream for f was written
		}
		if (!f) {
			// new file
			f = rt11_file_create();
			_this->file[_this->file_count++] = f;
			strcpy(f->filnam, filnam);
			strcpy(f->ext, ext);

			f->date = *localtime(&hostfdate);
			// only range 1972..1999 allowed
			if (f->date.tm_year < 72)
				f->date.tm_year = 72;
			else if (f->date.tm_year > 99)
				f->date.tm_year = 99;
			f->readonly = 0; // set from data stream
		}
		streamptr = NULL;
		//2. write correct stream
		if (!streamcode || strlen(streamcode) == 0) {
			streamptr = &f->data;
			// file is readonly, if data stream has no user write permission (see stat(2))
			f->readonly = !(hostmode & S_IWUSR); // set from data stream
		} else if (!strcasecmp(streamcode, RT11_STREAMNAME_DIREXT)) {
			streamptr = &f->dir_ext;
			// size of dir entry extra bytes is largest dir_ext stream
			if (data_size > _this->dir_entry_extra_bytes)
				_this->dir_entry_extra_bytes = data_size;
		} else if (!strcasecmp(streamcode, RT11_STREAMNAME_PREFIX)) {
			streamptr = &f->prefix;
		} else
			return error_set(ERROR_FILESYSTEM_FORMAT, "Illegal stream code %s", streamcode);

		// stream may not be filled before! else duplicate filename
		if (*streamptr != NULL)
			return error_set(ERROR_FILESYSTEM_DUPLICATE, "Duplicate filename/stream %s.%s %s",
					filnam, ext, streamcode);

		*streamptr = stream_create();
		if (streamcode) // else remains ""
			strcpy((*streamptr)->name, streamcode);
		(*streamptr)->data_size = data_size;
		(*streamptr)->data = malloc(data_size);
		memcpy((*streamptr)->data, data, data_size);

		// calc blocks count = prefix +data
		f->block_count = 0;
		if (f->prefix)
			f->block_count += NEEDED_BLOCKS(RT11_BLOCKSIZE, f->prefix->data_size + 2); // 2 bytes length word
		if (f->data)
			f->block_count += NEEDED_BLOCKS(RT11_BLOCKSIZE, f->data->data_size);
	} // if regular file
	return ERROR_OK;
}

// access files,and special bootblock/monitor/volumeinfo in an uniform way
// -3 = volume info, -2 = monitor, -1 = boot block
// bootblock is NULL, if empty
rt11_file_t *rt11_filesystem_file_get(rt11_filesystem_t *_this, int fileidx) {
	rt11_file_t *result = NULL;
	static rt11_file_t buff[3]; // buffer for bootblock, monitor, volinfo
	if (fileidx == -3) {
		result = &buff[0];
		strcpy(result->filnam, RT11_VOLUMEINFO_FILNAM);
		strcpy(result->ext, RT11_VOLUMEINFO_EXT);
		result->fixed = 1; // can not be deleted on shared dir
		rt11_filesystem_render_volumeinfo(_this, result);
		memset(&result->date, 0, sizeof(result->date));
	} else if (fileidx == -2
			&& !is_memset(_this->monitor->data, 0, _this->monitor->data_size)) {
		result = &buff[1];
		strcpy(result->filnam, RT11_MONITOR_FILNAM);
		strcpy(result->ext, RT11_MONITOR_EXT);
		result->data = _this->monitor;
		memset(&result->date, 0, sizeof(result->date));
	} else if (fileidx == -1
			&& !is_memset(_this->bootblock->data, 0, _this->bootblock->data_size)) {
		result = &buff[2];
		strcpy(result->filnam, RT11_BOOTBLOCK_FILNAM);
		strcpy(result->ext, RT11_BOOTBLOCK_EXT);
		result->data = _this->bootblock;
		memset(&result->date, 0, sizeof(result->date));
	} else if (fileidx >= 0 && fileidx < _this->file_count) {
		result = _this->file[fileidx];
	} else
		result = NULL;
	return result;
}

/**************************************************************
 * Display structures
 **************************************************************/

static char *rt11_date_text(struct tm t) {
	char *mon[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov",
			"Dec" };
	static char buff[80];
	sprintf(buff, "%02d-%3s-%02d", t.tm_mday, mon[t.tm_mon], t.tm_year);
	return buff;
}
// print a DIR like RT11
// RT11SJ.SYS    79P 20-Dec-85      DD    .SYS     5  20-Dec-85
// SWAP  .SYS    27  20-Dec-85      TT    .SYS     2  20-Dec-85
// DL    .SYS     4  20-Dec-85      STARTS.COM     1  20-Dec-85
// DIR   .SAV    19  20-Dec-85      DUP   .SAV    47  20-Dec-85
//  8 Files, 184 Blocks
//  320 Free blocks

static char *rt11_dir_entry_text(rt11_filesystem_t *_this, int fileidx) {
	static char buff[80];
	rt11_file_t *f = _this->file[fileidx];
	sprintf(buff, "%6s.%-3s%6d%c %s", f->filnam, f->ext, f->block_count,
			f->readonly ? 'P' : ' ', rt11_date_text(f->date));
	return buff;
}

// print a DIR like RT11
void rt11_filesystem_print_dir(rt11_filesystem_t *_this, FILE *stream) {
	int fileidx;
	char line[80];
	// no header
	line[0] = 0;
	for (fileidx = 0; fileidx < _this->file_count; fileidx++) {
		if (fileidx & 1) {
			// odd file #: right column, print
			strcat(line, "      ");
			strcat(line, rt11_dir_entry_text(_this, fileidx));
			fprintf(stream, "%s\n", line);
			line[0] = 0;
		} else {
			// even: left column
			strcpy(line, rt11_dir_entry_text(_this, fileidx));
		}
	}
	if (strlen(line)) // print pending left column
		fprintf(stream, "%s\n", line);
	fprintf(stream, " %d files, %d blocks\n", _this->file_count, _this->used_file_blocks);
	fprintf(stream, " %d Free blocks\n", _this->free_blocks);
}

void rt11_filesystem_print_diag(rt11_filesystem_t *_this, FILE *stream) {
	rt11_filesystem_print_dir(_this, stream);
}

/* convert filenames and timestamps */

// make filname.ext[.streamname]
char *rt11_filename_to_host(char *filnam, char *ext, char *streamname) {
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
	if (streamname && strlen(streamname)) {
		strcat(result, ".");
		strcat(result, streamname);
	}

	return result;
}

// result ist filnam.ext, without spaces
// "filname" and "ext" contain components WITH spaces, if != NULL
// "bla.foo.c" => "BLA.FO", "C  ", result = "BLA.FO.C"
char *rt11_filename_from_host(char *hostfname, char *filnam, char *ext) {
	static char result[80];
	char pathbuff[4096];
	char _filnam[7], _ext[4];
	char *s, *t;

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
	s = extract_extension(pathbuff, /*truncate*/1);
	t = _ext; // extract 3 char ext
	while (s && *s && (t - _ext) < 3)
		*t++ = *s++;
	*t = 0;

	// extract 6 char filnam
	s = pathbuff; // only filename
	t = _filnam;
	while (*s && (t - _filnam) < 6)
		*t++ = *s++;
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

