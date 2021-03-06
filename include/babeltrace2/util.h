#ifndef BABELTRACE2_UTIL_H
#define BABELTRACE2_UTIL_H

/*
 * Copyright (c) 2010-2019 EfficiOS Inc. and Linux Foundation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __BT_IN_BABELTRACE_H
# error "Please include <babeltrace2/babeltrace.h> instead."
#endif

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum bt_util_clock_cycles_to_ns_from_origin_status {
	BT_UTIL_CLOCK_CYCLES_TO_NS_FROM_ORIGIN_STATUS_OK		= __BT_FUNC_STATUS_OK,
	BT_UTIL_CLOCK_CYCLES_TO_NS_FROM_ORIGIN_STATUS_OVERFLOW_ERROR	= __BT_FUNC_STATUS_OVERFLOW_ERROR,
} bt_util_clock_cycles_to_ns_from_origin_status;

bt_util_clock_cycles_to_ns_from_origin_status
bt_util_clock_cycles_to_ns_from_origin(uint64_t cycles,
		uint64_t frequency, int64_t offset_seconds,
		uint64_t offset_cycles, int64_t *ns);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_UTIL_H */
