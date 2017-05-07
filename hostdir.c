/* hostdir.c: manage the shared directory on the host running tu58fs
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
 *  07-May-2017  JH  passes GCC warning levels -Wall -Wextra
 *  05-May-2017  JH/Peter Schranz	compile under MACOS
 *  08-Feb-2017	 JH  do not update PDP file system if readonly
 *  20-Jan-2017  JH  created
 *
 *  To sync PDP image and host directory, the current state of both sides is
 *  periodically compared with a recent snapshot of the hostdir.
 *  On each side, a file can be
 *  - undchanged since last sample
 *  - deleted (now missing)
 *  - changed
 *  - created (not in old snapshot)
 *
 *  Detection of change:
 *  on hostdir, filelen and file modification time is compared against snapshot.
 *  on PDP are no highresolution timestamps. Instead the image maintains a list of changed blocks,
 *  for each of these blocks the DOS-11 file system driver determines the changed file.
 *
 *  Sync actions: each file on each side can be in one of 4 states, resulting in
 *  4 x 4 situations.
 */

#define _HOSTDIR_C_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <dirent.h>
#include <utime.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "error.h"
#include "utils.h"
#include "filesort.h"
#include "main.h"
#include "filesystem.h"
#include "hostdir.h"  // own



#ifdef __MACH__
// access to struct stat
#define STAT_ST_MTIM(sb) (sb).st_mtimespec
#else
#define STAT_ST_MTIM(sb) (sb).st_mtim
#endif

// give the opposite of PDP resp. host
#define OTHER_SIDE(side) ((side) == side_pdp ? side_host : side_pdp)

// 1: no actual file operations
int dbg_simulate = 0;

// search a file by name,
// each PDP strem is an own file here
// if not found, create, add and set to "create"
// ONLY way to create files!
// filename
hostdir_file_t *snapshot_file_find(hostdir_snapshot_t *_this, char *pdp_filename_ext) {
	int i;
	hostdir_file_t *result = NULL;
	for (i = 0; !result && i < _this->file_count; i++) {
		if (!strcasecmp(_this->file[i].pdp_filnam_ext_stream, pdp_filename_ext))
			result = &_this->file[i]; // found
	}
	return result;
}

hostdir_file_t *snapshot_file_register(hostdir_snapshot_t *_this, char *pdp_filename_ext,
		hostdir_side_t side) {
	hostdir_file_t *result;
	result = snapshot_file_find(_this, pdp_filename_ext);
	if (result && result->state[OTHER_SIDE(side)] == fs_created) {
		// Special logic: if a file is create on both sides simultanuously
		// it is found by the 2nd side, because 1st side allocated it.
		// so do not create a 2nd time but mark as "created"
		// It ois NOT possible that the 1st side "created" and the 2nd side had the file before:
		// the snapshot shows only a synchronized state with same files on both sides.
		result->state[side] = fs_created;
	}
	if (!result) {
		assert(_this->file_count < HOSTDIR_MAX_FILES);
		result = &_this->file[_this->file_count++];
		memset(result, 0, sizeof(hostdir_file_t));
		strcpy(result->pdp_filnam_ext_stream, pdp_filename_ext);
		result->state[side] = fs_created;
		result->state[OTHER_SIDE(side)] = fs_missing;
	}
	return result;
}

// produces a state info like "deleted on host"
char *state_text(hostdir_side_t side, hostdir_file_state_t state) {
	static char buff[2][80];
	char *statetxt[] = { "unchanged", "missing", "changed", "created" };
	char *sidetxt[] = { "PDP", "host" };
	char *result = buff[side]; // static buffer for each side
	sprintf(result, "%s on %s", statetxt[state], sidetxt[side]);
	return result;
}

// if all: also unchanged files
static void snapshot_print(hostdir_snapshot_t *_this, FILE *stream, int all) {
	int i;
	UNUSED(stream) ;
	for (i = 0; i < _this->file_count; i++) {
		hostdir_file_t *f;
		f = &_this->file[i];
		if (all || f->state[side_pdp] != fs_unchanged || f->state[side_host] != fs_unchanged) {
			info("Unit %d, file %3d: %10s %s, %s.", _this->hostdir->unit,
				i, f->pdp_filnam_ext_stream,
					state_text(side_pdp, f->state[side_pdp]),
					state_text(side_host, f->state[side_host]));
		}
	}
}

static int snapshot_scan_hostdir(hostdir_t *_this) {
	int i;
	struct stat sb;
	DIR *dfd;
	struct dirent *dp;
	char pathbuff[4096];
	char file_to_delete[4096];
	hostdir_file_t *f;

	// set all files "deleted", found files are overwritten with other state
	// only deleted files remain "deleted"
	for (i = 0; i < _this->snapshot.file_count; i++)
		_this->snapshot.file[i].state[side_host] = fs_missing;

	dfd = opendir(_this->path); // error checking done, compact code
	// make list of regular files
	file_to_delete[0] = 0;
	while ((dp = readdir(dfd))) {
		sprintf(pathbuff, "%s/%s", _this->path, dp->d_name);
		// beware of . and .., not regular file
		if (stat(pathbuff, &sb))
			break;
		if (S_ISREG(sb.st_mode)) {
			// file create on both sides handled
			char *pdp_filename_ext;
			// convert to PDP conventions. Then perhaps not unique!
			pdp_filename_ext = filesystem_filename_from_host(_this->pdp_fs, dp->d_name, NULL,
			NULL);
			// Find entries with same pdp filename, but different host fname.
			// These are the cases were truncing the hostname leads double PDP name
			// Delete those hostfiles.
			f = snapshot_file_register(&_this->snapshot, pdp_filename_ext, side_host);
			if (strlen(f->hostfilename) && strcasecmp(f->hostfilename, dp->d_name)
					&& file_exists(_this->path, f->hostfilename)) {
				// duplicate PDP file with different hostnames
				fprintf(ferr,
						"Host file \"%s\" maps to duplicate PDP filename \"%s\", will be deleted\n",
						dp->d_name, pdp_filename_ext);
				strcpy(file_to_delete, pathbuff);
			} else {
				strcpy(f->hostfilename, dp->d_name);
				if (f->state[side_host] != fs_created) {
					if (f->host_len != sb.st_size || f->host_mtime != STAT_ST_MTIM(sb).tv_sec)
						f->state[side_host] = fs_changed;
					else
						f->state[side_host] = fs_unchanged;
				}
				// update to newest state
				f->host_len = sb.st_size;
				f->host_mtime = STAT_ST_MTIM(sb).tv_sec;
			}
		}
	}
	closedir(dfd);
	// delete only one hostfile per round ... in fact a whole file list should be maintained
	if (strlen(file_to_delete))
		unlink(file_to_delete);
	return ERROR_OK;
}

// register all files in the PDP file system
static int snapshot_scan_pdpimage(hostdir_t *_this) {
	int i, j;
	hostdir_file_t *f;

	// see above
	for (i = 0; i < _this->snapshot.file_count; i++)
		_this->snapshot.file[i].state[side_pdp] = fs_missing;

	// not expandle, we're only creating
	// filesystem_create(_this->fs, _this->dec_device, &_this->image_data, &_this->image_data_size,/*expandable*/0) ;

	filesystem_init(_this->pdp_fs);

	// analyse an image
	if (filesystem_parse(_this->pdp_fs))
		return error_set(error_code, "Uint %d: Scanning PDP image", _this->unit);
	// if PDP file system was created with block change map, now changed files are marked
	for (i = -FILESYSTEM_MAX_SPECIALFILE_COUNT; i < *_this->pdp_fs->file_count; i++) {
		file_t *fpdp = filesystem_file_get(_this->pdp_fs, i); // include bootblock & monitor
		// for all streams
		for (j = 0; j < FILESYSTEM_MAX_DATASTREAM_COUNT; j++)
			if (fpdp && fpdp->stream[j].valid) {
				char *fname = filesystem_filename_to_host(_this->pdp_fs, fpdp->filnam,
						fpdp->ext, fpdp->stream[j].name);
				f = snapshot_file_register(&_this->snapshot, fname, side_pdp);
				f->pdp_fileidx = i;
				f->pdp_fixed = fpdp->fixed;
				f->pdp_streamidx = j;
				if (f->state[side_pdp] != fs_created) {
					if (fpdp->stream[j].changed)
						f->state[side_pdp] = fs_changed;
					else
						f->state[side_pdp] = fs_unchanged;
				}
			}
	}
	return ERROR_OK;
}

// clear all "change" states on both sides
static int snapshot_clear_states(hostdir_t *_this) {
	int i;
	for (i = 0; i < _this->snapshot.file_count; i++) {
		hostdir_file_t *f = &_this->snapshot.file[i];
		f->state[side_pdp] = f->state[side_host] = fs_unchanged;
	}
	return ERROR_OK;
}

// init the hostdir snapshot from PDP and hostdir
static void snapshot_init(hostdir_t *_this) {
	_this->snapshot.file_count = 0;
	snapshot_scan_hostdir(_this);
	snapshot_scan_pdpimage(_this);
	snapshot_clear_states(_this);
	// now both sides "unchanged"
}

/*
 * fpath is a directory path
 * - if not exist; create dir
 * - if exists: check if no subdirs
 * 		if "wipe": delete all content
 *	 $VOLUM.INF is always deleted
 */
int hostdir_prepare(hostdir_t *_this, int wipe, int allowcreate, int *created) {
	struct stat sb;
	struct dirent *dp;
	DIR *dfd;
	int ok;
	char pathbuff[4096];
	FILE *f;

	if (created)
		*created = 0;
	if (stat(_this->path, &sb)) {
		// not found
		if (allowcreate) {
			if (mkdir(_this->path, 0777)) { // open rw for all
				error("Unit %d: Creation of directory \"%s\" failed in \"%s\"", _this->unit, _this->path,
						getcwd(pathbuff, sizeof(pathbuff)));
				return -1;
			}
			if (created)
				*created = 1;
		} else {
			error("Unit %d: Directory \"%s\" not found in \"%s\", creation forbidden", _this->unit, _this->path,
					getcwd(pathbuff, sizeof(pathbuff)));
		}
	}
	// again
	if (stat(_this->path, &sb) || !S_ISDIR(sb.st_mode))
		return error_set(ERROR_HOSTDIR, "Unit %d: \"%s\" is no directory", _this->unit, _this->path);

	// has it subdirs? iterate through
	if ((dfd = opendir(_this->path)) == NULL)
		return error_set(ERROR_HOSTDIR, "Unit %d: Can't open \"%s\"", _this->unit, _this->path);

	ok = 1;
	while (ok && (dp = readdir(dfd)) != NULL) {
		sprintf(pathbuff, "%s/%s", _this->path, dp->d_name);
		if (stat(pathbuff, &sb))
			ok = 0; // can not access something inside?
		else if (strcmp(dp->d_name, ".") && strcmp(dp->d_name, "..") && !S_ISREG(sb.st_mode))
			// only regular files and . and .. allowed
			ok = 0;
	}
	closedir(dfd);
	if (!ok)
		return error_set(ERROR_HOSTDIR, "Unit %d: Dir \"%s\" contains subdirs or strange stuff\n", _this->unit,
				_this->path);

	// can I write to it?
	sprintf(pathbuff, "%s/_tu58_file_test_", _this->path);
	f = fopen(pathbuff, "w");
	if (!f)
		ok = 0;
	if (fputs("test", f) < 0)
		ok = 0;
	if (fclose(f))
		ok = 0;
	if (remove(pathbuff))
		ok = 0;
	if (!ok)
		return error_set(ERROR_HOSTDIR, "Unit %d: Can't work with dir \"%s\"", _this->unit, _this->path);

	sprintf(pathbuff, "%s/%s", _this->path, "$VOLUM.INF");
	remove(pathbuff);

	// delete content
	if (wipe) {
		dfd = opendir(_this->path); // error checking done, compact code
		while ((dp = readdir(dfd))) {
			sprintf(pathbuff, "%s/%s", _this->path, dp->d_name);
			// beware of . and ..
			if (stat(pathbuff, &sb))
				break;
			if (S_ISREG(sb.st_mode))
				remove(pathbuff);
		}
	}

	return ERROR_OK;
}


// write file streams from filled filesystem into hostdir
// fpath must be "prepared()"
// pdp_fs must have been "parsed()"
int hostdir_from_pdp_fs(hostdir_t *_this) {
	char pathbuff[4096];
	int fileidx;
	struct utimbuf ut;
	// known filesystems have max 3 special files (RT11)
	for (fileidx = -FILESYSTEM_MAX_SPECIALFILE_COUNT; fileidx < *_this->pdp_fs->file_count;
			fileidx++) {
		int i;
		file_t *f = filesystem_file_get(_this->pdp_fs, fileidx);
		if (f)
			for (i = 0; i < FILESYSTEM_MAX_DATASTREAM_COUNT; i++)
				if (f->stream[i].valid) {
					file_stream_t *stream = &f->stream[i];
					sprintf(pathbuff, "%s/%s", _this->path,
							filesystem_filename_to_host(_this->pdp_fs, f->filnam, f->ext,
									stream->name));
					file_write(pathbuff, stream->data, stream->data_size);
					if (fileidx >= 0) {
						// regular file, not bootblock or monitor: set original file date
						ut.modtime = mktime(&f->date);
						ut.actime = mktime(&f->date);
						utime(pathbuff, &ut);
					}
				}
	}
	return ERROR_OK;
}

// add a file to the PDP filesystem
static int pdp_fs_file_add(hostdir_t *_this, filesystem_t *fs, char *fpath, char *fname) {
	char pathbuff[4096];
	struct stat sb;
	uint8_t *data;
	unsigned data_size, n;
	FILE *f;

	sprintf(pathbuff, "%s/%s", fpath, fname);
	if (stat(pathbuff, &sb))
		return error_set(ERROR_HOSTFILE, "Unit %d: Can get statistics for \"%s\"", _this->unit, pathbuff);

	f = fopen(pathbuff, "r");
	if (!f)
		return error_set(ERROR_HOSTFILE, "Unit %d: Can not open \"%s\"", _this->unit, pathbuff);
	// use stat size to allocate data buffer
	data_size = sb.st_size;
	data = malloc(data_size);
	n = fread(data, 1, data_size, f);
	fclose(f);

	if (n != data_size)
		return error_set(ERROR_HOSTFILE, "Unit %d: Read %d bytes instead of %d from \"%s\"", _this->unit, n,
				data_size, pathbuff);

	// add to filesystem
	filesystem_file_add(fs, fname, STAT_ST_MTIM(sb).tv_sec, sb.st_mode, data, data_size);

	free(data);
	return ERROR_OK;
}

// scan all files, add into filesystem in correct order
// recognizes monitor and bootblock
int hostdir_to_pdp_fs(hostdir_t *_this) {
	char pathbuff[4096];
	char *names[10000];
	int filecount;
	// load filenames
	// delete content
	struct stat sb;
	DIR *dfd;
	struct dirent *dp;
	int i;

	dfd = opendir(_this->path); // error checking done, compact code
	filecount = 0;
	// make list of regular files
	while ((dp = readdir(dfd))) {
		sprintf(pathbuff, "%s/%s", _this->path, dp->d_name);
		// beware of . and ..
		if (stat(pathbuff, &sb))
			break;
		if (S_ISREG(sb.st_mode)) {
			names[filecount++] = strdup(dp->d_name);
		}
	}

	// sort names[] according to filesystem order
	filename_sort(names, filecount, filesystem_fileorder(_this->pdp_fs), -1);

	// add all files
	filesystem_init(_this->pdp_fs);
	for (i = 0; i < filecount; i++) {
		if (pdp_fs_file_add(_this, _this->pdp_fs, _this->path, names[i]))
			return error_set(error_code, "Unit %d: Host dir to PDP filesystem", _this->unit);
	}

	return ERROR_OK;
}

// link to PDP image and directory
// PDP filesystem must have been initialized with device type, image data etc.
hostdir_t *hostdir_create(int unit, char *path, filesystem_t *pdp_fs) {
	hostdir_t *_this;
	_this = malloc(sizeof(hostdir_t));
	_this->unit = unit ;
	strcpy(_this->path, path);
	_this->pdp_fs = pdp_fs;

	_this->snapshot.hostdir = _this ;
	_this->snapshot.file_count = 0;
	return _this;
}

void hostdir_destroy(hostdir_t *_this) {
	free(_this);
}

// init image with content of existing hostdir files
static int hostdir_image_reload(hostdir_t *_this) {
	filesystem_init(_this->pdp_fs);
	hostdir_to_pdp_fs(_this); //  dir => filesystem
	filesystem_render(_this->pdp_fs); // filesystem => image
	if (opt_debug)
		filesystem_print_dir(_this->pdp_fs, ferr);
	if (opt_debug)
		filesystem_print_diag(_this->pdp_fs, ferr);

	snapshot_init(_this);
	return ERROR_OK;
}

// load all files from hostdir into image
// former content of image is lost
int hostdir_load(hostdir_t *_this, int allowcreate, int *created) {

	if (hostdir_prepare(_this, /*wipe*/0, allowcreate, created)) {
		error("Unit %d: hostdir_prepare() failed", _this->unit);
		return -1;
	}
	if (opt_verbose && *created)
		info("Unit %d: Host directory \"%s\" created", _this->unit, _this->path);

	return hostdir_image_reload(_this);
}

// convert image into files, save in hostdir
// former content of hostdir is lost
int hostdir_save(hostdir_t *_this) {
	if (hostdir_prepare(_this, /*wipe*/1, 0, NULL))
		return error_set(error_code, "Unit %d: Saving host dir", _this->unit);
	filesystem_init(_this->pdp_fs);
	filesystem_parse(_this->pdp_fs); // image => filesystem
	hostdir_from_pdp_fs(_this); // filesystem => dir
	if (opt_debug)
		filesystem_print_dir(_this->pdp_fs, ferr);
	if (opt_debug)
		filesystem_print_diag(_this->pdp_fs, ferr);
	return ERROR_OK;
}

// copy a file from pdp stream to hostdir.
// may copy several files, if PDP file has several streams
static void hostdir_file_copy_from_pdp(hostdir_t *_this, hostdir_file_t *f) {
	char pathbuff[4096];
	file_t *fpdp;
	file_stream_t *stream;
	sprintf(pathbuff, "%s/%s", _this->path, f->pdp_filnam_ext_stream);

	fpdp = filesystem_file_get(_this->pdp_fs, f->pdp_fileidx); // also boot and monitor
	stream = &fpdp->stream[f->pdp_streamidx];
	if (!dbg_simulate)
		file_write(pathbuff, stream->data, stream->data_size);
	if (opt_verbose)
		info("Unit %d: Copied file \"%s\" from PDP to shared dir.", _this->unit, pathbuff);
}

// delete a file on the hostdir
static void hostdir_file_delete(hostdir_t *_this, hostdir_file_t *f) {
	char pathbuff[4096];
	sprintf(pathbuff, "%s/%s", _this->path, f->hostfilename);
	if (!dbg_simulate)
		remove(pathbuff);
	if (opt_verbose)
		info("Unit %d: Deleted file \"%s\" on shared dir.", _this->unit, pathbuff);
}

int hostdir_sync(hostdir_t *_this) {
	// int any_file_change;
	int update_pdp;
	int update_snapshot;
	int i;
	// entry: snapshot contains files in shared dir,
	// as PDP filesystem and hostdir where synched

	// scan hostdir
	snapshot_scan_hostdir(_this);

	// scan PDP image
	snapshot_scan_pdpimage(_this);

	// print state
	if (opt_debug)
		snapshot_print(&_this->snapshot, ferr, 1);
	else if (opt_verbose)
		snapshot_print(&_this->snapshot, ferr, 0);

	update_pdp = 0;
	update_snapshot = 0;
	if (_this->pdp_fs->readonly) {
		// readonly: only sync hostdir from PDP file system
		int hostdir_changed = 0;
		for (i = 0; i < _this->snapshot.file_count; i++) {
			hostdir_file_t *f = &_this->snapshot.file[i];
			if (f->state[side_host] != fs_unchanged)
				hostdir_changed = 1;
		}
		if (hostdir_changed) {
			// short code: rebuild whole hostdir
			info("Unit %d: Device is readonly, reverting changes in shared dir \"%s\".", _this->unit, _this->path) ;
			hostdir_prepare(_this, /*wipe*/1, /*exists*/0, NULL);
			hostdir_from_pdp_fs(_this);
			update_snapshot = 1;
		}
	} else {
		// not readonly: update hostdir and PDP file system
		for (i = 0; i < _this->snapshot.file_count; i++) {
			hostdir_file_t *f = &_this->snapshot.file[i];
			// 16 cases. The cases when one side is unchanged are easy
			if (f->state[side_pdp] == fs_unchanged && f->state[side_host] == fs_unchanged) {
				// do nothing
			} else if (f->state[side_pdp] == fs_unchanged
					&& f->state[side_host] == fs_missing) {
				if (f->pdp_fixed)
					hostdir_file_copy_from_pdp(_this, f); // restore
				else
					update_pdp = 1; // update PDP from hostdir
			} else if (f->state[side_pdp] == fs_unchanged
					&& f->state[side_host] == fs_changed) {
				update_pdp = 1; // update PDP from hostdir
			} else if (f->state[side_pdp] == fs_unchanged
					&& f->state[side_host] == fs_created) {
				update_pdp = 1; // update PDP from hostdir

			} else if (f->state[side_pdp] == fs_missing
					&& f->state[side_host] == fs_unchanged) {
				hostdir_file_delete(_this, f);
				update_snapshot = 1;
			} else if (f->state[side_pdp] == fs_missing && f->state[side_host] == fs_missing) {
				// no need to update hostdir from pdp
				update_snapshot = 1; // but delete file from snapshot!
			} else if (f->state[side_pdp] == fs_missing && f->state[side_host] == fs_changed) {
				if (_this->pdp_priority)
					hostdir_file_delete(_this, f);
				else
					update_pdp = 1;
				update_snapshot = 1;
			} else if (f->state[side_pdp] == fs_missing && f->state[side_host] == fs_created) {
				// the file was created on host and is not yet on PDP
				update_pdp = 1; // force reload
			} else if (f->state[side_pdp] == fs_changed
					&& f->state[side_host] == fs_unchanged) {
				hostdir_file_copy_from_pdp(_this, f);
				update_snapshot = 1;
			} else if (f->state[side_pdp] == fs_changed && f->state[side_host] == fs_missing) {
				if (f->pdp_fixed || _this->pdp_priority)
					hostdir_file_copy_from_pdp(_this, f);
				else
					update_pdp = 1;
				update_snapshot = 1;
			} else if (f->state[side_pdp] == fs_changed && f->state[side_host] == fs_changed) {
				if (_this->pdp_priority)
					hostdir_file_copy_from_pdp(_this, f);
				else
					update_pdp = 1;
				update_snapshot = 1;
			} else if (f->state[side_pdp] == fs_changed && f->state[side_host] == fs_created) {
				if (_this->pdp_priority)
					hostdir_file_copy_from_pdp(_this, f);
				else
					update_pdp = 1;
				update_snapshot = 1;
			} else if (f->state[side_pdp] == fs_created
					&& f->state[side_host] == fs_unchanged) {
				hostdir_file_copy_from_pdp(_this, f);
				update_snapshot = 1;
			} else if (f->state[side_pdp] == fs_created && f->state[side_host] == fs_missing) {
				// new on PDP
				hostdir_file_copy_from_pdp(_this, f);
				update_snapshot = 1;
			} else if (f->state[side_pdp] == fs_created && f->state[side_host] == fs_changed) {
				if (_this->pdp_priority)
					hostdir_file_copy_from_pdp(_this, f);
				else
					update_pdp = 1;
				update_snapshot = 1;
			} else if (f->state[side_pdp] == fs_created && f->state[side_host] == fs_created) {
				if (_this->pdp_priority)
					hostdir_file_copy_from_pdp(_this, f);
				else
					update_pdp = 1;
				update_snapshot = 1;
			}
		}
	}
	if (update_pdp) {
		hostdir_file_t *f;
		// files in the host dir have changed:
		// reload the tu58 image
		hostdir_image_reload(_this);
		// send volum.inf (RT11)
		f = snapshot_file_find(&_this->snapshot, "$VOLUM.INF");
		if (f)
			hostdir_file_copy_from_pdp(_this, f);
		if (opt_verbose)
			info("Unit %d: Updated PDP image with shared dir \"%s\".", _this->unit, _this->path);
	}
	if (update_pdp || update_snapshot) {
		// if file was deleted
		snapshot_init(_this);
		if (opt_verbose)
			info("Unit %d: Scanned content of shared dir \"%s\".", _this->unit, _this->path);
	}

	return ERROR_OK;
}
