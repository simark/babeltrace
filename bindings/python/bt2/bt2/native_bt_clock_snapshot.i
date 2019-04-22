/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Francis Deslauriers <francis.deslauriers@efficios.com>
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
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/* From clock-snapshot-const.h */

typedef enum bt_clock_snapshot_state {
	BT_CLOCK_SNAPSHOT_STATE_KNOWN,
	BT_CLOCK_SNAPSHOT_STATE_UNKNOWN,
} bt_clock_snapshot_state;

typedef enum bt_clock_snapshot_status {
	BT_CLOCK_SNAPSHOT_STATUS_OK = 0,
	BT_CLOCK_SNAPSHOT_STATUS_OVERFLOW = -75,
} bt_clock_snapshot_status;

extern const bt_clock_class *bt_clock_snapshot_borrow_clock_class_const(
		const bt_clock_snapshot *clock_snapshot);

extern uint64_t bt_clock_snapshot_get_value(
		const bt_clock_snapshot *clock_snapshot);

extern bt_clock_snapshot_status bt_clock_snapshot_get_ns_from_origin(
		const bt_clock_snapshot *clock_snapshot,
		int64_t *OUTPUTINIT);
