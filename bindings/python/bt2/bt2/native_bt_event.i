/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2017 Philippe Proulx <pproulx@efficios.com>
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

/* From event-const.h */

typedef enum bt_event_status {
	BT_EVENT_STATUS_OK = 0,
	BT_EVENT_STATUS_NOMEM = -12,
} bt_event_status;

extern const bt_event_class *bt_event_borrow_class_const(
		const bt_event *event);

extern const bt_packet *bt_event_borrow_packet_const(
		const bt_event *event);

extern const bt_stream *bt_event_borrow_stream_const(
		const bt_event *event);

extern const bt_field *bt_event_borrow_common_context_field_const(
		const bt_event *event);

extern const bt_field *bt_event_borrow_specific_context_field_const(
		const bt_event *event);

extern const bt_field *bt_event_borrow_payload_field_const(
		const bt_event *event);

/* From event.h */

extern bt_event_class *bt_event_borrow_class(bt_event *event);

extern bt_packet *bt_event_borrow_packet(bt_event *event);

extern bt_stream *bt_event_borrow_stream(bt_event *event);

extern bt_field *
bt_event_borrow_common_context_field(bt_event *event);

extern bt_field *
bt_event_borrow_specific_context_field(bt_event *event);

extern bt_field *bt_event_borrow_payload_field(bt_event *event);
