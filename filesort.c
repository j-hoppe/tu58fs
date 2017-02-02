/* filesort.c: sort file names by group patterns
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
 * Sorts filenames according to list of regular expressions
 * Each regex defines a group with priority.
 * Files are sorted first for group, then for name
 *
 * Example input:
 * name[] = DATE.SYS, DB.SYS, DRSSM.SYS, HELP.TXT, SETUP.BIC, START.CCC PATCH.BIC, XXDPSM.SYS, _bootblock_, _monitor_
 * group[]= _bootblock_, _monitor_, XXDPSM.SYS, DRSSM.SYS .*\.SYS START\..* .*\.BI.
 *
 * Groups after pattern match
 * name[] = DATE.SYS, DB.SYS, DRSSM.SYS, HELP.TXT, SETUP.BIC, START.CCC PATCH.BIC, XXDPSM.SYS, _bootblock_, _monitor_
 * group    4         4       3          MAX       6          5         6          2           0            1
 *
 * Sort result:
 *         _bootblock_, _monitor_, XXDPSM.SYS, DRSSM.SYS, DATE.SYS, DB.SYS, START.CCC, PATCH.BIC, SETUP.BIC, HELP.TXT
 *
 *
 * BUG: if a more specific regex comes after a more general,
 * a files matches the general at first and never the specific.
 * However, if a file matches the pattern exactly (no regex), it can be moved to the end
 *
 *  Created on: 17.01.2017
 *      Author: root
 */
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/types.h>
#include <regex.h>

#include "error.h"	// ferr
#include "utils.h"	// ferr
#include "filesort.h"	// own

// a filename with assigned group
typedef struct {
	char *name;
	int group; // matches this regex
} sort_name_entry_t;

// a compiled group rgeex
typedef struct {
	int group;
	char *pattern; // regex or constant string
	regex_t reg; // compiled
} sort_group_regex_t;

#define	NOGROUP 0xffffff // no group index, sorts to the end

// compare name entry first by group, then by name
static int name_compare(const void *n1, const void *n2) {
	sort_name_entry_t *name1 = (sort_name_entry_t *) n1;
	sort_name_entry_t *name2 = (sort_name_entry_t *) n2;
	if (name1->group < name2->group)
		return -1;
	else if (name1->group > name2->group)
		return 1;
	else
		return strcasecmp(name1->name, name2->name);

}

// *count maybe -1, then lists are NULL terminated
void filename_sort(char **name, int name_count, char **group, int group_count) {
	sort_name_entry_t *_name;
	sort_group_regex_t *_group;
	int i, j;
	int err;
	char errbuff[1024];
	int match;

	// if name/group_size undefined: search for terminating NULL
	if (name_count < 0)
		for (name_count = 0; name[name_count]; name_count++)
			;
	if (group_count < 0)
		for (group_count = 0; group[group_count]; group_count++)
			;

	// allocate internal lists. internals have prefix "_"
	_name = calloc(name_count, sizeof(sort_name_entry_t));
	_group = calloc(group_count, sizeof(sort_group_regex_t));

	// init lists with params
	for (i = 0; i < name_count; i++) {
		_name[i].name = name[i];
		_name[i].group = NOGROUP;
	}

	for (i = 0; i < group_count; i++) {
		_group[i].pattern = group[i];
		_group[i].group = i; // enumerate
	}

	// compile all regex. case is ignored
	for (i = 0; i < group_count; i++) {
		err = regcomp(&_group[i].reg, _group[i].pattern, REG_ICASE | REG_NOSUB);
		if (err) {
			regerror(err, &_group[i].reg, errbuff, sizeof(errbuff));
			fprintf(ferr, "Error compiling regex: %s", errbuff);
		}
	}

	// 1) match each name against the exact patten, no regex
	for (i = 0; i < name_count; i++)
		for (match = j = 0; !match && j < group_count; j++)
			if (!strcasecmp(_group[j].pattern, _name[i].name)) {
				_name[i].group = j;
				match = 1;
			}

	// 2) match each name against the regexes, first match defines group
	for (i = 0; i < name_count; i++)
		if (_name[i].group == NOGROUP) // refine, not reDEfine
			for (match = j = 0; !match && j < group_count; j++) {
				if (regexec(&_group[j].reg, _name[i].name, 0, NULL, 0) == 0) {
					_name[i].group = j;
					match = 1;
				}
			}

	// sort by group and name
	qsort(_name, name_count, sizeof(sort_name_entry_t), name_compare);

	// result back into input name list
	for (i = 0; i < name_count; i++)
		name[i] = _name[i].name;

	/// cleanup
	for (i = 0; i < group_count; i++)
		regfree(&_group[i].reg);
	free(_name);
	free(_group);
}
