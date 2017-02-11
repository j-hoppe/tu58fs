/* filesystem.h - single interface to different DEC file systems
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
 */
#ifndef _FILESYSTEM_H_
#define _FILESYSTEM_H_

#include <sys/types.h>

#include "boolarray.h"
#include "xxdp.h"
#include "rt11.h"

// these DEC filesystems are supported
typedef enum {
	fsNONE = 0, // not known, tape is just a stream of bytes
	fsXXDP = 1,
	fsRT11 = 2
} filesystem_type_t;

// how much boot block, monitor, volume info etc?
#define FILESYSTEM_MAX_SPECIALFILE_COUNT	9

#define FILESYSTEM_MAX_DATASTREAM_COUNT	3
// XXDP: only file data
// RT11: data, prefix, directory extension


// the data of a file
typedef struct {
	int	valid ; // in use
	uint32_t data_size; // byte count in data[]
	uint8_t *data; // dynamic array with 'size' entries
	char	name[80] ; // name in shared dir
	uint8_t	changed ; // calc'd from image_changed_blocks
} file_stream_t ;

typedef struct {
	// these blocks are allocated, but no necessarily all used
	//	xxdp_blocklist_t blocklist;
	//	int contiguous; //  1: blocknumbers are numbered start, start+1, start+2, ...
	char filnam[80];  // normally 6 chars, encoded in 2 words RADIX50. Special filenames longer
	char ext[40]; // normally 3 chars, encoded 1 word
	//	xxdp_blocknr_t block_count ; // saved blockcount from UFD.

	// a "file" may have several data streams
	file_stream_t stream[FILESYSTEM_MAX_DATASTREAM_COUNT] ;

	struct tm date; // file date. only y,m,d valid
	int	fixed ; // part of PDP filesystem, can not be deleted on shared dir

} file_t;


typedef struct {
	filesystem_type_t type ;
	int	readonly ;
	xxdp_filesystem_t *xxdp ;
	rt11_filesystem_t *rt11 ;

	int	*file_count ; // virtual property

} filesystem_t ;


char *filesystem_name(filesystem_type_t type) ;

filesystem_t *filesystem_create(filesystem_type_t type, device_type_t device_type,
	int readonly, uint8_t *image_data, uint32_t image_data_size,
		boolarray_t *changedblocks) ;

void filesystem_destroy(filesystem_t *_this) ;

void filesystem_init(filesystem_t *_this) ;

// analyse an image
int filesystem_parse(filesystem_t *_this);

int filesystem_file_add(filesystem_t *_this, char *hostfname, time_t hostfdate,
		mode_t hostmode, uint8_t *data, uint32_t data_size) ;

file_t *filesystem_file_get(filesystem_t *_this, int fileidx) ;

// write filesystem into image
int filesystem_render(filesystem_t *_this);

// path file systemobjects in the image: DD.SYS on RT-11
int filesystem_patch(filesystem_t *_this);
// undo patches
int filesystem_unpatch(filesystem_t *_this);


void filesystem_print_dir(filesystem_t *_this, FILE *stream) ;
void filesystem_print_diag(filesystem_t *_this, FILE *stream) ;

char *filesystem_filename_to_host(filesystem_t *_this, char *filnam, char *ext, char *streamname) ;

char *filesystem_filename_from_host(filesystem_t *_this, char *hostfname, char *filnam, char *ext) ;

char 	**filesystem_fileorder(filesystem_t *_this) ;




#endif
