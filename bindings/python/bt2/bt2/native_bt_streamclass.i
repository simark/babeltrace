/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Philippe Proulx <pproulx@efficios.com>
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
struct bt_trace;
struct bt_stream_class;
struct bt_event_class;
struct bt_clock_class;
struct bt_event_header_field;
struct bt_packet_context_field;

/* Functions */
struct bt_stream_class *bt_stream_class_create(struct bt_trace *trace);
struct bt_stream_class *bt_stream_class_create_with_id(
		struct bt_trace *trace, uint64_t id);
struct bt_trace *bt_stream_class_borrow_trace(
		struct bt_stream_class *stream_class);
const char *bt_stream_class_get_name(
		struct bt_stream_class *stream_class);
int bt_stream_class_set_name(struct bt_stream_class *stream_class,
		const char *name);
bt_bool bt_stream_class_assigns_automatic_event_class_id(
		struct bt_stream_class *stream_class);
int bt_stream_class_set_assigns_automatic_event_class_id(
		struct bt_stream_class *stream_class, bt_bool value);
bt_bool bt_stream_class_assigns_automatic_stream_id(
		struct bt_stream_class *stream_class);
int bt_stream_class_set_assigns_automatic_stream_id(
		struct bt_stream_class *stream_class, bt_bool value);
uint64_t bt_stream_class_get_id(struct bt_stream_class *stream_class);
struct bt_field_type *bt_stream_class_borrow_packet_context_field_type(
		struct bt_stream_class *stream_class);
int bt_stream_class_set_packet_context_field_type(
		struct bt_stream_class *stream_class,
		struct bt_field_type *field_type);
struct bt_field_type *
bt_stream_class_borrow_event_header_field_type(
		struct bt_stream_class *stream_class);
int bt_stream_class_set_event_header_field_type(
		struct bt_stream_class *stream_class,
		struct bt_field_type *field_type);
struct bt_field_type *
bt_stream_class_borrow_event_common_context_field_type(
		struct bt_stream_class *stream_class);
int bt_stream_class_set_event_common_context_field_type(
		struct bt_stream_class *stream_class,
		struct bt_field_type *field_type);
uint64_t bt_stream_class_get_event_class_count(
		struct bt_stream_class *stream_class);
struct bt_event_class *bt_stream_class_borrow_event_class_by_index(
		struct bt_stream_class *stream_class, uint64_t index);
int bt_stream_class_set_default_clock_class(
		struct bt_stream_class *stream_class,
		struct bt_clock_class *clock_class);
struct bt_clock_class *bt_stream_class_borrow_default_clock_class(
		struct bt_stream_class *stream_class);
bt_bool bt_stream_class_default_clock_is_always_known(
		struct bt_stream_class *stream_class);
bt_bool bt_stream_class_packets_have_discarded_event_counter_snapshot(
		struct bt_stream_class *stream_class);
int bt_stream_class_set_packets_have_discarded_event_counter_snapshot(
		struct bt_stream_class *stream_class, bt_bool value);
bt_bool bt_stream_class_packets_have_packet_counter_snapshot(
		struct bt_stream_class *stream_class);
int bt_stream_class_set_packets_have_packet_counter_snapshot(
		struct bt_stream_class *stream_class, bt_bool value);
bt_bool bt_stream_class_packets_have_default_beginning_clock_value(
		struct bt_stream_class *stream_class);
int bt_stream_class_set_packets_have_default_beginning_clock_value(
		struct bt_stream_class *stream_class, bt_bool value);
bt_bool bt_stream_class_packets_have_default_end_clock_value(
		struct bt_stream_class *stream_class);
int bt_stream_class_set_packets_have_default_end_clock_value(
		struct bt_stream_class *stream_class, bt_bool value);
