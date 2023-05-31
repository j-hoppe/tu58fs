/* main.c: tu58fs user interface, global resources
 *
 * TU58 emulator for serial port, with file sharing to host
 *
 * Copyright (c) 2017, Joerg Hoppe, j_hoppe@t-online.de, www.retrocmp.com
 * partly code from Donald N North <ak6dn_at_mindspring_dot_com>
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
 *  17-May-2017 JH  V 1.3.0     new option "--usbdelay" for "--boot"
 *  07-May-2017 JH  V 1.2.1	    passes GCC warning levels -Wall -Wextra
 *  23-Mar-2017 JH  V 1.2.0     --boot option
 *  11-Feb-2017 JH  V 1.1.0 	oversized images for rt11 and xxdp
 *  8-Feb-2017 	JH  V 1.0.1 	protect readonly image.
 *  6-Feb-2017 	JH  V 1.0.0		releases for Ubuntu/BBB/RPI,Cygwin tested & published
 *  5-Feb-2017 	JH  V 0.5 		RT-11 working
 * 12-Jan-2017  JH  V 0.1   	Edit start
 *
 */

#define _MAIN_C_

#define VERSION	"v1.3.0"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <stdarg.h>
#include <strings.h>
#include <pthread.h>

#include "error.h"
#include "utils.h"
#include "getopt2.h"
#include "serial.h"
#include "hostdir.h"
#include "image.h"
#include "tu58.h"
#include "tu58drive.h"

#include "filesystem.h"

#include "monitor.h"
#include "bootloader.h"

#include "main.h"   // own

static char copyright[] = "(C) 2017 Joerg Hoppe <j_hoppe@t-online.de>,\n"
		"(C) 2005-2017 Don North <ak6dn@mindspring.com>,\n"
		"(C) 1984 Dan Ts'o <Rockefeller University>";

static char version[] = "tu58fs - DEC TU58 tape emulator with File Sharing "VERSION "\n"
"(compile "__DATE__ " " __TIME__ ")";

// global options
char opt_serial_port[256] = "";
int opt_serial_speed = 0; // line baud rate
int opt_serial_bitcount = 8; //
char opt_serial_parity = 'n'; // n, e, o
int opt_serial_stopbits = 1; // stop bits, 1 or 2
int opt_verbose = 0; // set nonzero to output more info
int opt_timing = 0; // set nonzero to add timing delays
int opt_mrspen = 0; // set nonzero to enable MRSP mode
int opt_nosync = 0; // set nonzero to skip sending INIT at restart
int opt_debug = 0; // set nonzero for debug output
int opt_vax = 0; // set to remove delays for aggressive VAX console timeouts
int opt_background = 0; // set to run in background mode (no console I/O except errors)
int opt_synctimeout_sec = 0; // save changed image to disk after so many seconds of write-inactivity
int opt_offlinetimeout_sec = 5; // disabled: TU58 waits with "offline" until so many seconds of RS232-inactivity
int opt_usbdelay = 0; // extra delay of RS232 over USB adapters

monitor_type_t opt_boot_monitor = monitor_none;
int opt_boot_address = 07000; // end of first 4k page
int opt_boot_keep = 0;

int arg_menu_linewidth = 80;

// command line args
static getopt_t getopt_parser;
static int drive_count;

/*
 * help()
 */
static char * examples[] =
		{
				"sudo ./" PROGNAME "-p /dev/ttyS2 -b 38400 -d 0 r 11XXDP.DSK\n", //
				"    Define device #0: tape image file is 11XXDP.DSK .\n", //
				"    Access to serial line device requires \"sudo\". Image is readonly.\n", //
				"    If it not exist, an error is signaled.\n",
				"\n", //
				PROGNAME " <serial params> -d 0 c 11XXDP.DSK\n", //
				"    Same. If \"11XXDP.DSK\" does not exists, an unformatted 0-filled image file is created.\n", //
				"\n", //
				PROGNAME " <serial params> -rt11 -sd 1 w tinyrt11\n", //
				"    Shared device image for device #1, \"tinyrt11\" is a directory:\n", //
				"    Image is constructed from file content, to a max size of 256kb, else error.\n", //
				"    Can be accessed with \"DD1:\"\n", //
				"\n",
				PROGNAME " <serial params> --size 10M -rt11 -d 1 w bigrt11\n", //
				"    Same, but device image is automatically enlarged to 10MBytes, max 32MB.\n", //
				"    A modified DD.SYS driver must be used on the RT11 system.\n", //
				"\n", //
				PROGNAME " <serial params> -xxdp -d 0 r 11XXDP.DSK -d 1 w data.dsk -sd 7 w shared.dir\n", //
				"    Define device #0:\n", //
				"    Standard 256kb image, loaded from file \"11XXDP.DSK\", not created.\n", //
				"    Device #1:\n", //
				"    If \"data.dsk\" does not exist, a file is create and formatted as empty XXDP\n", //
				"    If \"data.dsk\" exists, it is opened for read/write\n", //
				"    Device #7:\n", //
				"    contains the files in a sub directory \"shared.dir\" on the host.\n", //
				"    Can be accessed with \"DD0:\", \"DD1:\", \"DD7:\"\n",
				"\n", //
				PROGNAME " -xxdp --unpack 11XXDP.DSK 11xxdp.dir TU58\n", //
				"    Extracts all files from an TU58 image into a directory.\n", //
				"    If image is bootable, pseudofiles for bootloader and monitor are generated.\n", //
				"    The directory is then bootable too.\n", //
				"\n", //
				PROGNAME " <serial params> -xxdp -sd 0 w 11xxdp.dir\n", //
				"    Define device #0:\n", //
				"    Standard 256kb image, loaded from file \"11xxdp.dir\", not created.\n", //
				"    Dir is bootable, if it contains the pseudo files for monitor and boot block.\n", //
				"    A bootable dir is can be created by unpacking a bootable image file.\n", //
				"\n", //
				PROGNAME " <serial params> -rt11 -unpack RT11V53.DSK rt11v53.imgdir TU58 -sd 0 w rt11v53.imgdir\n", //
				"    Combination of exmaples before:\n", //
				"    Extract content of image into shared directory, then run TU58 emulator on that directory.\n",
				"    Dir is bootable, if the image is bootable.\n", //
				"\n", //
				PROGNAME " -p /dev/ttyS1 -b 9600 -f 7e2 --boot odt 1\n", //
				"    Deposit TU58 bootloader over serial console port into PDP-11 and try to start it.\n", //
				"    The console is configured for 7 bit, even parity and 2 stop bits.\n", //
				"    A LSI-11 with ODT monitor is assumed, boot loader is dumped into memory at\n", //
				"        address octal 7000 (at end of first 4K page).\n", //
				"    The actual TU58 emulator must have been started on a different serial port before.\n", //
				"    The boot loader will try to boot from TU58 tape.\n", //
				"    After that the terminal window remains active, so you have a primitive\n", //
				"    teletype to operate the booted PDP_11\n"
						"\n", //
				PROGNAME " -p 9 -b 115200 --usbdelay 10 --boot odt 1\n", //
				"    Boot under MS-Windows. Console connected to COM9, an USB-RS2323 adapter.\n",
				"    The console DL11 is tuned for 115200 baudrate, the USB dongle needs 10 ms extra delay (FTDI!)\n",
				NULL };

void help() {
	char **s;
	fprintf(ferr, "\n");
	fprintf(ferr, "NAME\n");
	fprintf(ferr, "\n");
	fprintf(ferr, "%s\n", version);
	fprintf(ferr, "%s\n", copyright);

	fprintf(ferr, "\n");
	fprintf(ferr, "SYNOPSIS\n");
	fprintf(ferr, "\n");
	fprintf(ferr, "Command line options are processed strictly left-to-right. \n\n");
	// getopt must be initialized to print the syntax
	getopt_help(&getopt_parser, stdout, arg_menu_linewidth, 10, PROGNAME);
	fprintf(ferr, "\n");
	fprintf(ferr, "EXAMPLES\n");
	fprintf(ferr, "\n");
	for (s = examples; *s; s++)
		fputs(*s, ferr);

	fprintf(ferr, "\n");
	fprintf(ferr, "SEE ALSO\n");
	fprintf(ferr, "\n");
	fprintf(ferr, "Online docs: www.retrocmp.com/tools/tu58fs \n");
	fprintf(ferr, "Repository: https://github.com/j-hoppe/tu58fs\n");
	fprintf(ferr, "Contact: j_hoppe@t-online.de\n");
	fprintf(ferr, "\n");
	exit(1);
}

// show error for one option
static void commandline_error() {
	fprintf(ferr, "Error while parsing commandline:\n");
	fprintf(ferr, "  %s\n", getopt_parser.curerrortext);
	exit(1);
}

// parameter wrong for currently parsed option
static void commandline_option_error(char *errtext, ...) {
	va_list args;
	fprintf(ferr, "Error while parsing commandline option:\n");
	if (errtext) {
		va_start(args, errtext);
		vfprintf(ferr, errtext, args);
		fprintf(ferr, "\nSyntax:  ");
		va_end(args);
	} else
		fprintf(ferr, "  %s\nSyntax:  ", getopt_parser.curerrortext);
	getopt_help_option(&getopt_parser, stdout, 96, 10);
	exit(1);
}

/* check whether the given device parmeter configuration
 * my cause problems.
 */

/*
 * read commandline parameters into global "param_" vars
 * result: 0 = OK, 1 = error
 */
static void parse_commandline(int argc, char **argv) {
	char buff[1024];
	int res;
	int cur_image_size = 0;
	filesystem_type_t cur_filesystem_type = fsNONE;

	// define commandline syntax
	getopt_init(&getopt_parser, /*ignore_case*/1);

//	getopt_def(&getopt_parser, NULL, NULL, "hostname", NULL, NULL, "Connect to the Blinkenlight API server on <hostname>\n"
//		"<hostname> may be numerical or ar DNS name",
//		"127.0.0.1", "connect to the server running on the same machine.",
//		"raspberrypi", "connected to a RaspberryPi with default name.");

	// !!!1 Do not define any defaults... else these will be set very time!!!

	// Commandline compatible with Don North tu58em
	getopt_parser.ignore_case = 0; // V != v
	getopt_def(&getopt_parser, "?", "help", NULL, NULL, NULL, "Print help.",
	NULL, NULL, NULL, NULL);
	getopt_def(&getopt_parser, "V", "version", NULL, NULL, NULL, "Output version string.",
	NULL, NULL, NULL, NULL);
	getopt_def(&getopt_parser, "dbg", "debug", NULL, NULL, NULL,
			"Enable debug output to console.",
			NULL, NULL, NULL, NULL);
	getopt_def(&getopt_parser, "v", "verbose", NULL, NULL, NULL,
			"Enable verbose output to terminal.",
			NULL, NULL, NULL, NULL);
	getopt_def(&getopt_parser, "m", "mrsp", NULL, NULL, NULL,
			"Disable sending INIT at initial startup.",
			NULL, NULL, NULL, NULL);
	getopt_def(&getopt_parser, "n", "nosync", NULL, NULL, NULL,
			"enable standard MRSP mode (byte-level handshake).",
			NULL, NULL, NULL, NULL);
	getopt_def(&getopt_parser, "x", "vax", NULL, NULL, NULL,
			"Remove delays for aggressive timeouts of VAX console.",
			NULL, NULL, NULL, NULL);
	getopt_def(&getopt_parser, "bk", "background", NULL, NULL, NULL,
			"Run in background mode, no console I/O except errors.",
			NULL, NULL, NULL, NULL);
	getopt_def(&getopt_parser, "t", "timing", "parameter", NULL, NULL,
			"timing 1: add timing delays to spoof diagnostic into passing.\n"
					"timing 2: add timing delays to mimic a real TU58.",
			NULL, NULL, NULL, NULL);
	getopt_def(&getopt_parser, "b", "baudrate", "baudrate", NULL, "38400",
			"Set serial line speed to 300..3000000 baud.",
			NULL, NULL, NULL, NULL);
	getopt_def(&getopt_parser, "f", "format", "bits_parity_stop", NULL, "8N1",
			"Set format parameters for serial line as a 3 char string <bitcount><parity><stopbits>\n"
					"<bitcount> maybe 7 or 8, <parity> is n (no), e (even) or o (odd), <stopbits> is 1 or 2.\n"
					"Set to special console params for --boot operation. leave default for device emulation.",
			"7e2", "Set for 7 bit even parity with 2 stop bits (--boot)", NULL, NULL);
//	getopt_def(&getopt_parser, "sb", "stopbits", "count", NULL, "1", "Set 1 or 2 stop bits.",
//	NULL, NULL, NULL, NULL);
	getopt_def(&getopt_parser, "p", "port", "serial_device", NULL, NULL,
			"Select serial port: \"COM<serial_device>:\" or <serial_device> is a node like \"/dev/ttyS1\"",
			NULL, NULL, NULL, NULL);

	getopt_def(&getopt_parser, "xx", "xxdp", NULL, NULL, NULL,
			"Select XXDP file system for following --device or --shareddevice options.\n"
					"New image files are create with an empty XXDP file system.",
			NULL, NULL, NULL, NULL);
	getopt_def(&getopt_parser, "rt", "rt11", NULL, NULL, NULL,
			"Same as --xxdp, but RT11 file system is selected.",
			NULL, NULL, NULL, NULL);

	getopt_def(&getopt_parser, "s", "size", "size", NULL, NULL,
			"Override size of TU58 imagefile. \n"
					"<size> is number of bytes; suffix \"K\": * 1024, suffix \"M\": * K * K.\n"
					"Smaller images are enlarged, greater are trunc'd if possible.\n"
					"Devices and file system try to adapt.\n"
					"Works for XXDP non-boot-tapes, and RT-11 with patched DD.SYS driver.\n"
					"\"--size std\" clears overriden size.", "10M",
			"image is 10 Megabytes = RL02 sized.",
			NULL, NULL);

	getopt_def(&getopt_parser, "d", "device", "unit,read_write_create,filename",
	NULL, NULL,
			"Open image file for a TU58 drive\n"
					"<unit>: File is mounted in this drive (0..7).\n"
					"<read_write_create>: \"r\" = device is read-only, \"w\" = writable,\n"
					"\"c\" = writable and file is created if not existent.\n"
					"Also the --xxdp, --rt11 and --size options are evaluated.\n"
					"A missing file is created and initialized with 0s or empty XXDP or RT11 filesystem.",
			"0 r 11XXDP.DSK", "mount image file XXDP.DSK into slot #0.",
			NULL, NULL);
	getopt_def(&getopt_parser, "sd", "shareddevice", "unit,read_write_create,directory",
	NULL, NULL, "same as --device, but image is filled with files from a host directory.\n"
			"-xxdp or -rt11 must be specified. <directory> must be writable and\n"
			"must not contain subdirs, it is created only with \"c\" option.",
			"1 w /home/user/tu58/data.dir", "fill image with files in a directory.",
			NULL, NULL);

	getopt_def(&getopt_parser, "st", "synctimeout", "seconds", NULL, "3",
			"An image changed by PDP is written to disk after this idle period.",
			NULL, NULL, NULL, NULL);
	/*
	 getopt_def(&getopt_parser, "ot", "offlinetimeout", "seconds", NULL, "3",
	 "By hitting a number-key 0..7, the device goes offline for user control.\n"
	 "PDP traffic shall not be interrupted,  so \"offline\" state is entered delayed\n"
	 "after RS232 port is silent for this period.",
	 NULL, NULL, NULL, NULL);
	 */
	sprintf(buff, "Read a binary disk/tape image, and extract files into directory\n"
			"A filesystem type must be specified (like -xxdp)\n"
			"<device_type> can specify a different device geometry for the image,\n"
			"allowed: %s", device_type_namelist());
	getopt_def(&getopt_parser, "up", "unpack", "filename,dirname,devicetype",
	NULL, NULL, buff,
	NULL, NULL, NULL, NULL);

	sprintf(buff, "Read a binary disk/tape image, and extract files into directory\n"
			"Read files from a directory and pack into binary disk/tape image\n"
			"<device_type> can specify a different device geometry for the image,\n"
			"allowed: %s", device_type_namelist());
	getopt_def(&getopt_parser, "pk", "pack", "dirname,filename,devicetype",
	NULL, NULL, buff,
	NULL, NULL, NULL, NULL);

	getopt_def(&getopt_parser, "boot", "boot", "monitor", "keep", NULL,
			"Deposits a TU58 bootloader over console monitor into PDP-11, then starts it.\n"
					"The TU58 emulator must have been started on a different serial port before.\n"
					"<port> and <baudrate> of the console are set by \"-p\" and \"-b\" options\n"
					"  left of \"--boot\".\n"
					"<monitor> specifies the implemented console: \"odt\", \"m9312\", \"1144\", or \"m9301\".\n"
					"  \"code\" displays the bootloader code as value/address pairs on stdout.\n"
					"With <keep>=1 the transfer dialog terminal window remains active, so you can\n"
					"  immediately operate the booted TU58 OS. With <keep>=0 connection is terminated and\n"
					"  you have to start a more comfortable terminal emulator.\n"
					"- The serial CONSOLE port is always DIFFERENT from the TU58 port!\n"
					"- The PDP-11 must be HALTed and show its monitor prompt,\n"
					"  with the HALT/RUN switch in RUN position.\n"
					"- the bootloader doesn't catch any TRAPs, so turn off the BEVENT/LTC signal.\n"
					"  The code is loaded at end of first 4k page at address 7000.\n",
					NULL, NULL, NULL, NULL);
	getopt_def(&getopt_parser, "ud", "usbdelay", "milliseconds", NULL, NULL,
			"Specifies extra delay for protocol in \"--boot\" operation.\n"
			"Some USB-RS232 adapters have large delays when polling input (for example FTDIs).\n"
			"Other brands (Prolific) and non-USB RS232 ports should never need this.\n"
			"Experiment for an optimum between download speed and reliability, recommended range 5-20.",
			NULL, NULL, NULL, NULL);

	/*
	 // test options
	 getopt_def(&getopt_parser, "testfs", "testfs", "filename", NULL, NULL,
	 "Read an image, convert it to filesystem and test",
	 NULL, NULL, NULL, NULL);
	 */
	if (argc < 2)
		help(); // at least 1 required

	drive_count = 0;
	opt_serial_port[0] = 0;
	opt_boot_monitor = monitor_none;

	res = getopt_first(&getopt_parser, argc, argv);
	while (res > 0) {
		if (getopt_isoption(&getopt_parser, "help")) {
			help();
		} else if (getopt_isoption(&getopt_parser, "version")) {
			info(version);
		} else if (getopt_isoption(&getopt_parser, "debug")) {
			opt_debug = 1;
		} else if (getopt_isoption(&getopt_parser, "verbose")) {
			opt_verbose = 1;
		} else if (getopt_isoption(&getopt_parser, "mrsp")) {
			opt_mrspen = 1;
		} else if (getopt_isoption(&getopt_parser, "nosync")) {
			opt_nosync = 1;
		} else if (getopt_isoption(&getopt_parser, "vax")) {
			opt_vax = 1;
		} else if (getopt_isoption(&getopt_parser, "synctimeout")) {
			if (getopt_arg_i(&getopt_parser, "seconds", &opt_synctimeout_sec) < 0)
				commandline_option_error(NULL);
			/*
			 } else if (getopt_isoption(&getopt_parser, "offlinetimeout")) {
			 if (getopt_arg_i(&getopt_parser, "seconds", &opt_offlinetimeout_sec) < 0)
			 commandline_option_error(NULL);
			 */
		} else if (getopt_isoption(&getopt_parser, "background")) {
			opt_background = 1;
		} else if (getopt_isoption(&getopt_parser, "timing")) {
			if (getopt_arg_i(&getopt_parser, "parameter", &opt_timing) < 0)
				commandline_option_error(NULL);
			if (opt_timing > 2)
				commandline_option_error("<timing> max 2");
		} else if (getopt_isoption(&getopt_parser, "baudrate")) {
			if (getopt_arg_i(&getopt_parser, "baudrate", &opt_serial_speed) < 0)
				commandline_option_error(NULL);
		} else if (getopt_isoption(&getopt_parser, "format")) {
			char formatstr[80];
			if (getopt_arg_s(&getopt_parser, "bits_parity_stop", formatstr, sizeof(formatstr))
					< 0)
				commandline_option_error(NULL);
			if (serial_decode_format(formatstr, &opt_serial_bitcount, &opt_serial_parity,
					&opt_serial_stopbits))
				commandline_option_error("Illegal format");
		} else if (getopt_isoption(&getopt_parser, "port")) {
			if (getopt_arg_s(&getopt_parser, "serial_device", opt_serial_port,
					sizeof(opt_serial_port)) < 0)
				commandline_option_error(NULL);
		} else if (getopt_isoption(&getopt_parser, "xxdp")) {
			cur_filesystem_type = fsXXDP;
		} else if (getopt_isoption(&getopt_parser, "rt11")) {
			cur_filesystem_type = fsRT11;
		} else if (getopt_isoption(&getopt_parser, "size")) {
			char buff[256];
			int len;
			if (getopt_arg_s(&getopt_parser, "size", buff, sizeof(buff)) < 0)
				commandline_option_error(NULL);
			len = strlen(buff) - 1; // last char
			if (!strcasecmp(buff, "STD")) {
				cur_image_size = 0; // no size specified
			} else {
				int scale = 1;
				char *endptr;
				if (toupper(buff[len]) == 'K') {
					scale = 1024;
					buff[len] = 0;
				} else if (toupper(buff[len]) == 'M') {
					scale = 1024 * 1024;
					buff[len] = 0;
				}
				cur_image_size = strtol(buff, &endptr, 0) * scale;
				if (!cur_image_size || (endptr != buff + len))
					commandline_option_error("<size> illegal");
			}

		} else if (getopt_isoption(&getopt_parser, "device")
				|| getopt_isoption(&getopt_parser, "shareddevice")) {
			int shared = getopt_isoption(&getopt_parser, "shareddevice");
			int unit;
			int readonly = 0; // initialize only to silence compiler
			int allowcreate = 0;
			char pathbuff[4096];
			if (getopt_arg_i(&getopt_parser, "unit", &unit) < 0)
				commandline_option_error(NULL);
			if (unit < 0 || unit >= TU58_DEVICECOUNT)
				commandline_option_error("<unit> between 0 and %d",
				TU58_DEVICECOUNT);

			if (getopt_arg_s(&getopt_parser, "read_write_create", pathbuff, sizeof(pathbuff))
					< 0)
				commandline_option_error(NULL);
			if (toupper(pathbuff[0]) == 'R') {
				readonly = 1;
				allowcreate = 0;
			} else if (toupper(pathbuff[0]) == 'W') {
				readonly = 0;
				allowcreate = 0;
			} else if (toupper(pathbuff[0]) == 'C') {
				readonly = 0;
				allowcreate = 1;
			} else
				commandline_option_error("<device> status must be of type R, W or C");
			if (shared) {
				if (getopt_arg_s(&getopt_parser, "directory", pathbuff, sizeof(pathbuff)) < 0)
					commandline_option_error(NULL);
				if (cur_filesystem_type == fsNONE)
					commandline_option_error("No filesystem type specified.");
			} else {
				if (getopt_arg_s(&getopt_parser, "filename", pathbuff, sizeof(pathbuff)) < 0)
					commandline_option_error(NULL);
			}

			tu58image_create(unit, cur_image_size);
			if (image_open(tu58image_get(unit), shared, readonly, allowcreate, pathbuff,
					cur_filesystem_type) < 0)
				commandline_option_error(NULL);
			image_info(tu58image_get(unit));
			drive_count++;
		} else if (getopt_isoption(&getopt_parser, "unpack")) {
			char filename[4096];
			char dirname[4096];
			char device_type_s[256];
			image_t *img;
			filesystem_t *pdp_fs;
			hostdir_t *hostdir;
			device_type_t device_type;
			if (getopt_arg_s(&getopt_parser, "filename", filename, sizeof(filename)) < 0)
				commandline_option_error(NULL);
			if (getopt_arg_s(&getopt_parser, "dirname", dirname, sizeof(dirname)) < 0)
				commandline_option_error(NULL);
			if (getopt_arg_s(&getopt_parser, "devicetype", device_type_s, sizeof(device_type_s))
					< 0)
				commandline_option_error(NULL);
			device_type = device_type_from_name(device_type_s);

			if (cur_filesystem_type == fsNONE)
				commandline_option_error("No filesystem type specified.");
			if (device_type == devNONE)
				commandline_option_error("No <devicetype>");
			if (cur_image_size != 0 && device_type != devTU58)
				commandline_option_error("--size specification allowed only for TU58 device.");

			// read binary
			img = image_create(device_type, 0xff, cur_image_size);
			// autosizing, so no device info needed
			if (image_open(img, 0, 0, 0, filename, cur_filesystem_type))
				fatal("image_open failed");

			pdp_fs = filesystem_create(cur_filesystem_type, device_type, 0, img->data,
					img->data_size, img->changedblocks);
			if (filesystem_parse(pdp_fs))
				fatal("filesystem_parse failed");
			hostdir = hostdir_create(0, dirname, pdp_fs);
			if (hostdir_prepare(hostdir, 1, 1, NULL))
				fatal("hostdir_prepare failed");
			hostdir_from_pdp_fs(hostdir);
			filesystem_print_dir(pdp_fs, ferr);
			info("Files extracted from \"%s\" and written to \"%s\".", filename, dirname);
			filesystem_destroy(pdp_fs);
			hostdir_destroy(hostdir);
			image_destroy(img);

		} else if (getopt_isoption(&getopt_parser, "pack")) {
			char filename[4096];
			char dirname[4096];
			char device_type_s[256];
			image_t *img;
			filesystem_t *pdp_fs;
			hostdir_t *hostdir;
			device_type_t device_type;
			if (getopt_arg_s(&getopt_parser, "filename", filename, sizeof(filename)) < 0)
				commandline_option_error(NULL);
			if (getopt_arg_s(&getopt_parser, "dirname", dirname, sizeof(dirname)) < 0)
				commandline_option_error(NULL);
			if (getopt_arg_s(&getopt_parser, "devicetype", device_type_s, sizeof(device_type_s))
					< 0)
				commandline_option_error(NULL);
			device_type = device_type_from_name(device_type_s);
			if (device_type == devNONE)
				commandline_option_error("No <devicetype>");
			if (cur_filesystem_type == fsNONE)
				commandline_option_error("No filesystem type specified.");
			if (cur_image_size != 0 && device_type != devTU58)
				commandline_option_error("--size specification allowed only for TU58 device.");
			img = image_create(device_type, 0xff, cur_image_size); // img is just a data buffer

			pdp_fs = filesystem_create(cur_filesystem_type, device_type, 0, img->data,
					img->data_size, NULL);
			hostdir = hostdir_create(0, dirname, pdp_fs);
			if (hostdir_prepare(hostdir, 0, 0, NULL)) // check
				fatal("hostdir_prepare failed");
			if (hostdir_to_pdp_fs(hostdir))
				fatal("hostdir_to_pdp_fs failed");
			if (filesystem_render(pdp_fs))
				fatal("filesystem_render failed");
			filesystem_print_dir(pdp_fs, ferr);
			{
				FILE *f = fopen(filename, "w");
				if (!f)
					fatal("opening file %s failed", filename);
				fwrite(img->data, 1, img->data_size, f);
				fclose(f);
			}
			filesystem_destroy(pdp_fs);
			hostdir_destroy(hostdir);
			image_destroy(img);

		} else if (getopt_isoption(&getopt_parser, "boot")) {
			char buff[256];
			if (getopt_arg_s(&getopt_parser, "monitor", buff, sizeof(buff)) < 0)
				commandline_option_error(NULL);
			if (!strcasecmp("CODE", buff)) {
				opt_boot_monitor = monitor_showcode;
				opt_boot_keep = 0;
			} else {
				if (!strcasecmp("ODT", buff))
					opt_boot_monitor = monitor_odt;
				else if (!strcasecmp("M9312", buff))
					opt_boot_monitor = monitor_m9312;
				else if (!strcasecmp("M9301", buff))
					opt_boot_monitor = monitor_m9301;
                else if (!strcasecmp("1144", buff))
                    opt_boot_monitor = monitor_1144;
				else
					commandline_option_error("Illegal monitor \"%s\"", buff);

				if (getopt_arg_i(&getopt_parser, "keep", &opt_boot_keep) < 0)
					commandline_option_error(NULL);
				// evaluation later in main()
			}
		} else if (getopt_isoption(&getopt_parser, "usbdelay")) {
			if (getopt_arg_i(&getopt_parser, "milliseconds", &opt_usbdelay) < 0)
				commandline_option_error(NULL);
		}
		res = getopt_next(&getopt_parser);
	}
	if (res == GETOPT_STATUS_MINARGCOUNT || res == GETOPT_STATUS_MAXARGCOUNT)
		// known option, but wrong number of arguments
		commandline_option_error(NULL);
	else if (res < 0)
		commandline_error();
}

static void check_capabilities() {
	int unit;
	image_t *img;
	filesystem_type_t fstype = fsNONE;
	unsigned fssize;

	if (drive_count > 0 && opt_boot_monitor != monitor_none)
		fatal("--boot function incompatible with device emulation!");

	if (drive_count > 0 && strlen(opt_serial_port) == 0) {
		fatal("No serial port specified, drive emulator not started.");
	}

	if (opt_boot_monitor != monitor_none && opt_boot_monitor != monitor_showcode && strlen(opt_serial_port) == 0) {
		fatal("No serial port specified, boot loader transfer not started.");
	}

	if (drive_count > 0 && opt_serial_bitcount != 8)
		fatal("TU58 drive emulation requires 8 bit serial line format!");

	// XXDP: boot device #0 oversized?
	img = tu58image_get(0);
	if (img && img->dec_filesystem == fsXXDP
			&& img->data_size != (unsigned)img->device_info->block_count * img->blocksize)
		warning("XXDP device #0 is oversized, XXDP2.5 can not boot this");

	//	RT-11: only dd0 and dd1:
	for (unit = 2; unit < TU58_DEVICECOUNT; unit++) {
		img = tu58image_get(unit);
		if (img && img->dec_filesystem == fsRT11)
			warning("RT-11 can only access DD0 and DD1:, TU58 unit %d not usable", unit);
	}
	// all devices same filesystem?

	fstype = fsNONE;
	for (unit = 0; unit < TU58_DEVICECOUNT; unit++) {
		img = tu58image_get(unit);
		if (img) {
			if (fstype == fsNONE)
				fstype = img->dec_filesystem;
			else if (fstype != img->dec_filesystem)
				fatal("All devices must have the same filesystem type!");
		}
	}

	// RT-11: all devices same size, when --size

	fssize = 0;
	for (unit = 0; unit < TU58_DEVICECOUNT; unit++) {
		img = tu58image_get(unit);
		if (img && img->dec_filesystem == fsRT11) {
			if (fssize == 0)
				fssize = img->data_size;
			else if (fssize != img->data_size)
				warning("All RT-11 devices must have the same image size!");
		}
	}

	// RT-11: boot device readonly, if patched?
	img = tu58image_get(0);
	if (img && img->dec_filesystem == fsRT11
			&& img->data_size != (unsigned)img->device_info->block_count * img->blocksize
			&& !img->readonly)
		warning("RT-11 device #0 is oversized and on shared dir.\n"
				"RT-11 v5.3 can not reload image after changed, so should be read-only.\n"
				"RT-11 v5.5 seems to be OK.");
}

static pthread_t th_run;	// emulator thread id
static pthread_t th_monitor;	// monitor thread id

#ifdef DEVICEDIALOG
// user wants to set a device offline for work
static void device_dialog(image_t *img) {
	char *tokens[256];
	char *fpath;
	int ready = 0;
	int n;

	tu58_offline_request = 1;
	info("TU58 goes offline after %d seconds of RS232 inactivity ...", opt_offlinetimeout_sec);
	while (!tu58_offline)
	delay_ms(100);
	info("TU58 now offline: \"all cartridges removed\".");

	while (!ready) {
		fprintf(ferr, "Choices for device %d%s :\n", img->unit,
				img->changed ? " (unsaved changes)" : "");
		fprintf(ferr, "S - save image to disk%s\n", img->changed ? "" : " (though unchanged)");
		if (img->shared)
		fprintf(ferr, "L <file>- reload image from dir \"%s\"\n", img->host_fpath);
		else
		fprintf(ferr, "L - load other image file (now \"%s\")\n", img->host_fpath);
		fprintf(ferr, "C - continue server with all drives online (cartridges inserted).\n");
		fprintf(ferr, ".	");

		do {
			n = inputline(tokens, 256);
			delay_ms(100);
		}while (n == 0);

		switch (toupper(tokens[0][0])) {
			case 'S':
			image_save(img);
			break;
			case 'L':
			if (n != 2) {
				error("Syntax: \"L filename\"");
				break;
			}
			fpath = tokens[1];
			if (img->shared)
			;
			else {
				// image_close(img);
				// re-open with all params same, only other file
				if (image_open(img, img->shared, img->readonly, 0, fpath, img->dec_filesystem)
						< 0)
				error("Opening file \"%s\" failed", fpath);
				else
				info("Opened file \"%s\"", fpath);
			}
			break;
			case 'C':
			ready = 1;
			break;

		}
	}
	// go online
	tu58_offline_request = 0;
}
#endif
//
// start tu58 drive emulation
//
void run_emulator(void) {
	// a sanity check for blocksize definition
	if (TU58_BLOCKSIZE % TU_DATA_LEN != 0)
		fatal("illegal BLOCKSIZE (%d) / TU_DATA_LEN (%d) ratio", TU58_BLOCKSIZE,
		TU_DATA_LEN);

	// say hello
	info("TU58 emulation start");
#ifdef DEVICEDIALOG
	info("0-7 device dialog, R restart, S toggle send init, V toggle verbose, D toggle debug, Q quit");
#else
	info("R restart, S toggle send init, V toggle verbose, D toggle debug, Q quit");
#endif

	// run the emulator
	if (pthread_create(&th_run, NULL, tu58_server, NULL))
		error("unable to create emulation thread");

	// run the monitor
	if (pthread_create(&th_monitor, NULL, tu58_monitor, NULL))
		error("unable to create monitor thread");

	// loop on user input
	for (;;) {
		uint8_t c;

		// get char from stdin (if available)
		if ((c = toupper(conget())) > 0) {
#ifdef DEVICEDIALOG
			if (c >= '0' && c <= '7') {
				image_t *img = NULL;
				int unit = c - '0';
				// number of open device?
				if (IMAGE_UNIT_VALID(unit))
				img = tu58image_get(unit);
				if (img && img->open)
				device_dialog(img);
			}
#endif
			if (c == 'V') {
				// toggle verbosity
				opt_verbose ^= 1;
				opt_debug = 0;
				info("verbosity set to %s; debug %s", opt_verbose ? "ON" : "OFF",
						opt_debug ? "ON" : "OFF");
			} else if (c == 'D') {
				// toggle debug
				opt_verbose = 1;
				opt_debug ^= 1;
				info("verbosity set to %s; debug %s", opt_verbose ? "ON" : "OFF",
						opt_debug ? "ON" : "OFF");
			} else if (c == 'S') {
				// toggle sending init string
				tu58_doinit = (tu58_doinit + 1) % 2;
				if (opt_debug)
					fprintf(ferr, "\n");
				info("send of <INIT> %sabled", tu58_doinit ? "en" : "dis");
			} else if (c == 'R') {
				// kill and restart the emulator
				if (pthread_cancel(th_run))
					error("unable to cancel emulation thread");
				if (pthread_join(th_run, NULL))
					error("unable to join on emulation thread");
				if (pthread_create(&th_run, NULL, tu58_server, NULL))
					error("unable to restart emulation thread");
			} else if (c == 'Q') {
				// kill the emulator and exit
				if (pthread_cancel(th_monitor))
					error("unable to cancel monitor thread");
				if (pthread_cancel(th_run))
					error("unable to cancel emulation thread");
				break;
			}
		}

		// wait a bit
		delay_ms(25);

	} // for (;;)

	// wait for emulator to finish
	if (pthread_join(th_run, NULL))
		error("unable to join on emulation thread");

	// all done
	info("TU58 emulation end");
	return;
}

int main(int argc, char *argv[]) {

	error_clear();
	ferr = stdout; // ferr in Eclipse console not visible?

	tu58images_init();

	parse_commandline(argc, argv);
	// returns only if everything is OK
	// Std options already executed

	// some debug info
	if (opt_debug) {
		info(version);
		info(copyright);
	}

	// some warnings
	check_capabilities();

	// give some info
	info("Using serial port %s at %d baud with %d%c%d format.", opt_serial_port,
			opt_serial_speed, opt_serial_bitcount, opt_serial_parity, opt_serial_stopbits);

	if (drive_count == 0) {
		info("No simulated drives were specified, emulator not started.");
	} else {
		// emulation: must have opened at least one unit

		if (opt_mrspen)
			info("MRSP mode enabled (NOT fully tested - use with caution)");

		// setup serial and console ports
		serial_devinit(&tu58_serial, opt_serial_port, opt_serial_speed, opt_serial_bitcount,
				opt_serial_parity, opt_serial_stopbits);
		coninit(0); // normal without echo

		// start thread with tu58 emulator
		run_emulator();

		// restore serial and console ports
		conrestore();
		serial_devrestore(&tu58_serial);

		// write back unsaved files and close
		tu58images_closeall();
	}

	// boot loader?
	if (opt_boot_monitor == monitor_showcode) {
		bootloader_show_code(stdout, opt_boot_address);
	} else if (opt_boot_monitor != monitor_none) {
		int res;
		serial_device_t monitor_serial; // PDP-11 console
		serial_devinit(&monitor_serial, opt_serial_port, opt_serial_speed, opt_serial_bitcount,
				opt_serial_parity, opt_serial_stopbits);
		coninit(1); // host console, raw: no processing of PDP-11 I/O

		// download, and echo to console
		res = bootloader_download(&monitor_serial, opt_boot_monitor, opt_boot_address);
		if (!res)
			res = bootloader_go(&monitor_serial, opt_boot_monitor, opt_boot_address);
		// no error

		if (opt_boot_keep) {
			if (res) {
				run_teletype(&monitor_serial, opt_boot_monitor,
						"PDP-11 does not respond. Use this teletype session to verify console prompt.");
			} else
				// interactive tty terminal on PDP-11 monitor port
				run_teletype(&monitor_serial, opt_boot_monitor, NULL);
		}

		// restore serial and console ports
		conrestore();
		serial_devrestore(&monitor_serial);
	}
	return 0 ;
}
