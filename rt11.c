/*
 *  rt11.c: Handles the RT11 filesystem on an image.
 *
 *  Copyright (c) 2017, Joerg Hoppe j_hoppe@t-online.de, www.retrocmp.com
 *  (C) 2005-2016 Donald N North <ak6dn_at_mindspring_dot_com>
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
 *  Created on: 12.01.2017
 */
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "image.h"
#include "rt11.h"	// own

//
// init RT-11 file directory structures (based on RT-11 v5.4)
//
int rt11_init(image_t *img) {
	int32_t i;

	static int16_t boot[] = { // offset 0000000
			0000240, 0000005, 0000404, 0000000, 0000000, 0041420, 0116020,
					0000400, 0004067, 0000044, 0000015, 0000000, 0005000,
					0041077, 0047517, 0026524, 0026525, 0067516, 0061040,
					0067557, 0020164, 0067157, 0073040, 0066157, 0066565,
					0006545, 0005012, 0000200, 0105737, 0177564, 0100375,
					0112037, 0177566, 0100372, 0000777 };

	static int16_t bitmap[] = { // offset 0001000
			0000000, 0170000, 0007777 };

	static int16_t direct1[] = { // offset 0001700
			0177777, 0000000, 0000000, 0000000, 0000000, 0000000, 0000000,
					0000000, 0000000, 0000001, 0000006, 0107123, 0052122,
					0030461, 0020101, 0020040, 0020040, 0020040, 0020040,
					0020040, 0020040, 0020040, 0020040, 0020040, 0042504,
					0051103, 0030524, 0040461, 0020040, 0020040 };

	static int16_t direct2[] = { // offset 0006000
			0000001, 0000000, 0000001, 0000000, 0000010, 0001000, 0000325,
					0063471, 0023364, 0000770, 0000000, 0002264, 0004000 };

	static struct {
		int16_t *data;
		int16_t length;
		int32_t offset;
	} table[] = { { boot, sizeof(boot), 00000 },
			{ bitmap, sizeof(bitmap), 01000 },
			{ direct1, sizeof(direct1), 01700 }, { direct2, sizeof(direct2),
					06000 }, { NULL, 0, 0 } };

	// now write data from the table
	for (i = 0; table[i].length; i++) {
		image_lseek(img, table[i].offset, SEEK_SET);
		if (image_write(img, table[i].data, table[i].length)
				!= table[i].length)
			return -1;
	}

	return 0;
}

