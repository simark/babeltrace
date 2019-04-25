/*
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#define BT_LOG_TAG "TRACE"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/trace-ir/trace-class.h>
#include <babeltrace/trace-ir/trace-class-const.h>
#include <babeltrace/trace-ir/trace-internal.h>
#include <babeltrace/trace-ir/clock-class-internal.h>
#include <babeltrace/trace-ir/stream-internal.h>
#include <babeltrace/trace-ir/stream-class-internal.h>
#include <babeltrace/trace-ir/event-internal.h>
#include <babeltrace/trace-ir/event-class.h>
#include <babeltrace/trace-ir/event-class-internal.h>
#include <babeltrace/ctf-writer/functor-internal.h>
#include <babeltrace/ctf-writer/clock-internal.h>
#include <babeltrace/trace-ir/field-wrapper-internal.h>
#include <babeltrace/trace-ir/field-class-internal.h>
#include <babeltrace/trace-ir/attributes-internal.h>
#include <babeltrace/trace-ir/utils-internal.h>
#include <babeltrace/trace-ir/resolve-field-path-internal.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/value.h>
#include <babeltrace/value-const.h>
#include <babeltrace/value-internal.h>
#include <babeltrace/types.h>
#include <babeltrace/endian-internal.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/compat/glib-internal.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

struct bt_trace_class_destruction_listener_elem {
	bt_trace_class_destruction_listener_func func;
	void *data;
};

#define BT_ASSERT_PRE_TRACE_CLASS_HOT(_tc)				\
	BT_ASSERT_PRE_HOT((_tc), "Trace class", ": %!+T", (_tc))

static
void destroy_trace_class(struct bt_object *obj)
{
	struct bt_trace_class *tc = (void *) obj;

	BT_LIB_LOGD("Destroying trace class object: %!+T", tc);
	/*
	 * Call destruction listener functions so that everything else
	 * still exists in the trace class.
	 */
	if (tc->destruction_listeners) {
		uint64_t i;
		BT_LIB_LOGV("Calling trace class destruction listener(s): %!+T", tc);

		/*
		* The trace class' reference count is 0 if we're here. Increment
		* it to avoid a double-destroy (possibly infinitely recursive).
		* This could happen for example if a destruction listener did
		* bt_object_get_ref() (or anything that causes
		* bt_object_get_ref() to be called) on the trace class (ref.
		* count goes from 0 to 1), and then bt_object_put_ref(): the
		* reference count would go from 1 to 0 again and this function
		* would be called again.
		*/
		tc->base.ref_count++;

		/* Call all the trace class destruction listeners */
		for (i = 0; i < tc->destruction_listeners->len; i++) {
			struct bt_trace_class_destruction_listener_elem elem =
				g_array_index(tc->destruction_listeners,
						struct bt_trace_class_destruction_listener_elem, i);

			if (elem.func) {
				elem.func(tc, elem.data);
			}

			/* The destruction listener should not have kept a
			 * reference to the trace class.  */
			BT_ASSERT_PRE(tc->base.ref_count == 1, "Destruction listener kept a reference to the trace class being destroyed.");
		}
		g_array_free(tc->destruction_listeners, TRUE);
		tc->destruction_listeners = NULL;
	}

	if (tc->environment) {
		BT_LOGD_STR("Destroying environment attributes.");
		bt_attributes_destroy(tc->environment);
		tc->environment = NULL;
	}

	if (tc->name.str) {
		g_string_free(tc->name.str, TRUE);
		tc->name.str = NULL;
		tc->name.value = NULL;
	}

	if (tc->stream_classes) {
		BT_LOGD_STR("Destroying stream classes.");
		g_ptr_array_free(tc->stream_classes, TRUE);
		tc->stream_classes = NULL;
	}

	g_free(tc);
}

struct bt_trace_class *bt_trace_class_create(bt_self_component *self_comp)
{
	struct bt_trace_class *tc = NULL;

	BT_ASSERT_PRE_NON_NULL(self_comp, "Self component");
	BT_LOGD_STR("Creating default trace class object.");
	tc = g_new0(struct bt_trace_class, 1);
	if (!tc) {
		BT_LOGE_STR("Failed to allocate one trace class.");
		goto error;
	}

	bt_object_init_shared_with_parent(&tc->base, destroy_trace_class);

	tc->stream_classes = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_object_try_spec_release);
	if (!tc->stream_classes) {
		BT_LOGE_STR("Failed to allocate one GPtrArray.");
		goto error;
	}

	tc->name.str = g_string_new(NULL);
	if (!tc->name.str) {
		BT_LOGE_STR("Failed to allocate one GString.");
		goto error;
	}

	tc->environment = bt_attributes_create();
	if (!tc->environment) {
		BT_LOGE_STR("Cannot create empty attributes object.");
		goto error;
	}

	tc->destruction_listeners = g_array_new(FALSE, TRUE,
		sizeof(struct bt_trace_class_destruction_listener_elem));
	if (!tc->destruction_listeners) {
		BT_LOGE_STR("Failed to allocate one GArray.");
		goto error;
	}

	tc->assigns_automatic_stream_class_id = true;
	BT_LIB_LOGD("Created trace class object: %!+T", tc);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(tc);

end:
	return tc;
}

const char *bt_trace_class_get_name(const struct bt_trace_class *tc)
{
	BT_ASSERT_PRE_NON_NULL(tc, "Trace class");
	return tc->name.value;
}

enum bt_trace_class_status bt_trace_class_set_name(
		struct bt_trace_class *tc, const char *name)
{
	BT_ASSERT_PRE_NON_NULL(tc, "Trace class");
	BT_ASSERT_PRE_NON_NULL(name, "Name");
	BT_ASSERT_PRE_TRACE_CLASS_HOT(tc);
	g_string_assign(tc->name.str, name);
	tc->name.value = tc->name.str->str;
	BT_LIB_LOGV("Set trace class's name: %!+T", tc);
	return BT_TRACE_CLASS_STATUS_OK;
}

bt_uuid bt_trace_class_get_uuid(const struct bt_trace_class *tc)
{
	BT_ASSERT_PRE_NON_NULL(tc, "Trace class");
	return tc->uuid.value;
}

void bt_trace_class_set_uuid(struct bt_trace_class *tc, bt_uuid uuid)
{
	BT_ASSERT_PRE_NON_NULL(tc, "Trace class");
	BT_ASSERT_PRE_NON_NULL(uuid, "UUID");
	BT_ASSERT_PRE_TRACE_CLASS_HOT(tc);
	memcpy(tc->uuid.uuid, uuid, BABELTRACE_UUID_LEN);
	tc->uuid.value = tc->uuid.uuid;
	BT_LIB_LOGV("Set trace class's UUID: %!+T", tc);
}

enum bt_trace_class_status bt_trace_class_add_destruction_listener(
		const struct bt_trace_class *_tc,
		bt_trace_class_destruction_listener_func listener,
		void *data, uint64_t *listener_id)
{
	struct bt_trace_class *tc = (void *) _tc;
	uint64_t i;
	struct bt_trace_class_destruction_listener_elem new_elem = {
		.func = listener,
		.data = data,
	};

	BT_ASSERT_PRE_NON_NULL(tc, "Trace class");
	BT_ASSERT_PRE_NON_NULL(listener, "Listener");

	/* Find the next available spot */
	for (i = 0; i < tc->destruction_listeners->len; i++) {
		struct bt_trace_class_destruction_listener_elem elem =
			g_array_index(tc->destruction_listeners,
				struct bt_trace_class_destruction_listener_elem, i);

		if (!elem.func) {
			break;
		}
	}

	if (i == tc->destruction_listeners->len) {
		g_array_append_val(tc->destruction_listeners, new_elem);
	} else {
		g_array_insert_val(tc->destruction_listeners, i, new_elem);
	}

	if (listener_id) {
		*listener_id = i;
	}

	BT_LIB_LOGV("Added trace class destruction listener: %![tc-]+T, "
			"listener-id=%" PRIu64, tc, i);
	return BT_TRACE_CLASS_STATUS_OK;
}

BT_ASSERT_PRE_FUNC
static
bool has_listener_id(const struct bt_trace_class *tc, uint64_t listener_id)
{
	BT_ASSERT(listener_id < tc->destruction_listeners->len);
	return (&g_array_index(tc->destruction_listeners,
			struct bt_trace_class_destruction_listener_elem,
			listener_id))->func != NULL;
}

enum bt_trace_class_status bt_trace_class_remove_destruction_listener(
		const struct bt_trace_class *_tc, uint64_t listener_id)
{
	struct bt_trace_class *tc = (void *) _tc;
	struct bt_trace_class_destruction_listener_elem *elem;

	BT_ASSERT_PRE_NON_NULL(tc, "Trace class");
	BT_ASSERT_PRE(has_listener_id(tc, listener_id),
		"Trace class has no such trace class destruction listener ID: "
		"%![tc-]+T, %" PRIu64, tc, listener_id);
	elem = &g_array_index(tc->destruction_listeners,
			struct bt_trace_class_destruction_listener_elem,
			listener_id);
	BT_ASSERT(elem->func);

	elem->func = NULL;
	elem->data = NULL;
	BT_LIB_LOGV("Removed trace class destruction listener: "
		"%![tc-]+T, listener-id=%" PRIu64,
		tc, listener_id);
	return BT_TRACE_CLASS_STATUS_OK;
}

BT_ASSERT_FUNC
static
bool trace_has_environment_entry(const struct bt_trace_class *tc, const char *name)
{
	BT_ASSERT(tc);

	return bt_attributes_borrow_field_value_by_name(
		tc->environment, name) != NULL;
}

static
enum bt_trace_class_status set_environment_entry(struct bt_trace_class *tc,
		const char *name, struct bt_value *value)
{
	int ret;

	BT_ASSERT(tc);
	BT_ASSERT(name);
	BT_ASSERT(value);
	BT_ASSERT_PRE(!tc->frozen ||
		!trace_has_environment_entry(tc, name),
		"Trace class is frozen: cannot replace environment entry: "
		"%![tc-]+T, entry-name=\"%s\"", tc, name);
	ret = bt_attributes_set_field_value(tc->environment, name,
		value);
	if (ret) {
		ret = BT_TRACE_CLASS_STATUS_NOMEM;
		BT_LIB_LOGE("Cannot set trace class's environment entry: "
			"%![tc-]+T, entry-name=\"%s\"", tc, name);
	} else {
		bt_value_freeze(value);
		BT_LIB_LOGV("Set trace class's environment entry: "
			"%![tc-]+T, entry-name=\"%s\"", tc, name);
	}

	return ret;
}

enum bt_trace_class_status bt_trace_class_set_environment_entry_string(
		struct bt_trace_class *tc, const char *name, const char *value)
{
	int ret;
	struct bt_value *value_obj;
	BT_ASSERT_PRE_NON_NULL(tc, "Trace class");
	BT_ASSERT_PRE_NON_NULL(name, "Name");
	BT_ASSERT_PRE_NON_NULL(value, "Value");
	value_obj = bt_value_string_create_init(value);
	if (!value_obj) {
		BT_LOGE_STR("Cannot create a string value object.");
		ret = -1;
		goto end;
	}

	/* set_environment_entry() logs errors */
	ret = set_environment_entry(tc, name, value_obj);

end:
	bt_object_put_ref(value_obj);
	return ret;
}

enum bt_trace_class_status bt_trace_class_set_environment_entry_integer(
		struct bt_trace_class *tc, const char *name, int64_t value)
{
	int ret;
	struct bt_value *value_obj;
	BT_ASSERT_PRE_NON_NULL(tc, "Trace class");
	BT_ASSERT_PRE_NON_NULL(name, "Name");
	value_obj = bt_value_integer_create_init(value);
	if (!value_obj) {
		BT_LOGE_STR("Cannot create an integer value object.");
		ret = BT_TRACE_CLASS_STATUS_NOMEM;
		goto end;
	}

	/* set_environment_entry() logs errors */
	ret = set_environment_entry(tc, name, value_obj);

end:
	bt_object_put_ref(value_obj);
	return ret;
}

uint64_t bt_trace_class_get_environment_entry_count(const struct bt_trace_class *tc)
{
	int64_t ret;

	BT_ASSERT_PRE_NON_NULL(tc, "Trace class");
	ret = bt_attributes_get_count(tc->environment);
	BT_ASSERT(ret >= 0);
	return (uint64_t) ret;
}

void bt_trace_class_borrow_environment_entry_by_index_const(
		const struct bt_trace_class *tc, uint64_t index,
		const char **name, const struct bt_value **value)
{
	BT_ASSERT_PRE_NON_NULL(tc, "Trace class");
	BT_ASSERT_PRE_NON_NULL(name, "Name");
	BT_ASSERT_PRE_NON_NULL(value, "Value");
	BT_ASSERT_PRE_VALID_INDEX(index,
		bt_attributes_get_count(tc->environment));
	*value = bt_attributes_borrow_field_value(tc->environment, index);
	BT_ASSERT(*value);
	*name = bt_attributes_get_field_name(tc->environment, index);
	BT_ASSERT(*name);
}

const struct bt_value *bt_trace_class_borrow_environment_entry_value_by_name_const(
		const struct bt_trace_class *tc, const char *name)
{
	BT_ASSERT_PRE_NON_NULL(tc, "Trace class");
	BT_ASSERT_PRE_NON_NULL(name, "Name");
	return bt_attributes_borrow_field_value_by_name(tc->environment,
		name);
}

uint64_t bt_trace_class_get_stream_class_count(const struct bt_trace_class *tc)
{
	BT_ASSERT_PRE_NON_NULL(tc, "Trace class");
	return (uint64_t) tc->stream_classes->len;
}

struct bt_stream_class *bt_trace_class_borrow_stream_class_by_index(
		struct bt_trace_class *tc, uint64_t index)
{
	BT_ASSERT_PRE_NON_NULL(tc, "Trace class");
	BT_ASSERT_PRE_VALID_INDEX(index, tc->stream_classes->len);
	return g_ptr_array_index(tc->stream_classes, index);
}

const struct bt_stream_class *
bt_trace_class_borrow_stream_class_by_index_const(
		const struct bt_trace_class *tc, uint64_t index)
{
	return bt_trace_class_borrow_stream_class_by_index(
		(void *) tc, index);
}

struct bt_stream_class *bt_trace_class_borrow_stream_class_by_id(
		struct bt_trace_class *tc, uint64_t id)
{
	struct bt_stream_class *stream_class = NULL;
	uint64_t i;

	BT_ASSERT_PRE_NON_NULL(tc, "Trace class");

	for (i = 0; i < tc->stream_classes->len; i++) {
		struct bt_stream_class *stream_class_candidate =
			g_ptr_array_index(tc->stream_classes, i);

		if (stream_class_candidate->id == id) {
			stream_class = stream_class_candidate;
			goto end;
		}
	}

end:
	return stream_class;
}

const struct bt_stream_class *
bt_trace_class_borrow_stream_class_by_id_const(
		const struct bt_trace_class *tc, uint64_t id)
{
	return bt_trace_class_borrow_stream_class_by_id((void *) tc, id);
}

BT_HIDDEN
void _bt_trace_class_freeze(const struct bt_trace_class *tc)
{
	BT_ASSERT(tc);
	BT_LIB_LOGD("Freezing trace class: %!+T", tc);
	((struct bt_trace_class *) tc)->frozen = true;
}

bt_bool bt_trace_class_assigns_automatic_stream_class_id(const struct bt_trace_class *tc)
{
	BT_ASSERT_PRE_NON_NULL(tc, "Trace class");
	return (bt_bool) tc->assigns_automatic_stream_class_id;
}

void bt_trace_class_set_assigns_automatic_stream_class_id(struct bt_trace_class *tc,
		bt_bool value)
{
	BT_ASSERT_PRE_NON_NULL(tc, "Trace class");
	BT_ASSERT_PRE_TRACE_CLASS_HOT(tc);
	tc->assigns_automatic_stream_class_id = (bool) value;
	BT_LIB_LOGV("Set trace class's automatic stream class ID "
		"assignment property: %!+T", tc);
}

void bt_trace_class_get_ref(const struct bt_trace_class *trace_class)
{
	bt_object_get_ref(trace_class);
}

void bt_trace_class_put_ref(const struct bt_trace_class *trace_class)
{
	bt_object_put_ref(trace_class);
}
