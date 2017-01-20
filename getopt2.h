/* getopt2.h:  advanced commandline parsing

 *
 *  Copyright (c) 2016-2017, Joerg Hoppe
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
 *  01-Feb-2016  JH      created
*/

#ifndef _GETOPT_H_
#define _GETOPT_H_


#include <stdio.h>

#define GETOPT_MAX_OPTION_DESCR	100
#define GETOPT_MAX_OPTION_ARGS	100
#define GETOPT_MAX_ERROR_LEN	256
#define GETOPT_MAX_LINELEN	256
#define GETOPT_MAX_DEFAULT_CMDLINE_LEN	1000
#define	GETOPT_MAX_CMDLINE_TOKEN	(2*GETOPT_MAX_OPTION_ARGS)

// status codes of first()/next()
#define GETOPT_STATUS_OK	1
#define GETOPT_STATUS_EOF	0			// cline / argumentlist processed
#define GETOPT_STATUS_ILLEGALOPTION	-1		// undefined -option
#define GETOPT_STATUS_MINARGCOUNT	-2		// not enough args for -option
#define GETOPT_STATUS_MAXARGCOUNT	-3	// too much args for -option
#define GETOPT_STATUS_ILLEGALARG	-4	// argument name not known
#define GETOPT_STATUS_ARGNOTSET	-5	// optional arg not specified
#define GETOPT_STATUS_ARGFORMATINT	-6	// argument has illegal format for decimal integer
#define GETOPT_STATUS_ARGFORMATHEX	-7	// argument has illegal format for hex integer

// static description of one Option
typedef struct {
	struct getopt_struct *parent ; // uplink
	char	*short_name; // code of option on command line
	char	*long_name;

	char	*fix_args[GETOPT_MAX_OPTION_ARGS + 1];  // name list of required arguments
	char	*var_args[GETOPT_MAX_OPTION_ARGS + 1];  // name list of variable arguments
	char	*fix_args_name_buff;
	char	*var_args_name_buff;
	int	fix_arg_count;
	int	max_arg_count;

	char	*default_args; // string representation of default arguments

	char *info;
	char *example_simple_cline_args;
	char *example_simple_info;
	char *example_complex_cline_args;
	char *example_complex_info;

	char	syntaxhelp[2*GETOPT_MAX_LINELEN]; // calculated like "-option arg1 args [optarg]
} getopt_option_descr_t;

// global record for parser
typedef struct getopt_struct {
	int  ignore_case; // case sensitivity
	getopt_option_descr_t	*nonoption_descr; // cline argument without "-option"
	getopt_option_descr_t	*option_descrs[GETOPT_MAX_OPTION_DESCR + 1];
	getopt_option_descr_t *cur_option; // current parsed option
	unsigned max_short_name_len ; // longest options_descrs->short_name len

	char	*cur_option_argval[GETOPT_MAX_OPTION_ARGS + 1]; // ptr to parsed args
	unsigned	cur_option_argvalcount;

	char	*curtoken; // ptr to current cleine arg, error context

	int		curerror;
	char	curerrortext[GETOPT_MAX_ERROR_LEN+1];

	// private
	char	default_cmdline_buff[GETOPT_MAX_DEFAULT_CMDLINE_LEN+1];
	int	argc;  // default cmdline + copy of user commandline
	char	*argv[GETOPT_MAX_CMDLINE_TOKEN+1];
	int	cur_cline_arg_idx; // index of next unprocessed argv[]
} getopt_t;



void getopt_init(getopt_t *_this, int ignore_case);
getopt_option_descr_t	*getopt_def(getopt_t *_this,
	char	*short_option_name,
	char	*long_option_name,
	char	*fix_args_csv,
	char	*opt_args_csv,
	char	*default_args,
	char	*info,
	char	*example_simple_cline, char *example_simple_info,
	char	*example_complex_cline, char *example_complex_info
	);
int getopt_isoption(getopt_t *_this, char *name);
int getopt_first(getopt_t *_this, int argc, char **argv);
int getopt_next(getopt_t *_this);
int getopt_arg_s(getopt_t *_this, char *argname, char *res, unsigned ressize);
int getopt_arg_i(getopt_t *_this, char *argname, int *res);
int getopt_arg_u(getopt_t *_this, char *argname, unsigned *val);
int getopt_arg_o(getopt_t *_this, char *argname, int *val);
int getopt_arg_h(getopt_t *_this, char *argname, int *val);

void getopt_help_commandline(getopt_t *_this, FILE *stream, unsigned linelen, unsigned indent);
void getopt_help_option(getopt_t *_this, FILE *stream, unsigned linelen, unsigned indent);
void getopt_help(getopt_t *_this, FILE *stream, unsigned linelen, unsigned indent, char *commandname);

#endif
