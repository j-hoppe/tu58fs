/* boolarray.c: large binary array
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

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "boolarray.h"

boolarray_t *boolarray_create(uint32_t bitcount) {
	boolarray_t *result = malloc(sizeof(boolarray_t));
	result->bitcount = bitcount;
	result->flags = malloc((bitcount / 32) + 1);
	boolarray_clear(result);
	return result;
}

void boolarray_destroy(boolarray_t *_this) {
	free(_this->flags);
	free(_this);
}

void boolarray_clear(boolarray_t *_this) {
	memset(_this->flags, 0, _this->bitcount / 32 + 1);
}

void boolarray_bit_set(boolarray_t *_this, uint32_t i) {
	assert(i < _this->bitcount);
	uint32_t w = _this->flags[i / 32];
	w |= (1 << (i % 32));
	_this->flags[i / 32] = w;
}

void boolarray_bit_clear(boolarray_t *_this, uint32_t i) {
	assert(i < _this->bitcount);
	uint32_t w = _this->flags[i / 32];
	w &= ~(1 << (i % 32));
	_this->flags[i / 32] = w;
}

int boolarray_bit_get(boolarray_t *_this, uint32_t i) {
	assert(i < _this->bitcount);
	uint32_t w = _this->flags[i / 32];
	return !!(w & (1 << (i % 32)));
}
