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

/* Type */
struct bt_event;
struct bt_event_header_field;
struct bt_clock_value;
struct bt_event_class;
struct bt_field;
struct bt_packet;

enum bt_clock_value_status;

/* Functions */
struct bt_event_class *bt_event_borrow_class(struct bt_event *event);

struct bt_packet *bt_event_borrow_packet(struct bt_event *event);

struct bt_stream *bt_event_borrow_stream(struct bt_event *event);

struct bt_field *bt_event_borrow_header_field(struct bt_event *event);

int bt_event_move_header(struct bt_event *event,
		struct bt_event_header_field *header);

struct bt_field *bt_event_borrow_common_context_field(
		struct bt_event *event);

struct bt_field *bt_event_borrow_specific_context_field(
		struct bt_event *event);

struct bt_field *bt_event_borrow_payload_field(struct bt_event *event);

int bt_event_set_default_clock_value(struct bt_event *event,
		uint64_t value_cycles);

enum bt_clock_value_status bt_event_borrow_default_clock_value(
		struct bt_event *event, struct bt_clock_value **BTOUTCLOCKVALUE);
