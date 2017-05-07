/* image.h: manage the image data of an TU58 cartridge
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
 *  07-May-2017  JH  passes GCC warning levels -Wall -Wextra
 *  20-Jan-2017  JH  created
 *
 *
 */

#ifndef _IMAGE_H_
#define _IMAGE_H_

#include <stdint.h>
#include <pthread.h>
#include <sys/stat.h>
#include "boolarray.h"
#include "device_info.h"
#include "filesystem.h"
#include "hostdir.h"

// just for bitmap of changed blocks
#define IMAGE_MAX_BLOCKS 1000000 // a 512 = > 512MB.

// image file data structure, represents a tape
typedef struct {
	int unit;	// own unit number, user tag
	pthread_mutex_t mutex;

	// if loaded from disk image
	char *host_fpath;		// file or directory name, valid while open
	int shared; // 0 = simple file image, 1 = files in host directory
	int readonly;

	int	forced_blockcount ; // internally calced

	struct stat host_fattr; // timestamps on open(), do track changes on disk
	hostdir_t	*hostdir ; // if shared
	filesystem_t *pdp_filesystem ;

	// basic geometry
	device_info_t *device_info ;
	int	blocksize ;

	int8_t open; // in use
	int8_t changed; // was written since last save()
	boolarray_t *changedblocks ;
	uint64_t changetime_ms; // time of last write in milli secs

	// memory buffer for image
	device_type_t dec_device ; // TU58
	filesystem_type_t dec_filesystem; // fsgeneric, fsxxdp, fsrt11
	uint32_t data_size; // count of allocated bytes in ->data
	uint8_t *data; // dynamic
	uint32_t seekpos; //read/write pointer, result of seek(). next unread byte
} image_t;


// image_t *tu58image_get(int32_t unit);
// image_t *image_is_open(image_t *_this);
image_t *image_create(device_type_t dec_device, int unit, int forced_data_size) ;

int image_open(image_t *_this, int shared, int readonly, int allowcreate, char *fname,
		filesystem_type_t dec_filesystem) ;
int image_lseek(image_t *_this, int offset, int whence);
int image_blockseek(image_t *_this, int32_t size, int32_t block, int32_t offset);

int image_read(image_t *_this, void *buf, int32_t count);
int image_write(image_t *_this, void *buf, int32_t count);
int image_save(image_t *_this);

int image_sync(image_t *_this);

void image_info(image_t *_this);

void image_destroy(image_t *_this);

#endif /* _IMAGE_H_ */
