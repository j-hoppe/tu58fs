/* filesystem.c - single interface to different DEC file systems
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
#define _FILESYSTEM_C_

#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "xxdp.h"
#include "rt11.h"

#include "filesystem.h" // own

char *filesystem_name(filesystem_type_t type) {
	switch (type) {
	case fsXXDP:
		return "XXDP";
	case fsRT11:
		return "RT11";
	}
	return NULL;
}

filesystem_t *filesystem_create(filesystem_type_t type, device_type_t device_type, int readonly,
		uint8_t *image_data, uint32_t image_data_size, boolarray_t *changedblocks) {
	filesystem_t *_this;
	_this = malloc(sizeof(filesystem_t));
	_this->type = type;
	_this->readonly = readonly;
	_this->xxdp = NULL;
	_this->rt11 = NULL;

	switch (type) {
	case fsXXDP:
		_this->xxdp = xxdp_filesystem_create(device_type, image_data, image_data_size,
				changedblocks);
		_this->file_count = &(_this->xxdp->file_count);
		break;
	case fsRT11:
		_this->rt11 = rt11_filesystem_create(device_type, image_data, image_data_size,
				changedblocks);
		_this->file_count = &(_this->rt11->file_count);
		break;
	default:
		fprintf(ferr, "filesystem_create(): unknown type");
	}
	return _this;
}

void filesystem_destroy(filesystem_t *_this) {
	switch (_this->type) {
	case fsXXDP:
		xxdp_filesystem_destroy(_this->xxdp);
		break;
	case fsRT11:
		rt11_filesystem_destroy(_this->rt11);
		break;
	default:
		fprintf(ferr, "xxdp_filesystem_destroy(): unknown type");
	}
	free(_this);
}

void filesystem_init(filesystem_t *_this) {
	switch (_this->type) {
	case fsXXDP:
		return xxdp_filesystem_init(_this->xxdp);
	case fsRT11:
		return rt11_filesystem_init(_this->rt11);
	default:
		fprintf(ferr, "filesystem_init(): unknown type");
	}
}

// analyse an image
int filesystem_parse(filesystem_t *_this) {
	switch (_this->type) {
	case fsXXDP:
		return xxdp_filesystem_parse(_this->xxdp);
	case fsRT11:
		return rt11_filesystem_parse(_this->rt11);
	default:
		return error_set(ERROR_FILESYSTEM_INVALID, "Filesystem not supported");
	}
}

// take a file of the shared dir, push it to the filesystem
// a PDP file can have several streams, "streamname" is
//
// the host file is only one stream of a PDP filesystem file
int filesystem_file_add(filesystem_t *_this, char *hostfname, time_t hostfdate, mode_t hostmode,
		uint8_t *data, uint32_t data_size) {
	switch (_this->type) {
	case fsXXDP: {
		return xxdp_filesystem_file_add(_this->xxdp, hostfname, hostfdate, data, data_size);
	}
	case fsRT11: {
		char *streamname = NULL;
		char *ext = extract_extension(hostfname, 0); // only test, do not clip
		// is last extension a known streamname?
		if (ext)
			if (!strcasecmp(ext, RT11_STREAMNAME_DIREXT)
					|| !strcasecmp(ext, RT11_STREAMNAME_PREFIX))
				streamname = extract_extension(hostfname, 1); // now clip
		return rt11_filesystem_file_stream_add(_this->rt11, hostfname, streamname, hostfdate,
				hostmode, data, data_size);
	}
	default:
		return error_set(ERROR_FILESYSTEM_INVALID, "Filesystem not supported");
	}
}

// access file streams, bootblock and monitor in an uniform way
file_t *filesystem_file_get(filesystem_t *_this, int fileidx) {
	static file_t result;
	char *s;

	result.filnam[0] = 0;
	result.ext[0] = 0;
	switch (_this->type) {
	case fsXXDP: {
		xxdp_file_t *f = xxdp_filesystem_file_get(_this->xxdp, fileidx);
		if (!f)
			return NULL;
		if (f->filnam)
			strcpy(result.filnam, f->filnam);
		if (f->ext)
			strcpy(result.ext, f->ext);
		result.stream[0].data = f->data;
		result.stream[0].data_size = f->data_size;
		strcpy(result.stream[0].name, ""); // main data
		result.stream[0].changed = f->changed;
		result.stream[0].valid = 1;
		result.stream[1].valid = 0;
		result.stream[2].valid = 0;
		result.date = f->date;
		result.fixed = 0;
	}
		break;
	case fsRT11: {
		rt11_file_t *f = rt11_filesystem_file_get(_this->rt11, fileidx);
		if (!f)
			return NULL;
		result.stream[0].valid = 0;
		result.stream[1].valid = 0;
		result.stream[2].valid = 0;

		// may have 3 streams
		result.stream[0].data = f->data->data;
		result.stream[0].data_size = f->data->data_size;
		strcpy(result.stream[0].name, f->data->name);
		result.stream[0].changed = f->data->changed;
		result.stream[0].valid = 1;
		if (f->prefix) {
			result.stream[1].data = f->prefix->data;
			result.stream[1].data_size = f->prefix->data_size;
			strcpy(result.stream[1].name, f->prefix->name);
			result.stream[1].changed = f->prefix->changed;
			result.stream[1].valid = 1;
		}
		if (f->dir_ext) {
			result.stream[2].data = f->dir_ext->data;
			result.stream[2].data_size = f->dir_ext->data_size;
			strcpy(result.stream[2].name, f->dir_ext->name);
			result.stream[2].changed = f->dir_ext->changed;
			result.stream[2].valid = 1;
		}
		result.date = f->date;
		result.fixed = f->fixed;

		if (f->filnam)
			strcpy(result.filnam, f->filnam);
		if (f->ext)
			strcpy(result.ext, f->ext);
	}
		break;
	default:
		return NULL;
	}
	return &result;
}

// write filesystem into image
int filesystem_render(filesystem_t *_this) {
	switch (_this->type) {
	case fsXXDP:
		return xxdp_filesystem_render(_this->xxdp);
	case fsRT11:
		return rt11_filesystem_render(_this->rt11);
	default:
		return error_set(ERROR_FILESYSTEM_INVALID, "Filesystem not supported");
	}
}

// path file systemobjects in the image: DD.SYS on RT-11
int filesystem_patch(filesystem_t *_this) {
	switch (_this->type) {
	case fsXXDP:
		return ERROR_OK; // nothing to do
	case fsRT11:
		return rt11_filesystem_patch(_this->rt11);
	default:
		return error_set(ERROR_FILESYSTEM_INVALID, "Filesystem not supported");
	}
}

// undo patches
int filesystem_unpatch(filesystem_t *_this) {
	switch (_this->type) {
	case fsXXDP:
		return ERROR_OK; // nothing to do
	case fsRT11:
		return rt11_filesystem_unpatch(_this->rt11);
	default:
		return error_set(ERROR_FILESYSTEM_INVALID, "Filesystem not supported");
	}
}

void filesystem_print_dir(filesystem_t *_this, FILE *stream) {
	switch (_this->type) {
	case fsXXDP:
		xxdp_filesystem_print_dir(_this->xxdp, stream);
		break;
	case fsRT11:
		rt11_filesystem_print_dir(_this->rt11, stream);
		break;
	}
}

void filesystem_print_diag(filesystem_t *_this, FILE *stream) {
	switch (_this->type) {
	case fsXXDP:
		xxdp_filesystem_print_diag(_this->xxdp, stream);
		break;
	case fsRT11:
		rt11_filesystem_print_diag(_this->rt11, stream);
		break;
	}
}

char *filesystem_filename_to_host(filesystem_t *_this, char *filnam, char *ext,
		char *streamname) {
	switch (_this->type) {
	case fsXXDP:
		return xxdp_filename_to_host(filnam, ext);
	case fsRT11:
		return rt11_filename_to_host(filnam, ext, streamname);
		break;
	default:
		return NULL;
	}
}

char *filesystem_filename_from_host(filesystem_t *_this, char *hostfname, char *filnam,
		char *ext) {
	switch (_this->type) {
	case fsXXDP:
		return xxdp_filename_from_host(hostfname, filnam, ext);
	case fsRT11:
		return rt11_filename_from_host(hostfname, filnam, ext);
		break;
	default:
		return NULL;
	}
}

char **filesystem_fileorder(filesystem_t *_this) {
	switch (_this->type) {
	case fsXXDP:
		return xxdp_fileorder;
	case fsRT11:
		return rt11_fileorder;
	default:
		return NULL;
	}
}

