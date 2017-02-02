/*
 *  rt11.h: Handles the RT11 filesystem on an image.
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
 *  Created on: 12.01.2017
 */
#ifndef _RT11_H_
#define _RT11_H_

#include <sys/types.h>
#include "rt11_radi.h"

#define RT11_BLOCKSIZE   512
#define RT11_MAX_BLOCKCOUNT 0x10000 // block addr only 16 bit
// no partitioned disks at the moment

#define RT11_FILE_EPRE  0000020	// dir entry status word: file has prefix block(s)
#define RT11_FILE_ETENT 0000400	// dir entry status word: tentative file
#define RT11_FILE_EMPTY 0001000	// dir entry status word: empty area
#define RT11_FILE_EPERM 0002000	// dir entry status word: permanent file
#define RT11_DIR_EEOS   0004000	// dir entry status word: end of segment marker
#define RT11_FILE_EREAD 0040000	// dir entry status word: write protect, deletion allowed
#define RT11_FILE_EPROT 0100000	// dir entry status word: protect permanent file

// own limits
#define	RT11_MAX_FILES_PER_IMAGE 1000

// pseudo file for volumn parameters
#define RT11_VOLUMEINFO_FILNAM	"$VOLUM" // valid RT11 file name
#define RT11_VOLUMEINFO_EXT	"INF"
// pseudo file for boot sector
#define RT11_BOOTBLOCK_FILNAM	"$BOOT" // valid RT11 file name "$BOOT.BLK"
#define RT11_BOOTBLOCK_EXT	"BLK"

// whatever is in blocks 2...5
#define RT11_MONITOR_FILNAM	"$MONI"
#define RT11_MONITOR_EXT	"TOR"

// mark data files with directory extension bytes and prefix blocks
// in the host filesystem with these extensions
// Example: data from host file "LOGGER.DAT.prefix" is put in the prefix block of
// file "LOGGER.DAT"
#define RT11_STREAMNAME_DIREXT	"dirext"
#define RT11_STREAMNAME_PREFIX	"prefix"


typedef uint16_t rt11_blocknr_t;

// a stream of data
// - for the bootloader on a rt11 image
// - file data, -prefixes and extra dir entries
typedef struct {
	rt11_blocknr_t blocknr; // start block
	uint32_t	byte_offset ; // offset in start block
//	rt11_blocknr_t blockcount; // count of blocks
	uint8_t *data;  // space for blockcount * BLOCKSIZE data
	uint32_t data_size; // byte count in data[]
	char name[80]; // name of stream, used as additional extension for hostfiles
	uint8_t changed; // calc'd from image_changed_blocks
} rt11_stream_t;

typedef struct {
	uint16_t	status ;
	char filnam[80];  // normally 6 chars, encoded in 2 words RADIX50. Special filenames longer
	char ext[40]; // normally 3 chars, encoded 1 word

	// position on disk from dir entry
	rt11_blocknr_t block_nr ; // start of data on volume
	rt11_blocknr_t block_count ; // total blocks on volume (prefix + data)

	rt11_stream_t *data ;
	rt11_stream_t *prefix ; // data in file prefix blocks, if any
	rt11_stream_t *dir_ext ; // extra bytes in extended directory entry, if any
	struct tm date; // file date. only y,m,d valid
	int	readonly ;
	int	fixed ; // is part of filesystem, can not be deleted
} rt11_file_t;

typedef struct {
	// link to image data and size
	// pointer reference outside locations,
	// which may be change if image-resize is necessary
	 uint8_t **image_data_ptr; // ptr to uint8_t data[]
	uint32_t *image_size_ptr; // ptr to size of data[]
	boolarray_t *image_changed_blocks; // blocks marked as "changed". may be NULL

	// the device this filesystem resides on
	device_type_t dec_device;
	device_info_t *device_info; // generic device info
	rt11_radi_t *radi ; // RT-11 specific device info

	// diskaddr_t bad_sector_file_sd ; // DEC std 144 bad sector file single density
	//diskaddr_t bad_sector_file_dd ; // dto, double density

	// data from home block
	int pack_cluster_size; // Pack cluster size (== 1). Not used?
	rt11_blocknr_t first_dir_blocknr; // Block number of first directory segment
	char system_version[4]; // System version (Radix-50 "V3A")
	char volume_id[13]; // Volumne identification, 12 ASCII chars "RT11A" + 7 spaces
	char owner_name[13]; // Owner name, 12 spaces
	char system_id[13]; // System identification "DECRT11A" + 4 spaces
	uint16_t homeblock_chksum; // checksum of the home block

	// directory layout data
	uint16_t dir_total_seg_num ; // total number of segments in this directory
	uint16_t dir_max_seg_nr ; // number of highest segment in use (only 1st segment)
	uint16_t dir_entry_extra_bytes ; // number of extra bytes per dir entry

	// layout data
	/*
	 fsNumDirSegs    = -1                # Number of directory segments (1-31)
	 fsExtraWords    = 0                 # Extra words per directory entry
	 fsDirEntSize    = 0                 # Dir entry size (words)
	 fsDirEntPerSeg  = 0                 # Maximum dir entries per segment
	 fsDataStart     = 0                 # First data block
	 fsResBlocks     = 0                 # Reserved blocks at end of filesystem
	 fsDataBlocks    = 0                 # Number of data blocks for files
	 */

	int expandable; // boolean: blockcount may be increased in _add_file()
	int blockcount; // usable blocks in filesystem.

	rt11_stream_t *nobootblock; // fix bootblock for no bootable volumes
	rt11_stream_t *bootblock; // block 0, if defined
	rt11_stream_t *monitor; // whatever is in block 2..5

	int file_count;
	rt11_file_t *file[RT11_MAX_FILES_PER_IMAGE];

	// cache directory statistics
	rt11_blocknr_t	used_file_blocks ;
	rt11_blocknr_t	free_blocks ;

	rt11_blocknr_t	file_space_blocknr ; // start of file space area
	rt11_blocknr_t	render_free_space_blocknr ; // start of free space for renderer
} rt11_filesystem_t;

#if !defined(_RT11_C_) && !defined(_RT11_RADI_C_)
extern char * rt11_fileorder[];
// extern xxdp_radi_t xxdp_radi[];
#endif

rt11_filesystem_t *rt11_filesystem_create(device_type_t dec_device, uint8_t **image_data_ptr,
		uint32_t *image_size_ptr, boolarray_t *changedblocks, int expandable);

void rt11_filesystem_destroy(rt11_filesystem_t *_this);

void rt11_filesystem_init(rt11_filesystem_t *_this);

int rt11_filesystem_parse(rt11_filesystem_t *_this);

int rt11_filesystem_file_stream_add(rt11_filesystem_t *_this, char *hostfname, char *streamcode,
		time_t hostfdate, mode_t hostmode, uint8_t *data, uint32_t data_size);

int rt11_filesystem_render(rt11_filesystem_t *_this);

rt11_file_t *rt11_filesystem_file_get(rt11_filesystem_t *_this, int fileidx);

void rt11_filesystem_print_dir(rt11_filesystem_t *_this, FILE *stream);

void rt11_filesystem_print_diag(rt11_filesystem_t *_this, FILE *stream);

char *rt11_filename_to_host(char *filnam, char *ext, char *streamname) ;

char *rt11_filename_from_host(char *hostfname, char *filnam, char *ext);

#endif /* _RT11_H_ */
