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

/* Types */

struct bt_trace;
struct bt_stream;
struct bt_stream_class;
struct bt_field_type;
struct bt_value;
struct bt_packet_header_field;

typedef enum bt_trace_class_status {
	BT_TRACE_CLASS_STATUS_OK = 0,
	BT_TRACE_CLASS_STATUS_NOMEM = -12,
} bt_trace_class_status;

typedef void (* bt_trace_is_static_listener)(
	struct bt_trace *trace, void *data);

typedef void (* bt_trace_listener_removed)(
	struct bt_trace *trace, void *data);

/* Functions */

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
		const char **BTOUTSTR, const bt_value **BTOUTVALUE);

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

extern void bt_trace_class_get_ref(const bt_trace_class *trace_class);

extern void bt_trace_class_put_ref(const bt_trace_class *trace_class);

/* Helper functions for Python */
%{
/*void trace_is_static_listener(const struct bt_trace *trace, void *py_callable)
{
	PyObject *py_trace_ptr = NULL;
	PyObject *py_res = NULL;

	py_trace_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(trace),
		SWIGTYPE_p_bt_trace, 0);
	if (!py_trace_ptr) {
		BT_LOGF_STR("Failed to create a SWIG pointer object.");
		abort();
	}

	py_res = PyObject_CallFunction(py_callable, "(O)", py_trace_ptr);
	BT_ASSERT(py_res == Py_None);
	Py_DECREF(py_trace_ptr);
	Py_DECREF(py_res);
}

void trace_listener_removed(const struct bt_trace *trace, void *py_callable)
{
	BT_ASSERT(py_callable);
	Py_DECREF(py_callable);
}

static uint64_t bt_py3_trace_add_is_static_listener(unsigned long long trace_addr,
		PyObject *py_callable)
{
	struct bt_trace *trace = (void *) trace_addr;
	int ret = 0;
	uint64_t id = 0;

	BT_ASSERT(trace);
	BT_ASSERT(py_callable);
	ret = bt_trace_add_is_static_listener(trace,
		trace_is_static_listener, trace_listener_removed, py_callable, &id);
	if (ret >= 0) {
		Py_INCREF(py_callable);
	} else if (ret < 0) {
		BT_LOGF_STR("Failed to add trace is static listener.");
		abort();
	}

	return ret;
}*/
%}

//int bt_py3_trace_add_is_static_listener(unsigned long long trace_addr,
//		PyObject *py_callable);
