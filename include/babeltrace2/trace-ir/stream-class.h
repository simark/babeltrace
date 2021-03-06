#ifndef BABELTRACE2_TRACE_IR_STREAM_CLASS_H
#define BABELTRACE2_TRACE_IR_STREAM_CLASS_H

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

#include <babeltrace2/types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern bt_stream_class *bt_stream_class_create(
		bt_trace_class *trace_class);

extern bt_stream_class *bt_stream_class_create_with_id(
		bt_trace_class *trace_class, uint64_t id);

extern bt_value *bt_stream_class_borrow_user_attributes(
		bt_stream_class *stream_class);

extern void bt_stream_class_set_user_attributes(
		bt_stream_class *stream_class, const bt_value *user_attributes);

extern bt_trace_class *bt_stream_class_borrow_trace_class(
		bt_stream_class *stream_class);

typedef enum bt_stream_class_set_name_status {
	BT_STREAM_CLASS_SET_NAME_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
	BT_STREAM_CLASS_SET_NAME_STATUS_OK		= __BT_FUNC_STATUS_OK,
} bt_stream_class_set_name_status;

extern bt_stream_class_set_name_status bt_stream_class_set_name(
		bt_stream_class *stream_class, const char *name);

extern void bt_stream_class_set_assigns_automatic_event_class_id(
		bt_stream_class *stream_class, bt_bool value);

extern void bt_stream_class_set_assigns_automatic_stream_id(
		bt_stream_class *stream_class, bt_bool value);

extern void bt_stream_class_set_supports_discarded_events(
		bt_stream_class *stream_class,
		bt_bool supports_discarded_events,
		bt_bool with_default_clock_snapshots);

extern void bt_stream_class_set_supports_discarded_packets(
		bt_stream_class *stream_class,
		bt_bool supports_discarded_packets,
		bt_bool with_default_clock_snapshots);

typedef enum bt_stream_class_set_field_class_status {
	BT_STREAM_CLASS_SET_FIELD_CLASS_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
	BT_STREAM_CLASS_SET_FIELD_CLASS_STATUS_OK		= __BT_FUNC_STATUS_OK,
} bt_stream_class_set_field_class_status;

extern void bt_stream_class_set_supports_packets(
		bt_stream_class *stream_class, bt_bool supports_packets,
		bt_bool with_beginning_default_clock_snapshot,
		bt_bool with_end_default_clock_snapshot);

extern bt_stream_class_set_field_class_status
bt_stream_class_set_packet_context_field_class(
		bt_stream_class *stream_class,
		bt_field_class *field_class);

extern bt_field_class *
bt_stream_class_borrow_packet_context_field_class(
		bt_stream_class *stream_class);

extern bt_stream_class_set_field_class_status
bt_stream_class_set_event_common_context_field_class(
		bt_stream_class *stream_class,
		bt_field_class *field_class);

extern bt_field_class *
bt_stream_class_borrow_event_common_context_field_class(
		bt_stream_class *stream_class);

extern bt_event_class *
bt_stream_class_borrow_event_class_by_index(
		bt_stream_class *stream_class, uint64_t index);

extern bt_event_class *
bt_stream_class_borrow_event_class_by_id(
		bt_stream_class *stream_class, uint64_t id);

extern bt_clock_class *bt_stream_class_borrow_default_clock_class(
		bt_stream_class *stream_class);

typedef enum bt_stream_class_set_default_clock_class_status {
	BT_STREAM_CLASS_SET_DEFAULT_CLOCK_CLASS_STATUS_OK	= __BT_FUNC_STATUS_OK,
} bt_stream_class_set_default_clock_class_status;

extern bt_stream_class_set_default_clock_class_status
bt_stream_class_set_default_clock_class(
		bt_stream_class *stream_class,
		bt_clock_class *clock_class);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_TRACE_IR_STREAM_CLASS_H */
