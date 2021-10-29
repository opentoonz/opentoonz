/*
 * Copyright (c) 2019, Thomas Bernard  <miniupnp@free.fr>
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that (i) the above copyright notices and this permission notice appear in
 * all copies of the software and related documentation, and (ii) the names of
 * Sam Leffler and Silicon Graphics may not be used in any advertising or
 * publicity relating to the software without the specific, prior written
 * permission of Sam Leffler and Silicon Graphics.
 *
 * THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY
 * WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 *
 * IN NO EVENT SHALL SAM LEFFLER OR SILICON GRAPHICS BE LIABLE FOR
 * ANY SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND,
 * OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF
 * LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

/*
 * TIFF Library
 *
 * Module to test ASCII tags read/write functions.
 */

#include "tif_config.h"

#include <stdio.h>

#include "tiffio.h"

#define CHECK_TYPE(t, s) \
	if (sizeof(t) != s) { \
		fprintf(stderr, "sizeof(" # t ")=%d, it should be %d\n", (int)sizeof(t), (int)s); \
		return 1; \
	}

int
main()
{
	CHECK_TYPE(int8_t, 1)
	CHECK_TYPE(int16_t, 2)
	CHECK_TYPE(int32_t, 4)
	CHECK_TYPE(int64_t, 8)
	CHECK_TYPE(uint8_t, 1)
	CHECK_TYPE(uint16_t, 2)
	CHECK_TYPE(uint32_t, 4)
	CHECK_TYPE(uint64_t, 8)
	CHECK_TYPE(tmsize_t , sizeof(size_t))
	return 0;
}

/* vim: set ts=8 sts=8 sw=8 noet: */
