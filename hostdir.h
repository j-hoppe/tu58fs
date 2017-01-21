/* hostdir.h: manage the shared directory on the host running tu58fs
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
 */

#ifndef _HOSTDIR_H_
#define _HOSTDIR_H_

#include "xxdp.h"

#define HOSTDIR_MAX_FILES	1000
#define HOSTDIR_MAX_FILENAMELEN	40 // normally only 6.3 used

typedef enum {
	side_pdp = 0, side_host = 1
} hostdir_side_t;

typedef enum {
	fs_unchanged = 0, // numbers for text conversion
	fs_missing = 1, //
	fs_changed = 2, //
	fs_created = 3 //
} hostdir_file_state_t;

// an entry in the hostdir snapshot
typedef struct {
	// identifying: the PDP filename
	char filnam_ext[40]; // normally only 6.3 used
	char hostfilename[256] ; // also needed: the host name

	// changes on host / PDP side, idx by "side"
	hostdir_file_state_t state[2];

	off_t host_len; // sampled size on host
	time_t host_mtime; // modification time

	int pdp_fileidx; // index in pdp-filesystem
} hostdir_file_t;

// state of host dir
typedef struct {
	int file_count;
	hostdir_file_t file[HOSTDIR_MAX_FILES];
} hostdir_snapshot_t;

typedef struct {
	char path[4096];  // path to host dir

	// PDP image
	xxdp_filesystem_t *pdp_fs; // link to initialized PDP file system
	// the fs is linked to the image data buffer

	hostdir_snapshot_t snapshot;

	// collision management
	int pdp_priority ; // 1: file state in PDP image overrides hostdir changes
} hostdir_t;

hostdir_t *hostdir_create(char *path, xxdp_filesystem_t *pdp_fs) ;
void hostdir_destroy(hostdir_t *_this);

int hostdir_prepare(hostdir_t *_this, int wipe, int allowcreate, int *created);
int hostdir_from_pdp_fs(hostdir_t *_this);
int hostdir_to_pdp_fs(hostdir_t *_this);


int hostdir_load(hostdir_t *_this, int autosizing, int allowcreate, int *created) ;
int hostdir_save(hostdir_t *_this) ;
int hostdir_sync(hostdir_t *_this) ;

#endif /* _HOSTDIR_H_ */
