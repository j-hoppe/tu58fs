/* image.c: manage the image data of an TU58 cartridge
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
 *  Image is load from disk on open(),
 *  manipulated in memory, and written back to disk only on demand.
 *
 *  Image is the core object.
 *
 *
 *
 *
 *                           +-------------+    open    +-----------+
 *                           |    image    |    sync    |           |
 *                           |             |    save    | host file |
 *                           |             | <--------> |           |
 *                           |             |            +-----------+
 *                           |             |
 *                           |             |             if "shared"
 *    +-----------+          |             |    load    +-----------+
 *    |           |          |             |    sync    |           |
 *    | TU58drive | <------> |             |    save    | host dir  |
 *    |           |          |             | <--------> |           |
 *    +-----------+          |             |            +-----------+
 *                           |             |                  :
 *                           |             |                  : parse, render
 *                           |             |                  :
 *                           |             |            +-----------+
 *                           |             |   data[]   | PDP-11    |
 *                           |             |  reference | filesystem|
 *                           |             | <--------> |           |
 *                           +-------------+            +-----------+
 *
 *
 */

#define _IMAGE_C_

#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <assert.h>

#include "utils.h"
#include "boolarray.h"
#include "main.h"
#include "hostdir.h"
#include "xxdp.h"
#include "rt11.h"

#include "image.h"	// own

#ifndef O_BINARY
#define O_BINARY 0		// for linux compatibility
#endif

// verify with filesystem_t
char *filesystemtext[3] = { "none", "XXDP", "RT11" };

// get next higher block count needed for "count" bytes
static int size2blocks(image_t *_this, int count) {
	_this->blocksize;
	int result = count / _this->blocksize;
	if (count % _this->blocksize)
		result++;
	return result;
}

// increase image size,
static int image_enlarge(image_t *_this, int new_blockcount) {
	uint8_t *newdata;
	int newsize;
	if (new_blockcount >= _this->max_blockcount) {
		error("Unit %d: image_enlarge invalid blockcount %d", _this->unit, new_blockcount);
		return -1;
	}
	if (new_blockcount < _this->min_blockcount)
		new_blockcount = _this->min_blockcount;

	newsize = new_blockcount * _this->blocksize;
	if (newsize > _this->data_size) {
		// enlarge and preserve content
		if (!_this->data)
			_this->data = (uint8_t *) malloc(newsize);
		else
			_this->data = (uint8_t *) realloc(_this->data, newsize);
		_this->data_size = newsize;
		if (opt_verbose)
			info("unit %d enlarged to %d blocks = %dKB", _this->unit, new_blockcount,
					_this->data_size / 1024);
	}
	return 0;
}

static void image_lock(image_t *_this) {
	pthread_mutex_lock(&_this->mutex);
}

static void image_unlock(image_t *_this) {
	pthread_mutex_unlock(&_this->mutex);
}

// opens image file or creates it
static int image_hostfile_open(image_t *_this, int allowcreate, int *filecreated) {
	int32_t fd;		// file descriptor
	// open binary host file
	int blockcount;

	*filecreated = 0;

	if (!_this->readonly) // check writability here.
		fd = open(_this->host_fpath, O_BINARY | O_RDWR, 0666);
	else
		fd = open(_this->host_fpath, O_BINARY | O_RDONLY);

	// create file if it does not exist
	if (fd < 0 && allowcreate) {
		fd = creat(_this->host_fpath, 0666);
		*filecreated = 1;
	}
	if (fd < 0) {
		error("unit %d: image_open cannot open or create '%s'", _this->unit, _this->host_fpath);
		return -2;
	}
	// get timestamps, to monitor changes
	stat(_this->host_fpath, &_this->host_fattr);

	// calc image size
	if (_this->autosizing && !*filecreated) {
		// round up to blocks
		blockcount = size2blocks(_this, _this->host_fattr.st_size);
		// never smaller then the physical cartridge:
		if (blockcount < _this->min_blockcount)
			blockcount = _this->min_blockcount;
	} else
		blockcount = _this->min_blockcount;
	_this->data_size = blockcount * _this->blocksize; // std
	assert(_this->data == NULL);
	_this->data = malloc(_this->data_size);
	_this->changedblocks = boolarray_create(IMAGE_MAX_BLOCKS);

	if (!*filecreated) {
		int res = read(fd, _this->data, _this->data_size);
		// read file to memory
		if (res < 0) {
			error("unit %d: image_open cannot read '%s'", _this->unit, _this->host_fpath);
			return -3;
		}
		if (res < _this->host_fattr.st_size) {
			error("unit %d: image_open cannot read %d bytes from '%s'", _this->unit,
					_this->host_fattr.st_size, _this->host_fpath);
			return -3;
		}
		_this->changed = 0; // is in sync with disc file
	}

	// close file, TU58 works only on image
	// if "c" flag, now empty file created
	close(fd);

	if (*filecreated) {
		// init mem. even if later file is loaded?
		switch (_this->dec_filesystem) {
		case fsnone:
			memset(_this->data, 0, _this->data_size);
			info("unit %d: zero'd new tape on '%s'", _this->unit, _this->host_fpath);
			break;
		case fsxxdp: {
			xxdp_filesystem_t *pdp_fs;
			// render an empty filesystem into data[]
			pdp_fs = xxdp_filesystem_create(_this->dec_device, &_this->data, &_this->data_size,
			NULL, 1);
			if (xxdp_filesystem_render(pdp_fs)) {
				error("Creating empty filesystem failed");
				return -1;
			}
			xxdp_filesystem_destroy(pdp_fs);
			// xxdp_init(_this);
		}
			info("unit %d: initialize XXDP directory on '%s'", _this->unit, _this->host_fpath);
			break;
		case fsrt11:
			rt11_init(_this);
			info("unit %d: initialize RT-11 directory on '%s'", _this->unit, _this->host_fpath);
			break;
		}
		_this->changed = 1; // must be written
		// blocks need not be marked as "changed" because no file on image yet
	}
	return 0;
}

// write image to file
static int image_hostfile_save(image_t *_this) {
	int32_t fd;		// file descriptor
	fd = open(_this->host_fpath, O_BINARY | O_RDWR, 0666);
	if (fd < 0) {
		error("unit %d: image_save cannot open '%s'", _this->unit, _this->host_fpath);
		return -2;
	}
	write(fd, _this->data, _this->data_size);
	close(fd);
}

void image_init(image_t *_this) {
	pthread_mutex_init(&_this->mutex, NULL);
	_this->open = 0;
	_this->changed = 0;
	_this->changedblocks = NULL;
	_this->host_fpath = NULL;
	_this->pdp_filesystem = NULL;
	_this->hostdir = NULL;
	_this->dec_filesystem = fsnone;
	_this->data = NULL;
	_this->data_size = 0;
	_this->blocksize = 512;
	// must be set before use
	_this->min_blockcount = 0;
	_this->max_blockcount = 0;
}

/*
 // test unit # and return pointer. Fail if image not open
 image_t *image_is_open(image_t *_this) {
 if (!_this->open) {
 error("closed unit %d", _this->unit);
 return NULL;
 }
 return _this;
 }
 */

int image_open(image_t *_this, int shared, int readonly, int allowcreate, char *fname,
		dec_device_t dec_device, dec_filesystem_t dec_filesystem, int autosizing) {
	int res;
	int filecreated;
	// save some data
	_this->host_fpath = strdup(fname); // free'd on close
	_this->shared = shared;
	_this->readonly = readonly;
	_this->dec_device = dec_device;
	_this->dec_filesystem = dec_filesystem;
	_this->autosizing = autosizing;
	if (shared) {
		// make filesystem from files and allocate data
		_this->pdp_filesystem = xxdp_filesystem_create(_this->dec_device, &_this->data,
				&_this->data_size, _this->changedblocks, _this->autosizing);

		_this->hostdir = hostdir_create(_this->host_fpath, _this->pdp_filesystem);

		// image size: std, may be enlarge by autosize
		_this->data_size = _this->min_blockcount * _this->blocksize; // std
		assert(_this->data == NULL);
		_this->data = malloc(_this->data_size);
		_this->changedblocks = boolarray_create(IMAGE_MAX_BLOCKS);

		if (res = hostdir_load(_this->hostdir, _this->autosizing, allowcreate, &filecreated)) {
			error("hostdir_load failed");
			return res;
		}
		// data and data_size may have been enlarged !
	} else {
		// also initializes new tape
		if (res = image_hostfile_open(_this, allowcreate, &filecreated)) {
			error("image_hostfile_open failed");
			return res;
		}
	}
	_this->seekpos = 0;
	_this->open = 1;
//	_this->changed = 0;
	//	boolarray_clear(_this->changedblocks);
	//	_this->changetime_ms = now_ms();

	return 0;
}

void image_info(image_t *_this) {
	// output some info...
	if (_this->shared)
		info("Unit %d %10s fmt=%s size=%dKB=%d blocks, shared dir=\"%s\"", _this->unit,
				_this->readonly ? "readonly" : "read/write",
				filesystemtext[_this->dec_filesystem], _this->data_size / 1024,
				_this->data_size / _this->blocksize, _this->host_fpath);
	else
		info("Unit %d %10s fmt=%s size=%dKB=%d blocks, img file=\"%s\"", _this->unit,
				_this->readonly ? "readonly" : "read/write",
				filesystemtext[_this->dec_filesystem], _this->data_size / 1024,
				_this->data_size / _this->blocksize, _this->host_fpath);
}

// like lseek(2) on files
// whence: SEEK_SET = The file offset is set to offset bytes.
// SEEK_CUR = The file offset is set to its current location plus offset bytes.
// SEEK_END = The file offset is set to the size of the file plus offset bytes.
int image_lseek(image_t *_this, int offset, int whence) {
	int newpos = _this->seekpos;
	if (!_this->open)
		return -1; // ???
	switch (whence) {
	case SEEK_SET:
		newpos = offset;
		break;
	case SEEK_CUR:
		newpos = _this->seekpos + offset;
		break;
	case SEEK_END:
		newpos = _this->data_size + offset;
		break;
	default:
		error("image_lseek(): invalid option");
		return -1;
	}
	_this->seekpos = newpos;
	return newpos;
}

// seek tape position with in a block
// offset = byte pos within block
int image_blockseek(image_t *_this, int32_t blocksize, int32_t blocknr, int32_t offset) {
	int result = 0;
	if (!_this->open) {
		error("image_blockseek(): closed unit %d", _this->unit);
		return -1;
	}

	image_lock(_this);
	// change pos to end to testsize ?
	if (blocknr * blocksize + offset > image_lseek(_this, 0, SEEK_END))
		result = -2;
	else if (image_lseek(_this, blocknr * blocksize + offset, SEEK_SET) < 0)
		result = -3;
	else
		result = 0;
	image_unlock(_this);

	return result;
}

// read data from image, like read(2)
int image_read(image_t *_this, void *buf, int32_t count) {
	int bytesleft;
	uint8_t *src;
	if (!_this->open) {
		error("image_read(): closed unit %d", _this->unit);
		return -1; // ill unit or not open
	}
	image_lock(_this);

	if (_this->autosizing) {
		int neededblocks = size2blocks(_this, _this->seekpos + count);
		if (image_enlarge(_this, neededblocks) < 0)
			error("Unit %d: can not enlarge image to %d blocks", _this->unit, neededblocks);
	}

	// read until count or end, set seekpos
	bytesleft = _this->data_size - _this->seekpos;
	if (count > bytesleft) {
		count = bytesleft;
	}
	src = _this->data + _this->seekpos;
	memcpy(buf, src, count);
	_this->seekpos += count;

	image_unlock(_this);
	return count;
}

// write data to image, like write(2)
int image_write(image_t *_this, void *buf, int32_t count) {
	int bytesleft;
	uint8_t *dest;
	uint32_t blknr;
	if (!_this->open) {
		error("image_write(): closed unit %d", _this->unit);
		return -1; // ill unit or not open
	}

	if (_this->readonly) {
		error("unit %d read only", _this->unit);
		return -1;
	}

	image_lock(_this);

	if (_this->autosizing) {
		int neededblocks = size2blocks(_this, _this->seekpos + count);
		if (image_enlarge(_this, neededblocks) < 0)
			error("Unit %d: can not enlarge image to %d blocks", _this->unit, neededblocks);
	}

	bytesleft = _this->data_size - _this->seekpos;
	if (count > bytesleft)
		count = bytesleft;

	dest = _this->data + _this->seekpos;
	memcpy(dest, buf, count);

	// set dirty
	_this->changed = 1;
	_this->changetime_ms = now_ms();
	// mark all block in range
	for (blknr = _this->seekpos / _this->blocksize;
			blknr < (_this->seekpos + count) / _this->blocksize; blknr++)
		boolarray_bit_set(_this->changedblocks, blknr);

	_this->seekpos += count;

	image_unlock(_this);
	return count;
}

// write image data to disk
int image_save(image_t *_this) {
	int res;
	if (!_this->open) {
		error("image_save(): closed unit %d", _this->unit);
		return -1; // ill unit or not open
	}

	if (_this->readonly) {
		error("unit %d read only", _this->unit);
		return -1;
	}
	if (opt_verbose)
		info("unit %d: saving %s image to %s\"%s\"'", _this->unit,
				_this->changed ? "changed" : "unchanged", _this->shared ? "shared " : "",
				_this->host_fpath);

	image_lock(_this);
	if (_this->shared) {
		if (res = hostdir_save(_this->hostdir)) {
			error("hostdir_save failed");
			return res;
		}
	} else {
		if (res = image_hostfile_save(_this)) {
			error("image_hostfile_save failed");
			return res;
		}
	}
	_this->changed = 0;
	boolarray_clear(_this->changedblocks);
	image_unlock(_this);

	return 0;
}

// write to disk, if unsave
// what if disk content and image has changed?
int image_sync(image_t *_this) {
	if (_this->open) {
		if (_this->shared) {
			// merge files in the image and the shared directory
			image_lock(_this) ;
			hostdir_sync(_this->hostdir);
			image_unlock(_this) ;
		} else {
			// just save the image file
			if (_this->changed)
				return image_save(_this);
		}
	} else
		return 0;
}

// no further read/write allowed.
void image_close(image_t *_this) {
	_this->open = 0;
	if (_this->host_fpath)
		free(_this->host_fpath);
	_this->host_fpath = NULL;
	if (_this->data)
		free(_this->data);
	_this->data = NULL;
	_this->data_size = 0;
	boolarray_destroy(_this->changedblocks);
	_this->changedblocks = NULL;
	if (_this->shared) {
		hostdir_destroy(_this->hostdir);
		xxdp_filesystem_destroy(_this->pdp_filesystem);
	}
}

