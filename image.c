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

#include "error.h"
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

image_t *image_create(device_type_t dec_device, int unit, int forced_data_size) {
	int block_count;
	image_t *_this;
	_this = malloc(sizeof(image_t));
	pthread_mutex_init(&_this->mutex, NULL);
	_this->open = 0;
	_this->changed = 0;
	_this->changedblocks = NULL;
	_this->host_fpath = NULL;
	_this->pdp_filesystem = NULL;
	_this->hostdir = NULL;
	_this->dec_filesystem = fsNONE;
	_this->dec_device = dec_device;
	_this->unit = unit;
	_this->device_info = (device_info_t*) search_tagged_array(device_info_table,
			sizeof(device_info_t), dec_device);
	assert(_this->device_info);

	// create data buffer, not yet extended
	_this->blocksize = 512; // all devices

	_this->forced_blockcount = NEEDED_BLOCKS(_this->blocksize, forced_data_size);

	if (_this->forced_blockcount) { // correct range
		if (_this->forced_blockcount < _this->device_info->block_count)
			_this->forced_blockcount = _this->device_info->block_count;
		else if (_this->forced_blockcount > _this->device_info->max_block_count)
			_this->forced_blockcount = _this->device_info->max_block_count;
		block_count = _this->forced_blockcount;
	} else
		block_count = _this->device_info->block_count;
	_this->data_size = block_count * _this->blocksize;
	_this->data = malloc(_this->data_size);
	_this->changedblocks = boolarray_create(IMAGE_MAX_BLOCKS);

	return _this;
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
	if (fd < 0)
		return error_set(ERROR_HOSTFILE, "Unit %d: image_open cannot open or create \"%s\"",
				_this->unit, _this->host_fpath);
	// get timestamps, to monitor changes
	stat(_this->host_fpath, &_this->host_fattr);

	// clear image
	memset(_this->data, 0, _this->data_size);

	if (!*filecreated) {
		// existing file
		int res;
		if (_this->host_fattr.st_size > _this->data_size) { // trunc ?
			if (!is_fileset(_this->host_fpath, 0, _this->data_size))
				fatal(
						"File \"%s\" is %d blocks, shall be trunc'd to %d blocks, non-zero data would be lost",
						_this->host_fpath, NEEDED_BLOCKS(_this->blocksize, _this->data_size),
						_this->forced_blockcount);
		}

		res = read(fd, _this->data, _this->data_size);

		// read file to memory
		if (res < 0)
			return error_set(ERROR_HOSTFILE, "Unit %d: image_open cannot read \"%s\"",
					_this->unit, _this->host_fpath);
		if (res < _this->host_fattr.st_size)
			return error_set(ERROR_HOSTFILE,
					"Unit %d: image_open cannot read %d bytes from \"%s\"", _this->unit,
					_this->host_fattr.st_size, _this->host_fpath);
		_this->changed = 0; // is in sync with disc file
	} else {
		// new file created
		// init mem. even if later file is loaded?
		switch (_this->dec_filesystem) {
		case fsNONE:
			info("unit %d: zero'd new tape on '%s'", _this->unit, _this->host_fpath);
			break;
		case fsXXDP:
		case fsRT11: {
			filesystem_t *pdp_fs;
			// render an empty filesystem into data[]
			pdp_fs = filesystem_create(_this->dec_filesystem, _this->dec_device,
					_this->readonly, _this->data, _this->data_size, NULL);
			if (filesystem_render(pdp_fs))
				return error_set(error_code, "Creating empty file system");
			filesystem_destroy(pdp_fs);
			info("unit %d: initialize %s directory on '%s'", _this->unit,
					filesystem_name(pdp_fs->type), _this->host_fpath);
		}
			break;
		}
		_this->changed = 1; // must be written
		// blocks need not be marked as "changed" because no file on image yet
	}

	// close file, TU58 works only on image
	// if "c" flag, now empty file created
	close(fd);

	return ERROR_OK;
}

// write image to file
static int image_hostfile_save(image_t *_this) {
	int32_t fd;		// file descriptor
	fd = open(_this->host_fpath, O_BINARY | O_RDWR, 0666);
	if (fd < 0)
		return error_set(ERROR_HOSTFILE, "Unit %d: image_save cannot open \"%s\"", _this->unit,
				_this->host_fpath);
	write(fd, _this->data, _this->data_size);
	close(fd);
	return 0;
}

int image_open(image_t *_this, int shared, int readonly, int allowcreate, char *fname,
		filesystem_type_t dec_filesystem) {
	int filecreated;
	// save some data
	_this->host_fpath = strdup(fname); // free'd on close
	_this->shared = shared;
	_this->readonly = readonly;
	_this->dec_filesystem = dec_filesystem;
	if (shared) {
		// make filesystem from files and allocate data
		_this->pdp_filesystem = filesystem_create(dec_filesystem, _this->dec_device,
				_this->readonly, _this->data, _this->data_size, _this->changedblocks);

		_this->hostdir = hostdir_create(_this->host_fpath, _this->pdp_filesystem);

		if (hostdir_load(_this->hostdir, allowcreate, &filecreated))
			return error_set(error_code, "Opening shared directory");
		// data and data_size may have been enlarged !
	} else {
		// also initializes new tape
		if (image_hostfile_open(_this, allowcreate, &filecreated))
			return error_set(error_code, "Opening image file");
	}
	_this->seekpos = 0;
	_this->open = 1;

	return ERROR_OK;
}

void image_info(image_t *_this) {
	// output some info...
	if (_this->shared)
		info("Unit %d %10s fmt=%s size=%dKB=%d blocks, shared dir=\"%s\"", _this->unit,
				_this->readonly ? "readonly" : "read/write",
				filesystemtext[_this->dec_filesystem], _this->data_size / 1024,
				NEEDED_BLOCKS(_this->blocksize, _this->data_size), _this->host_fpath);
	else
		info("Unit %d %10s fmt=%s size=%dKB=%d blocks, img file=\"%s\"", _this->unit,
				_this->readonly ? "readonly" : "read/write",
				filesystemtext[_this->dec_filesystem], _this->data_size / 1024,
				NEEDED_BLOCKS(_this->blocksize, _this->data_size), _this->host_fpath);
}

// like lseek(2) on files
// whence: SEEK_SET = The file offset is set to offset bytes.
// SEEK_CUR = The file offset is set to its current location plus offset bytes.
// SEEK_END = The file offset is set to the size of the file plus offset bytes.
int image_lseek(image_t *_this, int offset, int whence) {
	int newpos = _this->seekpos;
	if (!_this->open)
		return error_set(ERROR_IMAGE_MODE, "image_lseek(): closed unit %d", _this->unit);
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
		return error_set(ERROR_ILLPARAMVAL, "image_lseek(): invalid option");
	}
	_this->seekpos = newpos;
	return newpos;
}

// seek tape position with in a block
// offset = byte pos within block
int image_blockseek(image_t *_this, int32_t blocksize, int32_t blocknr, int32_t offset) {
	int result = 0;
	if (!_this->open)
		return error_set(ERROR_IMAGE_MODE, "image_blockseek(): closed unit %d", _this->unit);

	image_lock(_this);
	// change pos to end to testsize ?
	if (blocknr * blocksize + offset > image_lseek(_this, 0, SEEK_END))
		result = ERROR_IMAGE_EOF;
	else if (image_lseek(_this, blocknr * blocksize + offset, SEEK_SET) < 0)
		result = ERROR_IMAGE_EOF;
	else
		result = ERROR_OK;
	image_unlock(_this);

	return error_set(result, "Seek error within tape block");
}

// read data from image, like read(2)
int image_read(image_t *_this, void *buf, int32_t count) {
	int bytesleft;
	uint8_t *src;
	if (!_this->open)
		return error_set(ERROR_IMAGE_MODE, "image_read(): closed unit %d", _this->unit);
	image_lock(_this);

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
	if (!_this->open)
		return error_set(ERROR_IMAGE_MODE, "image_write(): closed unit %d", _this->unit);

	if (_this->readonly)
		return error_set(ERROR_IMAGE_MODE, "unit %d read only", _this->unit);

	image_lock(_this);

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
	// boolarray_print_diag(_this->changedblocks, stderr, _this->block_count, "IMAGE");
	_this->seekpos += count;

	image_unlock(_this);
	return count;
}

// write image data to disk
int image_save(image_t *_this) {
	int res;
	if (!_this->open)
		return error_set(ERROR_IMAGE_MODE, "image_save(): closed unit %d", _this->unit);

	if (_this->readonly)
		return error_set(ERROR_IMAGE_MODE, "image_save(): unit %d read only", _this->unit);
	if (opt_verbose)
		info("unit %d: saving %s image to %s\"%s\"'", _this->unit,
				_this->changed ? "changed" : "unchanged", _this->shared ? "shared " : "",
				_this->host_fpath);

	image_lock(_this);
	if (_this->shared) {
		if (hostdir_save(_this->hostdir))
			error_set(error_code, "hostdir_save failed");
	} else {
		if (image_hostfile_save(_this))
			error_set(error_code, "image_hostfile_save failed");
	}
	if (!error_code) {
		_this->changed = 0;
		boolarray_clear(_this->changedblocks);
	}
	image_unlock(_this);
	return error_code;
}

// write to disk, if unsave
// what if disk content and image has changed?
int image_sync(image_t *_this) {
	int result;
	if (_this->open) {
		if (_this->shared) {
			// merge files in the image and the shared directory
			image_lock(_this);
			hostdir_sync(_this->hostdir);
			image_unlock(_this);
		} else {
			// just save the image file
			if (_this->changed)
				result = image_save(_this);
		}
	} else
		result = ERROR_OK;
	boolarray_clear(_this->changedblocks);
	return result;
}

// no further read/write allowed.
void image_destroy(image_t *_this) {
	_this->open = 0;
	if (_this->host_fpath)
		free(_this->host_fpath);
	_this->host_fpath = NULL;
	if (_this->data)
		free(_this->data);
	_this->data = NULL;
	_this->data_size = 0;
	if (_this->shared) {
		if (_this->hostdir)
			hostdir_destroy(_this->hostdir);
		if (_this->pdp_filesystem)
			filesystem_destroy(_this->pdp_filesystem);
	}
	free(_this);
}

