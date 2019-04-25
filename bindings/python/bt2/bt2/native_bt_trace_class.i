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

/* From trace-class-const.h */

typedef enum bt_trace_class_status {
	BT_TRACE_CLASS_STATUS_OK = 0,
	BT_TRACE_CLASS_STATUS_NOMEM = -12,
} bt_trace_class_status;

typedef void (* bt_trace_class_destruction_listener_func)(
		const bt_trace_class *trace_class, void *data);

extern bt_bool bt_trace_class_assigns_automatic_stream_class_id(
		const bt_trace_class *trace_class);

extern const char *bt_trace_class_get_name(
		const bt_trace_class *trace_class);

extern bt_uuid bt_trace_class_get_uuid(
		const bt_trace_class *trace_class);

extern uint64_t bt_trace_class_get_environment_entry_count(
		const bt_trace_class *trace_class);

extern void bt_trace_class_borrow_environment_entry_by_index_const(
		const bt_trace_class *trace_class, uint64_t index,
		const char **name, const bt_value **value);

extern const bt_value *
bt_trace_class_borrow_environment_entry_value_by_name_const(
		const bt_trace_class *trace_class, const char *name);

extern uint64_t bt_trace_class_get_stream_class_count(
		const bt_trace_class *trace_class);

extern const bt_stream_class *
bt_trace_class_borrow_stream_class_by_index_const(
		const bt_trace_class *trace_class, uint64_t index);

extern const bt_stream_class *bt_trace_class_borrow_stream_class_by_id_const(
		const bt_trace_class *trace_class, uint64_t id);

extern bt_trace_class_status bt_trace_class_add_destruction_listener(
        const bt_trace_class *trace_class,
        bt_trace_class_destruction_listener_func listener,
        void *data, uint64_t *listener_id);

extern bt_trace_class_status bt_trace_class_remove_destruction_listener(
        const bt_trace_class *trace_class, uint64_t listener_id);

extern void bt_trace_class_get_ref(const bt_trace_class *trace_class);

extern void bt_trace_class_put_ref(const bt_trace_class *trace_class);

/* From trace-class.h */

extern bt_trace_class *bt_trace_class_create(bt_self_component *self_comp);

extern void bt_trace_class_set_assigns_automatic_stream_class_id(
		bt_trace_class *trace_class, bt_bool value);

extern bt_trace_class_status bt_trace_class_set_name(
		bt_trace_class *trace_class, const char *name);

extern void bt_trace_class_set_uuid(bt_trace_class *trace_class,
		bt_uuid uuid);

extern bt_trace_class_status bt_trace_class_set_environment_entry_integer(
		bt_trace_class *trace_class,
		const char *name, int64_t value);

extern bt_trace_class_status bt_trace_class_set_environment_entry_string(
		bt_trace_class *trace_class,
		const char *name, const char *value);

extern bt_stream_class *bt_trace_class_borrow_stream_class_by_index(
		bt_trace_class *trace_class, uint64_t index);

extern bt_stream_class *bt_trace_class_borrow_stream_class_by_id(
		bt_trace_class *trace_class, uint64_t id);
