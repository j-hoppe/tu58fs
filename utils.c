/* utils.c: miscellaneous helpers
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
 */
#define _UTILS_C_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "error.h"
#include "utils.h"	// own

//
// delay routine.
// !!! Do use carefully !
// !!! Neither usleep() not nano_sleep() work well under CygWin.
// !!! Granularity seems to be about 10 ms !!!
void delay_us(int32_t us) {
	struct timespec rqtp;
	int32_t sts;

	// check if any delay required
	if (us <= 0)
		return;

	// nanosleep() preferred over usleep(),
	// compute integer seconds and fraction (in nanoseconds)
	rqtp.tv_sec = us / 1000000L;
	rqtp.tv_nsec = (us % 1000000L) * 1000L;

	// if nanosleep() fails then just plain sleep()
	if ((sts = nanosleep(&rqtp, NULL)) == -1)
		sleep(rqtp.tv_sec);
}


void delay_ms(int32_t ms) {
		delay_us(1000 * ms) ;
}


// system time in milli seconds
uint64_t now_ms() {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * (uint64_t) 1000 + tv.tv_usec / 1000;
}

// system time in micro seconds
uint64_t now_us() {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * (uint64_t) 1000000 + tv.tv_usec ;
}


// simple timeout system
static uint64_t	timeout_time_us ; //target time

void timeout_set(int delta_us) {
	timeout_time_us = now_us() + delta_us ;
}

// 1 = timeout
int timeout_reached() {
	return (now_us() > timeout_time_us) ;
}


char *cur_time_text() {
	static char result[40] ;
	time_t timer;
	struct tm* tm_info;
	time(&timer);
	tm_info = localtime(&timer);
	strftime(result, 26, "%H:%M:%S", tm_info);
	return result;
}

// is memory all set with a const value?
// reverse oeprator to memset()
// size == 0: true
int is_memset(void *ptr, uint8_t val, uint32_t size) {
	for (; size; ptr++, size--)
		if (*(uint8_t *) ptr != val)
			return 0;
	return 1;
}

// are all bytes in file behind "offset" set to "val" ?
int is_fileset(char *fpath, uint8_t val, uint32_t offset) {
	int result;
	FILE *f;
	uint8_t b;
	f = fopen(fpath, "r");
	fseek(f, offset, SEEK_SET);
	result = 1;
	// inefficient byte loop
	while (result && fread(&b, sizeof(b), 1, f) == 1)
		if (b != val)
			result = 0;

	fclose(f);
	return result;
}


// write binary data into file
int file_write(char *fpath, uint8_t *data, unsigned size) {
	int fd;
	// O_TRUNC: set to length 0
	fd = open(fpath, O_CREAT | O_TRUNC | O_RDWR, 0666);
	// or f = fopen(fpath, "w") ;
	if (fd < 0)
		return error_set(ERROR_HOSTFILE, "File write: can not open \"%s\"", fpath);
	write(fd, data, size);
	close(fd);
	return ERROR_OK;
}


// read a string and separate into tokens
// return: count
int inputline(char **tokenlist, int tokenlist_size) {
	static char buffer[1024];
	int n;
	char *t;

	n = 0;
	if (fgets(buffer, sizeof(buffer), stdin)) {
		t = strtok(buffer, " \t\n");
		while (t && n < tokenlist_size - 1) {
			tokenlist[n++] = t;
			t = strtok(NULL, " \t\n");
		}
	}
	tokenlist[n] = NULL; // terminate list
	return n;
}

// return result from big static buffer
char *strtrim(char *txt) {
	static char buff[1024];
	char *s = txt; // start

	assert(strlen(txt) < sizeof(buff));

	// skip leading space
	while (*s && isspace(*s))
		s++;
	strcpy(buff, s); // buff now without leading space
	// s = last last non-white char
	s = buff + strlen(buff) - 1;
	while (s > buff && isspace(*s))
		s--;
	*(s + 1) = 0; // clip off
	return buff;
}

// pad a string right upto "len" with char "c"
char *strrpad(char *txt, int len, char c) {
	static char buff[1024];
	memset(buff, c, len); // init buffer with base pattern
	strncpy(buff, txt, strlen(txt));
	buff[len] = 0;
	return buff;
}

// string with all invisible chars in \x notation
char *strprintable(char *s, int size) {
	static char buffer[1024] ;
	char buf[10] ;
	int i ;
	char c ;
	char *wp = buffer ;
	*wp = 0 ;
	for (i= 0 ;  i < size ; i++) {
		c = s[i] ;
		// must be room for one more char expr
		assert(wp-buffer < (int)sizeof(buffer)-5) ;
		if (c == 0) {
			strcat(wp, "<NUL>") ;
			wp += 5 ;
		} else if (c == 0x07) {
			strcat(wp, "<BEL>") ;
			wp += 5 ;
		} else if (c == 0x0a) {
			strcat(wp, "<LF>") ;
			wp += 4 ;
		} else if (c == 0x0d) {
			strcat(wp, "<CR>") ;
			wp += 4 ;
		} else if (c == 0x20) {
				strcat(wp, "<SPC>") ;
				wp += 5 ;
		} else if (c <= 0x1f || c >= 0x7f) {
			sprintf(buf, "\\x%.2x", (int)c) ;
			strcat(wp, buf) ;
			wp += strlen(buf) ;
		} else
			*wp++ = c ;
		*wp=0 ;
	}
	return buffer ;
}


// encode char into a 0..39 value
// " ABCDEFGHIJKLMNOPQRSTUVWXYZ$.%0123456789"
// invalid = %
// see https://en.wikipedia.org/wiki/DEC_Radix-50#16-bit_systems
static int rad50_chr2val(int c) {
	if (c == ' ')
		return 000;
	else if (c >= 'A' && c <= 'Z')
		/// => 000001..011010
		return 1 + c - 'A';
	else if (c == '$')
		return 033;
	else if (c == '.')
		return 034;
	else if (c == '%')
		return 035;
	else if (c >= '0' && c <= '9')
		return 036 + c - '0';
	else
		return 035; // RT-11 for "invalid"
}

static int rad50_val2chr(int val) {
	if (val == 000)
		return ' ';
	else if (val >= 001 && val <= 032)
		return 'A' + val - 001;
	else if (val == 033)
		return '$';
	else if (val == 034)
		return '.';
	else if (val == 035)
		return '%';
	else if (val >= 036 && val <= 047)
		return '0' + val - 036;
	else
		return '%'; // RT-11 for "invalid"
}

// convert 3 chars in RAD-50 encoding to a string
// letters are digits in a base 40 (octal "50") number system
// highest digit = left most letter
char *rad50_decode(uint16_t w) {
	static char result[4];
	result[2] = rad50_val2chr(w % 050);
	w /= 050;
	result[1] = rad50_val2chr(w % 050);
	w /= 050;
	result[0] = rad50_val2chr(w);
	result[3] = 0;
	return result;
}

uint16_t rad50_encode(char *s) {
	uint16_t result = 0;
	int n;
	if (!s)
		return 0; // 3 spaces

	n = strlen(s);
	if (n > 0)
		result += rad50_chr2val(toupper(s[0]));
	result *= 050;
	if (n > 1)
		result += rad50_chr2val(toupper(s[1]));
	result *= 050;
	if (n > 2)
		result += rad50_chr2val(toupper(s[2]));
	return result;
}

// 1, if path/filename exists
int file_exists(char *path, char *filename) {
	char buffer[4096];
	struct stat st;
	if (path && strlen(path))
		sprintf(buffer, "%s/%s", path, filename);
	else
		strcpy(buffer, filename);
	return !stat(buffer, &st);

}

// clips off the last exension in filename
// if "truncate": file is truncated, extension is returned
char *extract_extension(char *filename, int truncate) {
	char *dotpos;
	char *s;

	// find the last "."
	dotpos = NULL;
	for (s = filename; *s; s++)
		if (*s == '.')
			dotpos = s;

	if (dotpos) {
		if (truncate)
			*dotpos = 0; // clip off "." and extension
		return dotpos + 1;
	} else
		return NULL;
}

int leapyear(int y) {
	return ((y % 4 == 0) && (y % 100 != 0)) || (y % 400 == 0);
}

int monthlen_noleap[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
int monthlen_leap[] = { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

// convert a DOS-11 data to time_t
// day = 5 bits, month= 4bits, year = 9bits
struct tm dos11date_decode(uint16_t w) {
	int *monthlen;
	struct tm result;
	int y = (w / 1000) + 1970;
	int m;
	int d = w % 1000; // starts as day of year

	memset(&result, 0, sizeof(result)); //clear

	// use correct table
	monthlen = leapyear(y) ? monthlen_leap : monthlen_noleap;

	m = 0;
	while (d > monthlen[m]) {
		d -= monthlen[m];
		m++;
	}
	result.tm_year = y - 1900;
	result.tm_mon = m; // 0..11
	result.tm_mday = d; // 1..31

	assert(w == dos11date_encode(result)); // cross check
	return result;
}

// year after allowed: trunc to this.
int dos11date_overflow_year = 1999;
uint16_t dos11date_encode(struct tm t) {
	int *monthlen;
	uint16_t result = 0;
	int doy;
	int y = 1900 + t.tm_year; // year is easy
	int m;

	monthlen = leapyear(y) ? monthlen_leap : monthlen_noleap;

	for (doy = m = 0; m < t.tm_mon; m++)
		doy += monthlen[m];

	result = doy + t.tm_mday;
	result += 1000 * (y - 1970);
	return result;
}

#define HEXDUMP_BYTESPERLINE	16
// mount hex and ascii and print
static void hexdump_put(FILE *stream, unsigned start, char *line_hexb, char* line_hexw,
		char *line_ascii) {
	// expand half filled lines
	while (strlen(line_hexb) < HEXDUMP_BYTESPERLINE * 3)
		strcat(line_hexb, " ");
	while (strlen(line_hexw) < (HEXDUMP_BYTESPERLINE / 2) * 5)
		strcat(line_hexw, " ");
	while (strlen(line_ascii) < HEXDUMP_BYTESPERLINE)
		strcat(line_ascii, " ");
	fprintf(stream, "%3x: %s   %s  %s\n", start, line_hexb, line_hexw, line_ascii);
	line_hexb[0] = 0; // clear output
	line_hexw[0] = 0;
	line_ascii[0] = 0;
}
// hex dump with info
void hexdump(FILE *stream, uint8_t *data, int size, char *fmt, ...) {
	va_list args;
	int i, startaddr;
	char line_hexb[80]; // buffer for hex bytes
	char line_hexw[80]; // buffer for hex words
	char line_ascii[40]; // buffer for ASCII chars

	if (fmt && strlen(fmt)) {
		va_start(args, fmt);
		vfprintf(stream, fmt, args);
		fprintf(stream, "\n");
		va_end(args);
	}
	line_hexb[0] = 0; // clear output
	line_hexw[0] = 0;
	line_ascii[0] = 0;
	for (startaddr = i = 0; i < size; i++) {
		char buff[80];
		char c;
		if ((i % HEXDUMP_BYTESPERLINE) == 0 && i > 0) { // dump cur line
			hexdump_put(stream, startaddr, line_hexb, line_hexw, line_ascii);
			startaddr = i; // label for next line
		} else if ((i % 8) == 0 && i > 0) { // 8er col separator
			strcat(line_hexb, " ");
		}
		// append cur byte to hex and char display line
		sprintf(buff, "%02x", (unsigned) data[i]);
		if (strlen(line_hexb))
			strcat(line_hexb, " ");
		strcat(line_hexb, buff);
		if (i % 2) { // odd: mount word. LSB first
			unsigned w = data[i];
			w = (w << 8) | data[i - 1];
			if (strlen(line_hexw))
				strcat(line_hexw, " ");
			sprintf(buff, "%04x", w);
			strcat(line_hexw, buff);
		}

		c = data[i];
		if (c < 0x20 || c >= 0x7f)
			c = '.';
		sprintf(buff, "%c", c);
		strcat(line_ascii, buff);
	}    // dump rest of lines
	if (strlen(line_hexb))
		hexdump_put(stream, startaddr, line_hexb, line_hexw, line_ascii);
}

/*
 * searches a tag value in an array of records, whose first element is an "int" sized tag.
 * end of array is marked with a 0 tag.
 * returns pointer to the found record on success.
 */
void *search_tagged_array(void *base, int element_size, int search_val) {
	uint8_t *ptr = base;
	int cur_tag;

	cur_tag = *(int *) ptr;
	while (cur_tag != search_val && cur_tag != 0) {
		ptr += element_size;
		cur_tag = *(int *) ptr;
	}
	if (cur_tag)
		return ptr;
	else
		return NULL;
}


