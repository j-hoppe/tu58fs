/* hostdir.c: manage the shared directory on the host running tu58em
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
 *  To sync PDP image and host directory, the current state of both sides is
 *  periodically compared with a recent snapshot of the hostdir.
 *  On each side, a file can be
 *  - unchanged since last sample
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

#include "utils.h"
#include "filesort.h"
#include "main.h"
#include "xxdp.h"
#include "hostdir.h"  // own

// give the opposite of PDP resp. host
#define OTHER_SIDE(side) ((side) == side_pdp ? side_host : side_pdp)

// 1: no actual file operations
int dbg_simulate = 0;

// access files, bootblock and monitor in an uniform way
// bootbloc or monitor ar NULL, if empty
static xxdp_file_t *pdp_file_get(xxdp_filesystem_t *_this, int fileidx) {
	xxdp_file_t *result = NULL;
	static xxdp_file_t buff[2]; // buffers for monitor and bootblock
	if (fileidx == -1 && !is_memset(_this->bootblock->data, 0, _this->bootblock->data_size)) {
		result = &buff[0];
		strcpy(result->filnam, _this->bootblock->filename);
		strcpy(result->ext, "");
		result->data = _this->bootblock->data;
		result->data_size = _this->bootblock->data_size;
		memset(&result->date, 0, sizeof(result->date));
	} else if (fileidx == -2 && !is_memset(_this->monitor->data, 0, _this->monitor->data_size)) {
		result = &buff[1];
		strcpy(result->filnam, _this->monitor->filename);
		strcpy(result->ext, "");
		result->data = _this->monitor->data;
		result->data_size = _this->monitor->data_size;
		memset(&result->date, 0, sizeof(result->date));
	} else if (fileidx >= 0 && fileidx < _this->file_count) {
		result = _this->file[fileidx];
	} else
		result = NULL;
	return result;
}

// search a file by name,
// if not found, create, add and set to "create"
// ONLY way to create files!
// filename
hostdir_file_t *snapshot_locate_file(hostdir_snapshot_t *_this, char *filnam_ext,
		hostdir_side_t side) {
	int i;
	hostdir_file_t *result = NULL;
	assert(strlen(filnam_ext) < HOSTDIR_MAX_FILENAMELEN);
	for (i = 0; !result && i < _this->file_count; i++) {
		if (!strcasecmp(_this->file[i].filnam_ext, filnam_ext))
			result = &_this->file[i]; // found
	}
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
		strcpy(result->filnam_ext, filnam_ext);
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
	for (i = 0; i < _this->file_count; i++) {
		hostdir_file_t *f;
		char *msg;
		f = &_this->file[i];
		if (all || f->state[side_pdp] != fs_unchanged || f->state[side_host] != fs_unchanged) {
			fprintf(stream, "%3d: %10s %s, %s\n", i, f->filnam_ext,
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
	hostdir_file_t *f;

	// all files deleted, found files overwrite with other state
	// only deleted files remain "deleted"
	for (i = 0; i < _this->snapshot.file_count; i++)
		_this->snapshot.file[i].state[side_host] = fs_missing;

	dfd = opendir(_this->path); // error checking done, compact code
	// make list of regular files
	while (dp = readdir(dfd)) {
		sprintf(pathbuff, "%s/%s", _this->path, dp->d_name);
		// beware of . and .., not regular file
		if (stat(pathbuff, &sb))
			break;
		if (S_ISREG(sb.st_mode)) {
			// file create on both sides handled
			char *filname_ext;
			// convert to PDP convetions. Then perhaps not unique!
			filname_ext = xxdp_filename_from_host(dp->d_name, NULL, NULL);

			f = snapshot_locate_file(&_this->snapshot, filname_ext, side_host);
			strcpy(f->hostfilename, dp->d_name);
			if (f->state[side_host] != fs_created) {
				if (f->host_len != sb.st_size || f->host_mtime != sb.st_mtim.tv_sec)
					f->state[side_host] = fs_changed;
				else
					f->state[side_host] = fs_unchanged;
			}
			// update to newest state
			f->host_len = sb.st_size;
			f->host_mtime = sb.st_mtim.tv_sec;
		}
	}
	closedir(dfd);

	return 0;
}

// register all files in the PDP file system
static int snapshot_scan_pdpimage(hostdir_t *_this) {
	int i;
	hostdir_file_t *f;

	// see above
	for (i = 0; i < _this->snapshot.file_count; i++)
		_this->snapshot.file[i].state[side_pdp] = fs_missing;

	// not expandle, we're only creating
	// xxdp_filesystem_create(_this->fs, _this->dec_device, &_this->image_data, &_this->image_data_size,/*expandable*/0) ;

	xxdp_filesystem_init(_this->pdp_fs);

	// analyse an image
	if (xxdp_filesystem_parse(_this->pdp_fs)) {
		fprintf(ferr, "xxdp_filesystem_parse failed\n");
		return -1;
	}
	// if PDP file system was created with block change map, now changed files are marked
	for (i = -2; i < _this->pdp_fs->file_count; i++) {
		xxdp_file_t *fpdp = pdp_file_get(_this->pdp_fs, i); // include bootblock & monitor
		if (fpdp) {
			char *fname = xxdp_filename_to_host(fpdp->filnam, fpdp->ext);
			f = snapshot_locate_file(&_this->snapshot, fname, side_pdp);
			f->pdp_fileidx = i;
			if (f->state[side_pdp] != fs_created) {
				if (fpdp->changed)
					f->state[side_pdp] = fs_changed;
				else
					f->state[side_pdp] = fs_unchanged;
			}
		}
	}
	return 0;
}

// clear all "change" states on both sides
static int snapshot_clear_states(hostdir_t *_this) {
	int i;
	for (i = 0; i < _this->snapshot.file_count; i++) {
		hostdir_file_t *f = &_this->snapshot.file[i];
		f->state[side_pdp] = f->state[side_host] = fs_unchanged;
	}
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
 *
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
			if (mkdir(_this->path, 0666)) { // open rw for all
				error("Creation of directory \"%s\" failed in \"%s\"", _this->path,
						getcwd(pathbuff, sizeof(pathbuff)));
				return -1;
			}
			if (created)
				*created = 1;
		} else {
			error("Directory \"%s\" not found in \"%s\", creation forbidden", _this->path,
					getcwd(pathbuff, sizeof(pathbuff)));
		}
	}
	// again
	if (stat(_this->path, &sb) || !S_ISDIR(sb.st_mode)) {
		error("\"%s\" is no directory\n", _this->path);
		return -1;
	}

	// has it subdirs? iterate through
	if ((dfd = opendir(_this->path)) == NULL) {
		fprintf(ferr, "Can't open %s\n", _this->path);
		return -1;
	}
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
	if (!ok) {
		error("Dir \"%s\" contains subdirs or strange stuff\n", _this->path);
		return -1;
	}

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
	if (!ok) {
		error("Can't work with dir \"%s\"\n", _this->path);
		return -1;
	}

	// delete content
	if (wipe) {
		dfd = opendir(_this->path); // error checking done, compact code
		while (dp = readdir(dfd)) {
			sprintf(pathbuff, "%s/%s", _this->path, dp->d_name);
			// beware of . and ..
			if (stat(pathbuff, &sb))
				break;
			if (S_ISREG(sb.st_mode))
				remove(pathbuff);
		}
	}

	return 0; // OK
}

// write binary data into file
int file_write(char *fpath, uint8_t *data, unsigned size) {
	int fd;
	fd = open(fpath, O_CREAT | O_RDWR, 0666);
	if (fd < 0) {
		error("File write: cannot open '%s'", fpath);
		return -1;
	}
	write(fd, data, size);
	close(fd);
	return 0;
}

// write files from filled xxdp filesystem into hostdir
// fpath must be "prepared()"
// pdp_fs must have been "parsed()"
int hostdir_from_pdp_fs(hostdir_t *_this) {
	char pathbuff[4096];
	int i;
	struct utimbuf ut;

	sprintf(pathbuff, "%s/%s", _this->path, _this->pdp_fs->bootblock->filename);
	file_write(pathbuff, _this->pdp_fs->bootblock->data, _this->pdp_fs->bootblock->data_size);

	sprintf(pathbuff, "%s/%s", _this->path, _this->pdp_fs->monitor->filename);
	file_write(pathbuff, _this->pdp_fs->monitor->data, _this->pdp_fs->monitor->data_size);

	for (i = 0; i < _this->pdp_fs->file_count; i++) {
		xxdp_file_t *f = _this->pdp_fs->file[i];
		sprintf(pathbuff, "%s/", _this->path);
		strcat(pathbuff, strtrim(f->filnam)); // remove spaces from filnam.ext
		strcat(pathbuff, ".");
		strcat(pathbuff, strtrim(f->ext)); // remove spaces from filnam.ext
		file_write(pathbuff, f->data, f->data_size);

		// set original file date
		ut.modtime = mktime(&f->date);
		ut.actime = mktime(&f->date);
		utime(pathbuff, &ut);
	}
	return 0;
}

// add a file to the PDP filesystem
static int pdp_fs_file_add(xxdp_filesystem_t *fs, char *fpath, char *fname) {
	char pathbuff[4096];
	struct stat sb;
	uint8_t *data;
	unsigned data_size, n;
	int special;
	FILE *f;

	sprintf(pathbuff, "%s/%s", fpath, fname);
	if (stat(pathbuff, &sb)) {
		error("Can get statistics for %s\n", pathbuff);
		return -1;
	}

	f = fopen(pathbuff, "r");
	if (!f) {
		error("Can not open %s\n", pathbuff);
		return -1;
	}
	// use stat size to allocate data buffer
	data_size = sb.st_size;
	data = malloc(data_size);
	n = fread(data, 1, data_size, f);
	fclose(f);

	if (n != data_size) {
		error("Read %d bytes instead of %d from %s\n", n, data_size, pathbuff);
		return -1;
	}

	if (!strcasecmp(fname, fs->bootblock->filename))
		special = -1;
	else if (!strcasecmp(fname, fs->monitor->filename))
		special = -2;
	else
		special = 0;

	// add to filessystem
	xxdp_filesystem_add_file(fs, special, fname, sb.st_mtim.tv_sec, data, data_size);

	free(data);
	return 0;
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
	while (dp = readdir(dfd)) {
		sprintf(pathbuff, "%s/%s", _this->path, dp->d_name);
		// beware of . and ..
		if (stat(pathbuff, &sb))
			break;
		if (S_ISREG(sb.st_mode)) {
			names[filecount++] = strdup(dp->d_name);
		}
	}

	// sort names[] according to xxdp order
	filename_sort(names, filecount, xxdp_fileorder, -1);

	// add all files
	xxdp_filesystem_init(_this->pdp_fs);
	for (i = 0; i < filecount; i++) {
		if (pdp_fs_file_add(_this->pdp_fs, _this->path, names[i]))
			return -1;
	}

	return 0;
}

// link to PDP image and directory
// PDP filesystem must have been initilaiszed with devcie type, image data etc.
hostdir_t *hostdir_create(char *path, xxdp_filesystem_t *pdp_fs) {
	hostdir_t *_this;
	_this = malloc(sizeof(hostdir_t));
	strcpy(_this->path, path);
	_this->pdp_fs = pdp_fs;

	_this->snapshot.file_count = 0;
	return _this;
}

void hostdir_destroy(hostdir_t *_this) {
	free(_this);
}

// init image with content of existing hostdir files
static int hostdir_image_reload(hostdir_t *_this) {
	xxdp_filesystem_init(_this->pdp_fs);
	hostdir_to_pdp_fs(_this); //  dir => filesystem
	xxdp_filesystem_render(_this->pdp_fs); // filesystem => image
	if (opt_debug)
		xxdp_filesystem_print_dir(_this->pdp_fs, ferr);
	if (opt_debug)
		xxdp_filesystem_print_blocks(_this->pdp_fs, ferr);

	snapshot_init(_this);
	return 0;
}

// load all files from hostdir into image
// former content of image is lost
int hostdir_load(hostdir_t *_this, int autosizing, int allowcreate, int *created) {

	if (hostdir_prepare(_this, /*wipe*/0, allowcreate, created)) {
		error("hostdir_prepare failed");
		return -1;
	}
	if (opt_verbose && *created)
		info("Host directory \"%s\" created", _this->path);

	return hostdir_image_reload(_this);
}

// convert image into files, save in hostdir
// former content of hostdir is lost
int hostdir_save(hostdir_t *_this) {
	if (hostdir_prepare(_this, /*wipe*/1, 0, NULL)) {
		error("hostdir_prepare failed");
		return -1;
	}
	xxdp_filesystem_init(_this->pdp_fs);
	xxdp_filesystem_parse(_this->pdp_fs); // image => filesystem
	hostdir_from_pdp_fs(_this); // filesystem => dir
	if (opt_verbose)
		xxdp_filesystem_print_dir(_this->pdp_fs, ferr);
	if (opt_debug)
		xxdp_filesystem_print_blocks(_this->pdp_fs, ferr);
	return 0;
}

// copy a file from pdp to hostdir.
static void hostdir_file_copy_from_pdp(hostdir_t *_this, hostdir_file_t *f) {
	char pathbuff[4096];
	xxdp_file_t *fpdp;
	sprintf(pathbuff, "%s/%s", _this->path, f->filnam_ext);
	fpdp = pdp_file_get(_this->pdp_fs, f->pdp_fileidx); // also boot and monitor
	if (!dbg_simulate)
		file_write(pathbuff, fpdp->data, fpdp->data_size);
	if (opt_verbose)
		info("Copied file \"%s\" from PDP to hostdir", pathbuff);
}

// delete a file on the hostdir
static void hostdir_file_delete(hostdir_t *_this, hostdir_file_t *f) {
	char pathbuff[4096];
	sprintf(pathbuff, "%s/%s", _this->path, f->hostfilename);
	if (!dbg_simulate)
		remove(pathbuff);
	if (opt_verbose)
		info("Deleted file \"%s\" on hostdir", pathbuff);
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
	for (i = 0; i < _this->snapshot.file_count; i++) {
		hostdir_file_t *f = &_this->snapshot.file[i];
		// 16 cases. The cases when one side is unchanged are easy
		if (f->state[side_pdp] == fs_unchanged && f->state[side_host] == fs_unchanged) {
			// do nothing
		} else if (f->state[side_pdp] == fs_unchanged && f->state[side_host] != fs_unchanged) {
			update_pdp = 1; // update PDP from hostdir
		} else if (f->state[side_pdp] == fs_missing && f->state[side_host] == fs_unchanged) {
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
		} else if (f->state[side_pdp] == fs_changed && f->state[side_host] == fs_unchanged) {
			hostdir_file_copy_from_pdp(_this, f);
			update_snapshot = 1;
		} else if (f->state[side_pdp] == fs_changed && f->state[side_host] == fs_missing) {
			if (_this->pdp_priority)
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
		} else if (f->state[side_pdp] == fs_created && f->state[side_host] == fs_unchanged) {
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
	if (update_pdp) {
		// files in the host dir have changed:
		// reload the tu58 image
		if (opt_verbose)
			info("Updating image with \"%s\"", _this->path);
		hostdir_image_reload(_this);
	}
	if (update_pdp || update_snapshot) {
		// if file was deleted
		snapshot_init(_this);
		if (opt_verbose)
			info("Loading content of \"%s\"", _this->path);
	}

	return 0;
}
