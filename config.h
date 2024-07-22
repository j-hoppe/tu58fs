/*  config.h - server for TU58 protocol, ctrl over serial line, work on image
 *
 *  Original (C) 1984 Dan Ts'o <Rockefeller Univ. Dept. of Neurobiology>
 *  Update   (C) 2005-2016 Donald N North <ak6dn_at_mindspring_dot_com>
 *  Update   (C) 2017 Joerg Hoppe <j_hoppe@t-online.de>, www.retrocmp.com
 *  Update   (C) 2024 June Tate-Gans <june@theonelab.com>, www.nybblesandbytes.net
 *
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  o Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  o Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 *  o Neither the name of the copyright holder nor the names of its
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
 *  This is the TU58 emulation program written at Rockefeller Univ., Dept. of
 *  Neurobiology. We copyright (C) it and permit its use provided it is not
 *  sold to others. Originally written by Dan Ts'o circa 1984 or so.
 *
 *  21-Jul-2024 June Tate-Gans  add this configuration header
 */
#ifndef CONFIG_H
#define CONFIG_H

/*
 * This configuration header helps to setup the source tree better
 * for your host compilation system. Some of this requires you to
 * know how your system is setup. In general, if you're running
 * a recent system, the defaults should work for you. Otherwise,
 * let the compiler error output be your guide.
 */

/* Whether or not your system has clock_gettime */
#define HAVE_CLOCK_GETTIME 1

#endif // CONFIG_H

