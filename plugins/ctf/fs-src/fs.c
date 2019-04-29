/*
 * fs.c
 *
 * Babeltrace CTF file system Reader Component
 *
 * Copyright 2015-2017 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace/common-internal.h>
#include <babeltrace/babeltrace.h>
#include <babeltrace/compat/uuid-internal.h>
#include <plugins-common.h>
#include <glib.h>
#include <babeltrace/assert-internal.h>
#include <inttypes.h>
#include <stdbool.h>
#include "fs.h"
#include "metadata.h"
#include "data-stream-file.h"
#include "file.h"
#include "../common/metadata/decoder.h"
#include "../common/msg-iter/msg-iter.h"
#include "../common/utils/utils.h"
#include "query.h"

#define BT_LOG_TAG "PLUGIN-CTF-FS-SRC"
#include "logging.h"

static
int msg_iter_data_set_current_ds_file(struct ctf_fs_msg_iter_data *msg_iter_data)
{
	struct ctf_fs_ds_file_info *ds_file_info;
	int ret = 0;

	BT_ASSERT(msg_iter_data->ds_file_info_index <
		msg_iter_data->ds_file_group->ds_file_infos->len);
	ds_file_info = g_ptr_array_index(
		msg_iter_data->ds_file_group->ds_file_infos,
		msg_iter_data->ds_file_info_index);

	ctf_fs_ds_file_destroy(msg_iter_data->ds_file);
	msg_iter_data->ds_file = ctf_fs_ds_file_create(
		msg_iter_data->ds_file_group->ctf_fs_trace,
		msg_iter_data->pc_msg_iter,
		msg_iter_data->msg_iter,
		msg_iter_data->ds_file_group->stream,
		ds_file_info->path->str);
	if (!msg_iter_data->ds_file) {
		ret = -1;
	}

	return ret;
}

static
void ctf_fs_msg_iter_data_destroy(
		struct ctf_fs_msg_iter_data *msg_iter_data)
{
	if (!msg_iter_data) {
		return;
	}

	ctf_fs_ds_file_destroy(msg_iter_data->ds_file);

	if (msg_iter_data->msg_iter) {
		bt_msg_iter_destroy(msg_iter_data->msg_iter);
	}

	g_free(msg_iter_data);
}

static
void set_msg_iter_emits_stream_beginning_end_messages(
		struct ctf_fs_msg_iter_data *msg_iter_data)
{
	bt_msg_iter_set_emit_stream_beginning_message(
		msg_iter_data->ds_file->msg_iter,
		msg_iter_data->ds_file_info_index == 0);
	bt_msg_iter_set_emit_stream_end_message(
		msg_iter_data->ds_file->msg_iter,
		msg_iter_data->ds_file_info_index ==
			msg_iter_data->ds_file_group->ds_file_infos->len - 1);
}

static
bt_self_message_iterator_status ctf_fs_iterator_next_one(
		struct ctf_fs_msg_iter_data *msg_iter_data,
		const bt_message **out_msg)
{
	bt_self_message_iterator_status status;

	BT_ASSERT(msg_iter_data->ds_file);

	while (true) {
		bt_message *msg;

		status = ctf_fs_ds_file_next(msg_iter_data->ds_file, &msg);
		switch (status) {
		case BT_SELF_MESSAGE_ITERATOR_STATUS_OK:
			*out_msg = msg;
			msg = NULL;
			goto end;
		case BT_SELF_MESSAGE_ITERATOR_STATUS_END:
		{
			int ret;

			if (msg_iter_data->ds_file_info_index ==
					msg_iter_data->ds_file_group->ds_file_infos->len - 1) {
				/* End of all group's stream files */
				goto end;
			}

			msg_iter_data->ds_file_info_index++;
			bt_msg_iter_reset_for_next_stream_file(
				msg_iter_data->msg_iter);
			set_msg_iter_emits_stream_beginning_end_messages(
				msg_iter_data);

			/*
			 * Open and start reading the next stream file
			 * within our stream file group.
			 */
			ret = msg_iter_data_set_current_ds_file(msg_iter_data);
			if (ret) {
				status = BT_SELF_MESSAGE_ITERATOR_STATUS_ERROR;
				goto end;
			}

			/* Continue the loop to get the next message */
			break;
		}
		default:
			goto end;
		}
	}

end:
	return status;
}

BT_HIDDEN
bt_self_message_iterator_status ctf_fs_iterator_next(
		bt_self_message_iterator *iterator,
		bt_message_array_const msgs, uint64_t capacity,
		uint64_t *count)
{
	bt_self_message_iterator_status status =
		BT_SELF_MESSAGE_ITERATOR_STATUS_OK;
	struct ctf_fs_msg_iter_data *msg_iter_data =
		bt_self_message_iterator_get_data(iterator);
	uint64_t i = 0;

	while (i < capacity && status == BT_SELF_MESSAGE_ITERATOR_STATUS_OK) {
		status = ctf_fs_iterator_next_one(msg_iter_data, &msgs[i]);
		if (status == BT_SELF_MESSAGE_ITERATOR_STATUS_OK) {
			i++;
		}
	}

	if (i > 0) {
		/*
		 * Even if ctf_fs_iterator_next_one() returned something
		 * else than BT_SELF_MESSAGE_ITERATOR_STATUS_OK, we
		 * accumulated message objects in the output
		 * message array, so we need to return
		 * BT_SELF_MESSAGE_ITERATOR_STATUS_OK so that they are
		 * transfered to downstream. This other status occurs
		 * again the next time muxer_msg_iter_do_next() is
		 * called, possibly without any accumulated
		 * message, in which case we'll return it.
		 */
		*count = i;
		status = BT_SELF_MESSAGE_ITERATOR_STATUS_OK;
	}

	return status;
}

static
int ctf_fs_iterator_reset(struct ctf_fs_msg_iter_data *msg_iter_data)
{
	int ret;

	msg_iter_data->ds_file_info_index = 0;
	ret = msg_iter_data_set_current_ds_file(msg_iter_data);
	if (ret) {
		goto end;
	}

	bt_msg_iter_reset(msg_iter_data->msg_iter);
	set_msg_iter_emits_stream_beginning_end_messages(msg_iter_data);

end:
	return ret;
}

BT_HIDDEN
bt_self_message_iterator_status ctf_fs_iterator_seek_beginning(
		bt_self_message_iterator *it)
{
	struct ctf_fs_msg_iter_data *msg_iter_data =
		bt_self_message_iterator_get_data(it);
	bt_self_message_iterator_status status =
		BT_SELF_MESSAGE_ITERATOR_STATUS_OK;

	BT_ASSERT(msg_iter_data);
	if (ctf_fs_iterator_reset(msg_iter_data)) {
		status = BT_SELF_MESSAGE_ITERATOR_STATUS_ERROR;
	}

	return status;
}

BT_HIDDEN
void ctf_fs_iterator_finalize(bt_self_message_iterator *it)
{
	ctf_fs_msg_iter_data_destroy(
		bt_self_message_iterator_get_data(it));
}

BT_HIDDEN
bt_self_message_iterator_status ctf_fs_iterator_init(
		bt_self_message_iterator *self_msg_iter,
		bt_self_component_source *self_comp,
		bt_self_component_port_output *self_port)
{
	struct ctf_fs_port_data *port_data;
	struct ctf_fs_msg_iter_data *msg_iter_data = NULL;
	bt_self_message_iterator_status ret =
		BT_SELF_MESSAGE_ITERATOR_STATUS_OK;

	port_data = bt_self_component_port_get_data(
		bt_self_component_port_output_as_self_component_port(
			self_port));
	BT_ASSERT(port_data);
	msg_iter_data = g_new0(struct ctf_fs_msg_iter_data, 1);
	if (!msg_iter_data) {
		ret = BT_SELF_MESSAGE_ITERATOR_STATUS_NOMEM;
		goto error;
	}

	msg_iter_data->pc_msg_iter = self_msg_iter;
	msg_iter_data->msg_iter = bt_msg_iter_create(
		port_data->ds_file_group->ctf_fs_trace->metadata->tc,
		bt_common_get_page_size() * 8,
		ctf_fs_ds_file_medops, NULL);
	if (!msg_iter_data->msg_iter) {
		BT_LOGE_STR("Cannot create a CTF message iterator.");
		ret = BT_SELF_MESSAGE_ITERATOR_STATUS_NOMEM;
		goto error;
	}

	msg_iter_data->ds_file_group = port_data->ds_file_group;
	if (ctf_fs_iterator_reset(msg_iter_data)) {
		ret = BT_SELF_MESSAGE_ITERATOR_STATUS_ERROR;
		goto error;
	}

	bt_self_message_iterator_set_data(self_msg_iter,
		msg_iter_data);
	if (ret != BT_SELF_MESSAGE_ITERATOR_STATUS_OK) {
		goto error;
	}

	msg_iter_data = NULL;
	goto end;

error:
	bt_self_message_iterator_set_data(self_msg_iter, NULL);

end:
	ctf_fs_msg_iter_data_destroy(msg_iter_data);
	return ret;
}

void ctf_fs_destroy(struct ctf_fs_component *ctf_fs)
{
	if (!ctf_fs) {
		return;
	}

	if (ctf_fs->traces) {
		g_ptr_array_free(ctf_fs->traces, TRUE);
	}

	if (ctf_fs->port_data) {
		g_ptr_array_free(ctf_fs->port_data, TRUE);
	}

	g_free(ctf_fs);
}

static void
port_data_destroy(struct ctf_fs_port_data *port_data)
{
	if (!port_data) {
		return;
	}

	g_free(port_data);
}

static
void port_data_destroy_notifier(void *data) {
	struct ctf_fs_port_data *port_data = data;
	port_data_destroy(port_data);
}


static
void ctf_fs_trace_destroy(struct ctf_fs_trace *ctf_fs_trace)
{
	if (!ctf_fs_trace) {
		return;
	}

	if (ctf_fs_trace->ds_file_groups) {
		g_ptr_array_free(ctf_fs_trace->ds_file_groups, TRUE);
	}

	BT_TRACE_PUT_REF_AND_RESET(ctf_fs_trace->trace);

	if (ctf_fs_trace->path) {
		g_string_free(ctf_fs_trace->path, TRUE);
	}

	if (ctf_fs_trace->name) {
		g_string_free(ctf_fs_trace->name, TRUE);
	}

	if (ctf_fs_trace->metadata) {
		ctf_fs_metadata_fini(ctf_fs_trace->metadata);
		g_free(ctf_fs_trace->metadata);
	}

	g_free(ctf_fs_trace);
}

static
void ctf_fs_trace_destroy_notifier(void *data)
{
	struct ctf_fs_trace *trace = data;
	ctf_fs_trace_destroy(trace);
}

struct ctf_fs_component *ctf_fs_new(void)
{
	struct ctf_fs_component *ctf_fs = g_new0(struct ctf_fs_component, 1);
	if (!ctf_fs) {
		return NULL;
	}

	ctf_fs->port_data =
		g_ptr_array_new_with_free_func(port_data_destroy_notifier);
	if (!ctf_fs->port_data) {
		goto error;
	}

	ctf_fs->traces =
		g_ptr_array_new_with_free_func(ctf_fs_trace_destroy_notifier);
	if (!ctf_fs->traces) {
		goto error;
	}

	return ctf_fs;

error:
	ctf_fs_destroy(ctf_fs);
	return NULL;
}

void ctf_fs_finalize(bt_self_component_source *component)
{
	ctf_fs_destroy(bt_self_component_get_data(
		bt_self_component_source_as_self_component(component)));
}

static
GString *get_stream_instance_unique_name(
		struct ctf_fs_ds_file_group *ds_file_group)
{
	GString *name;
	struct ctf_fs_ds_file_info *ds_file_info;

	name = g_string_new(NULL);
	if (!name) {
		goto end;
	}

	/*
	 * If there's more than one stream file in the stream file
	 * group, the first (earliest) stream file's path is used as
	 * the stream's unique name.
	 */
	BT_ASSERT(ds_file_group->ds_file_infos->len > 0);
	ds_file_info = g_ptr_array_index(ds_file_group->ds_file_infos, 0);
	g_string_assign(name, ds_file_info->path->str);

end:
	return name;
}

static
int create_one_port_for_trace(struct ctf_fs_component *ctf_fs,
		struct ctf_fs_trace *ctf_fs_trace,
		struct ctf_fs_ds_file_group *ds_file_group)
{
	int ret = 0;
	struct ctf_fs_port_data *port_data = NULL;
	GString *port_name = NULL;

	port_name = get_stream_instance_unique_name(ds_file_group);
	if (!port_name) {
		goto error;
	}

	BT_LOGD("Creating one port named `%s`", port_name->str);

	/* Create output port for this file */
	port_data = g_new0(struct ctf_fs_port_data, 1);
	if (!port_data) {
		goto error;
	}

	port_data->ctf_fs = ctf_fs;
	port_data->ds_file_group = ds_file_group;
	ret = bt_self_component_source_add_output_port(
		ctf_fs->self_comp, port_name->str, port_data, NULL);
	if (ret) {
		goto error;
	}

	g_ptr_array_add(ctf_fs->port_data, port_data);
	port_data = NULL;
	goto end;

error:
	ret = -1;

end:
	if (port_name) {
		g_string_free(port_name, TRUE);
	}

	port_data_destroy(port_data);
	return ret;
}

static
int create_ports_for_trace(struct ctf_fs_component *ctf_fs,
		struct ctf_fs_trace *ctf_fs_trace)
{
	int ret = 0;
	size_t i;

	/* Create one output port for each stream file group */
	for (i = 0; i < ctf_fs_trace->ds_file_groups->len; i++) {
		struct ctf_fs_ds_file_group *ds_file_group =
			g_ptr_array_index(ctf_fs_trace->ds_file_groups, i);

		ret = create_one_port_for_trace(ctf_fs, ctf_fs_trace,
			ds_file_group);
		if (ret) {
			BT_LOGE("Cannot create output port.");
			goto end;
		}
	}

end:
	return ret;
}

static
void ctf_fs_ds_file_info_destroy(struct ctf_fs_ds_file_info *ds_file_info)
{
	if (!ds_file_info) {
		return;
	}

	if (ds_file_info->path) {
		g_string_free(ds_file_info->path, TRUE);
	}

	ctf_fs_ds_index_destroy(ds_file_info->index);
	g_free(ds_file_info);
}

static
struct ctf_fs_ds_file_info *ctf_fs_ds_file_info_create(const char *path,
		int64_t begin_ns, struct ctf_fs_ds_index *index)
{
	struct ctf_fs_ds_file_info *ds_file_info;

	ds_file_info = g_new0(struct ctf_fs_ds_file_info, 1);
	if (!ds_file_info) {
		goto end;
	}

	ds_file_info->path = g_string_new(path);
	if (!ds_file_info->path) {
		ctf_fs_ds_file_info_destroy(ds_file_info);
		ds_file_info = NULL;
		goto end;
	}

	ds_file_info->begin_ns = begin_ns;
	ds_file_info->index = index;
	index = NULL;

end:
	ctf_fs_ds_index_destroy(index);
	return ds_file_info;
}

static
void ctf_fs_ds_file_group_destroy(struct ctf_fs_ds_file_group *ds_file_group)
{
	if (!ds_file_group) {
		return;
	}

	if (ds_file_group->ds_file_infos) {
		g_ptr_array_free(ds_file_group->ds_file_infos, TRUE);
	}

	bt_stream_put_ref(ds_file_group->stream);
	g_free(ds_file_group);
}

static
struct ctf_fs_ds_file_group *ctf_fs_ds_file_group_create(
		struct ctf_fs_trace *ctf_fs_trace,
		struct ctf_stream_class *sc,
		uint64_t stream_instance_id)
{
	struct ctf_fs_ds_file_group *ds_file_group;

	ds_file_group = g_new0(struct ctf_fs_ds_file_group, 1);
	if (!ds_file_group) {
		goto error;
	}

	ds_file_group->ds_file_infos = g_ptr_array_new_with_free_func(
		(GDestroyNotify) ctf_fs_ds_file_info_destroy);
	if (!ds_file_group->ds_file_infos) {
		goto error;
	}

	ds_file_group->stream_id = stream_instance_id;
	BT_ASSERT(sc);
	ds_file_group->sc = sc;
	ds_file_group->ctf_fs_trace = ctf_fs_trace;
	goto end;

error:
	ctf_fs_ds_file_group_destroy(ds_file_group);
	ds_file_group = NULL;

end:
	return ds_file_group;
}

/* Replace by g_ptr_array_insert when we depend on glib >= 2.40. */
static
void array_insert(GPtrArray *array, gpointer element, size_t pos)
{
	size_t original_array_len = array->len;

	/* Allocate an unused element at the end of the array. */
	g_ptr_array_add(array, NULL);

	/* If we are not inserting at the end, move the elements by one. */
	if (pos < original_array_len) {
		memmove(&(array->pdata[pos + 1]),
			&(array->pdata[pos]),
			(original_array_len - pos) * sizeof(gpointer));
	}

	/* Insert the value and bump the array len */
	array->pdata[pos] = element;
}

/* Insert ds_file_info in ds_file_group's list of ds_file_infos at the right
   place to keep it sorted. */

static
void ds_file_group_insert_ds_file_info_sorted(
		struct ctf_fs_ds_file_group *ds_file_group,
		struct ctf_fs_ds_file_info *ds_file_info)
{
	guint i;

	/* Find the spot where to insert this ds_file_info. */
	for (i = 0; i < ds_file_group->ds_file_infos->len; i++) {
		struct ctf_fs_ds_file_info *other_ds_file_info =
			g_ptr_array_index(ds_file_group->ds_file_infos, i);

		if (ds_file_info->begin_ns < other_ds_file_info->begin_ns) {
			break;
		}
	}

	array_insert(ds_file_group->ds_file_infos, ds_file_info, i);
}

/* Create a new ds_file_info using the provided path, begin_ns and index, then
   add it to ds_file_group's list of ds_file_infos. */

static
int ctf_fs_ds_file_group_add_ds_file_info(
		struct ctf_fs_ds_file_group *ds_file_group,
		const char *path, int64_t begin_ns,
		struct ctf_fs_ds_index *index)
{
	struct ctf_fs_ds_file_info *ds_file_info;
	int ret = 0;

	/* Onwership of index is transferred. */
	ds_file_info = ctf_fs_ds_file_info_create(path, begin_ns, index);
	index = NULL;
	if (!ds_file_info) {
		goto error;
	}

	ds_file_group_insert_ds_file_info_sorted(ds_file_group, ds_file_info);

	ds_file_info = NULL;
	goto end;

error:
	ctf_fs_ds_file_info_destroy(ds_file_info);
	ctf_fs_ds_index_destroy(index);
	ret = -1;
end:
	return ret;
}

static
int add_ds_file_to_ds_file_group(struct ctf_fs_trace *ctf_fs_trace,
		const char *path)
{
	int64_t stream_instance_id = -1;
	int64_t begin_ns = -1;
	struct ctf_fs_ds_file_group *ds_file_group = NULL;
	bool add_group = false;
	int ret;
	size_t i;
	struct ctf_fs_ds_file *ds_file = NULL;
	struct ctf_fs_ds_index *index = NULL;
	struct bt_msg_iter *msg_iter = NULL;
	struct ctf_stream_class *sc = NULL;
	struct bt_msg_iter_packet_properties props;

	msg_iter = bt_msg_iter_create(ctf_fs_trace->metadata->tc,
		bt_common_get_page_size() * 8, ctf_fs_ds_file_medops, NULL);
	if (!msg_iter) {
		BT_LOGE_STR("Cannot create a CTF message iterator.");
		goto error;
	}

	ds_file = ctf_fs_ds_file_create(ctf_fs_trace, NULL, msg_iter,
		NULL, path);
	if (!ds_file) {
		goto error;
	}

	ret = bt_msg_iter_get_packet_properties(ds_file->msg_iter, &props);
	if (ret) {
		BT_LOGE("Cannot get stream file's first packet's header and context fields (`%s`).",
			path);
		goto error;
	}

	sc = ctf_trace_class_borrow_stream_class_by_id(ds_file->metadata->tc,
		props.stream_class_id);
	BT_ASSERT(sc);
	stream_instance_id = props.data_stream_id;

	if (props.snapshots.beginning_clock != UINT64_C(-1)) {
		BT_ASSERT(sc->default_clock_class);
		ret = bt_util_clock_cycles_to_ns_from_origin(
			props.snapshots.beginning_clock,
			sc->default_clock_class->frequency,
			sc->default_clock_class->offset_seconds,
			sc->default_clock_class->offset_cycles, &begin_ns);
		if (ret) {
			BT_LOGE("Cannot convert clock cycles to nanoseconds from origin (`%s`).",
				path);
			goto error;
		}
	}

	index = ctf_fs_ds_file_build_index(ds_file);
	if (!index) {
		BT_LOGW("Failed to index CTF stream file \'%s\'",
			ds_file->file->path->str);
	}

	if (begin_ns == -1) {
		/*
		 * No beggining timestamp to sort the stream files
		 * within a stream file group, so consider that this
		 * file must be the only one within its group.
		 */
		stream_instance_id = -1;
	}

	if (stream_instance_id == -1) {
		/*
		 * No stream instance ID or no beginning timestamp:
		 * create a unique stream file group for this stream
		 * file because, even if there's a stream instance ID,
		 * there's no timestamp to order the file within its
		 * group.
		 */
		ds_file_group = ctf_fs_ds_file_group_create(ctf_fs_trace,
			sc, UINT64_C(-1));
		if (!ds_file_group) {
			goto error;
		}

		ret = ctf_fs_ds_file_group_add_ds_file_info(ds_file_group,
			path, begin_ns, index);
		/* Ownership of index is transferred. */
		index = NULL;
		if (ret) {
			goto error;
		}

		add_group = true;
		goto end;
	}

	BT_ASSERT(stream_instance_id != -1);
	BT_ASSERT(begin_ns != -1);

	/* Find an existing stream file group with this ID */
	for (i = 0; i < ctf_fs_trace->ds_file_groups->len; i++) {
		ds_file_group = g_ptr_array_index(
			ctf_fs_trace->ds_file_groups, i);

		if (ds_file_group->sc == sc &&
				ds_file_group->stream_id ==
				stream_instance_id) {
			break;
		}

		ds_file_group = NULL;
	}

	if (!ds_file_group) {
		ds_file_group = ctf_fs_ds_file_group_create(ctf_fs_trace,
			sc, stream_instance_id);
		if (!ds_file_group) {
			goto error;
		}

		add_group = true;
	}

	ret = ctf_fs_ds_file_group_add_ds_file_info(ds_file_group, path,
		begin_ns, index);
	index = NULL;
	if (ret) {
		goto error;
	}

	goto end;

error:
	ctf_fs_ds_file_group_destroy(ds_file_group);
	ret = -1;

end:
	if (add_group && ds_file_group) {
		g_ptr_array_add(ctf_fs_trace->ds_file_groups, ds_file_group);
	}

	ctf_fs_ds_file_destroy(ds_file);

	if (msg_iter) {
		bt_msg_iter_destroy(msg_iter);
	}

	ctf_fs_ds_index_destroy(index);
	return ret;
}

static
int create_ds_file_groups(struct ctf_fs_trace *ctf_fs_trace)
{
	int ret = 0;
	const char *basename;
	GError *error = NULL;
	GDir *dir = NULL;

	/* Check each file in the path directory, except specific ones */
	dir = g_dir_open(ctf_fs_trace->path->str, 0, &error);
	if (!dir) {
		BT_LOGE("Cannot open directory `%s`: %s (code %d)",
			ctf_fs_trace->path->str, error->message,
			error->code);
		goto error;
	}

	while ((basename = g_dir_read_name(dir))) {
		struct ctf_fs_file *file;

		if (!strcmp(basename, CTF_FS_METADATA_FILENAME)) {
			/* Ignore the metadata stream. */
			BT_LOGD("Ignoring metadata file `%s" G_DIR_SEPARATOR_S "%s`",
				ctf_fs_trace->path->str, basename);
			continue;
		}

		if (basename[0] == '.') {
			BT_LOGD("Ignoring hidden file `%s" G_DIR_SEPARATOR_S "%s`",
				ctf_fs_trace->path->str, basename);
			continue;
		}

		/* Create the file. */
		file = ctf_fs_file_create();
		if (!file) {
			BT_LOGE("Cannot create stream file object for file `%s" G_DIR_SEPARATOR_S "%s`",
				ctf_fs_trace->path->str, basename);
			goto error;
		}

		/* Create full path string. */
		g_string_append_printf(file->path, "%s" G_DIR_SEPARATOR_S "%s",
				ctf_fs_trace->path->str, basename);
		if (!g_file_test(file->path->str, G_FILE_TEST_IS_REGULAR)) {
			BT_LOGD("Ignoring non-regular file `%s`",
				file->path->str);
			ctf_fs_file_destroy(file);
			file = NULL;
			continue;
		}

		ret = ctf_fs_file_open(file, "rb");
		if (ret) {
			BT_LOGE("Cannot open stream file `%s`", file->path->str);
			goto error;
		}

		if (file->size == 0) {
			/* Skip empty stream. */
			BT_LOGD("Ignoring empty file `%s`", file->path->str);
			ctf_fs_file_destroy(file);
			continue;
		}

		ret = add_ds_file_to_ds_file_group(ctf_fs_trace,
			file->path->str);
		if (ret) {
			BT_LOGE("Cannot add stream file `%s` to stream file group",
				file->path->str);
			ctf_fs_file_destroy(file);
			goto error;
		}

		ctf_fs_file_destroy(file);
	}

	goto end;

error:
	ret = -1;

end:
	if (dir) {
		g_dir_close(dir);
		dir = NULL;
	}

	if (error) {
		g_error_free(error);
	}

	return ret;
}

static
int set_trace_name(bt_trace *trace, const char *name_suffix)
{
	int ret = 0;
	const bt_trace_class *tc = bt_trace_borrow_class_const(trace);
	const bt_value *val;
	GString *name;

	name = g_string_new(NULL);
	if (!name) {
		BT_LOGE_STR("Failed to allocate a GString.");
		ret = -1;
		goto end;
	}

	/*
	 * Check if we have a trace environment string value named `hostname`.
	 * If so, use it as the trace name's prefix.
	 */
	val = bt_trace_class_borrow_environment_entry_value_by_name_const(
		tc, "hostname");
	if (val && bt_value_is_string(val)) {
		g_string_append(name, bt_value_string_get(val));

		if (name_suffix) {
			g_string_append_c(name, G_DIR_SEPARATOR);
		}
	}

	if (name_suffix) {
		g_string_append(name, name_suffix);
	}

	ret = bt_trace_set_name(trace, name->str);
	if (ret) {
		goto end;
	}

	goto end;

end:
	if (name) {
		g_string_free(name, TRUE);
	}

	return ret;
}

static
struct ctf_fs_trace *ctf_fs_trace_create(bt_self_component_source *self_comp,
		const char *path, const char *name,
		struct ctf_fs_metadata_config *metadata_config)
{
	struct ctf_fs_trace *ctf_fs_trace;
	int ret;

	ctf_fs_trace = g_new0(struct ctf_fs_trace, 1);
	if (!ctf_fs_trace) {
		goto end;
	}

	ctf_fs_trace->path = g_string_new(path);
	if (!ctf_fs_trace->path) {
		goto error;
	}

	ctf_fs_trace->name = g_string_new(name);
	if (!ctf_fs_trace->name) {
		goto error;
	}

	ctf_fs_trace->metadata = g_new0(struct ctf_fs_metadata, 1);
	if (!ctf_fs_trace->metadata) {
		goto error;
	}

	ctf_fs_metadata_init(ctf_fs_trace->metadata);
	ctf_fs_trace->ds_file_groups = g_ptr_array_new_with_free_func(
		(GDestroyNotify) ctf_fs_ds_file_group_destroy);
	if (!ctf_fs_trace->ds_file_groups) {
		goto error;
	}

	ret = ctf_fs_metadata_set_trace_class(self_comp,
		ctf_fs_trace, metadata_config);
	if (ret) {
		goto error;
	}

	if (ctf_fs_trace->metadata->trace_class) {
		ctf_fs_trace->trace =
			bt_trace_create(ctf_fs_trace->metadata->trace_class);
		if (!ctf_fs_trace->trace) {
			goto error;
		}
	}

	if (ctf_fs_trace->trace) {
		ret = set_trace_name(ctf_fs_trace->trace, name);
		if (ret) {
			goto error;
		}
	}

	ret = create_ds_file_groups(ctf_fs_trace);
	if (ret) {
		goto error;
	}

	goto end;

error:
	ctf_fs_trace_destroy(ctf_fs_trace);
	ctf_fs_trace = NULL;

end:
	return ctf_fs_trace;
}

static
int path_is_ctf_trace(const char *path)
{
	GString *metadata_path = g_string_new(NULL);
	int ret = 0;

	if (!metadata_path) {
		ret = -1;
		goto end;
	}

	g_string_printf(metadata_path, "%s" G_DIR_SEPARATOR_S "%s", path, CTF_FS_METADATA_FILENAME);

	if (g_file_test(metadata_path->str, G_FILE_TEST_IS_REGULAR)) {
		ret = 1;
		goto end;
	}

end:
	g_string_free(metadata_path, TRUE);
	return ret;
}

static
int add_trace_path(GList **trace_paths, const char *path)
{
	GString *norm_path = NULL;
	int ret = 0;

	norm_path = bt_common_normalize_path(path, NULL);
	if (!norm_path) {
		BT_LOGE("Failed to normalize path `%s`.", path);
		ret = -1;
		goto end;
	}

	// FIXME: Remove or ifdef for __MINGW32__
	if (strcmp(norm_path->str, "/") == 0) {
		BT_LOGE("Opening a trace in `/` is not supported.");
		ret = -1;
		goto end;
	}

	*trace_paths = g_list_prepend(*trace_paths, norm_path);
	BT_ASSERT(*trace_paths);
	norm_path = NULL;

end:
	if (norm_path) {
		g_string_free(norm_path, TRUE);
	}

	return ret;
}

static
int ctf_fs_find_traces(GList **trace_paths, const char *start_path)
{
	int ret;
	GError *error = NULL;
	GDir *dir = NULL;
	const char *basename = NULL;

	/* Check if the starting path is a CTF trace itself */
	ret = path_is_ctf_trace(start_path);
	if (ret < 0) {
		goto end;
	}

	if (ret) {
		/*
		 * Stop recursion: a CTF trace cannot contain another
		 * CTF trace.
		 */
		ret = add_trace_path(trace_paths, start_path);
		goto end;
	}

	/* Look for subdirectories */
	if (!g_file_test(start_path, G_FILE_TEST_IS_DIR)) {
		/* Starting path is not a directory: end of recursion */
		goto end;
	}

	dir = g_dir_open(start_path, 0, &error);
	if (!dir) {
		if (error->code == G_FILE_ERROR_ACCES) {
			BT_LOGD("Cannot open directory `%s`: %s (code %d): continuing",
				start_path, error->message, error->code);
			goto end;
		}

		BT_LOGE("Cannot open directory `%s`: %s (code %d)",
			start_path, error->message, error->code);
		ret = -1;
		goto end;
	}

	while ((basename = g_dir_read_name(dir))) {
		GString *sub_path = g_string_new(NULL);

		if (!sub_path) {
			ret = -1;
			goto end;
		}

		g_string_printf(sub_path, "%s" G_DIR_SEPARATOR_S "%s", start_path, basename);
		ret = ctf_fs_find_traces(trace_paths, sub_path->str);
		g_string_free(sub_path, TRUE);
		if (ret) {
			goto end;
		}
	}

end:
	if (dir) {
		g_dir_close(dir);
	}

	if (error) {
		g_error_free(error);
	}

	return ret;
}

static
GList *ctf_fs_create_trace_names(GList *trace_paths, const char *base_path) {
	GList *trace_names = NULL;
	GList *node;
	const char *last_sep;
	size_t base_dist;

	/*
	 * At this point we know that all the trace paths are
	 * normalized, and so is the base path. This means that
	 * they are absolute and they don't end with a separator.
	 * We can simply find the location of the last separator
	 * in the base path, which gives us the name of the actual
	 * directory to look into, and use this location as the
	 * start of each trace name within each trace path.
	 *
	 * For example:
	 *
	 *     Base path: /home/user/my-traces/some-trace
	 *     Trace paths:
	 *       - /home/user/my-traces/some-trace/host1/trace1
	 *       - /home/user/my-traces/some-trace/host1/trace2
	 *       - /home/user/my-traces/some-trace/host2/trace
	 *       - /home/user/my-traces/some-trace/other-trace
	 *
	 * In this case the trace names are:
	 *
	 *       - some-trace/host1/trace1
	 *       - some-trace/host1/trace2
	 *       - some-trace/host2/trace
	 *       - some-trace/other-trace
	 */
	last_sep = strrchr(base_path, G_DIR_SEPARATOR);

	/* We know there's at least one separator */
	BT_ASSERT(last_sep);

	/* Distance to base */
	base_dist = last_sep - base_path + 1;

	/* Create the trace names */
	for (node = trace_paths; node; node = g_list_next(node)) {
		GString *trace_name = g_string_new(NULL);
		GString *trace_path = node->data;

		BT_ASSERT(trace_name);
		g_string_assign(trace_name, &trace_path->str[base_dist]);
		trace_names = g_list_append(trace_names, trace_name);
	}

	return trace_names;
}

/* Helper for create_ctf_fs_traces, to handle a single path/root. */

static
int create_ctf_fs_traces_one_root(bt_self_component_source *self_comp,
		struct ctf_fs_component *ctf_fs,
		const char *path_param)
{
	struct ctf_fs_trace *ctf_fs_trace = NULL;
	int ret = 0;
	GString *norm_path = NULL;
	GList *trace_paths = NULL;
	GList *trace_names = NULL;
	GList *tp_node;
	GList *tn_node;

	norm_path = bt_common_normalize_path(path_param, NULL);
	if (!norm_path) {
		BT_LOGE("Failed to normalize path: `%s`.",
			path_param);
		goto error;
	}

	ret = ctf_fs_find_traces(&trace_paths, norm_path->str);
	if (ret) {
		goto error;
	}

	if (!trace_paths) {
		BT_LOGE("No CTF traces recursively found in `%s`.",
			path_param);
		goto error;
	}

	trace_names = ctf_fs_create_trace_names(trace_paths, norm_path->str);
	if (!trace_names) {
		BT_LOGE("Cannot create trace names from trace paths.");
		goto error;
	}

	for (tp_node = trace_paths, tn_node = trace_names; tp_node;
			tp_node = g_list_next(tp_node),
			tn_node = g_list_next(tn_node)) {
		GString *trace_path = tp_node->data;
		GString *trace_name = tn_node->data;

		ctf_fs_trace = ctf_fs_trace_create(self_comp,
				trace_path->str, trace_name->str,
				&ctf_fs->metadata_config);
		if (!ctf_fs_trace) {
			BT_LOGE("Cannot create trace for `%s`.",
				trace_path->str);
			goto error;
		}

		g_ptr_array_add(ctf_fs->traces, ctf_fs_trace);
		ctf_fs_trace = NULL;
	}

	goto end;

error:
	ret = -1;
	ctf_fs_trace_destroy(ctf_fs_trace);

end:
	for (tp_node = trace_paths; tp_node; tp_node = g_list_next(tp_node)) {
		if (tp_node->data) {
			g_string_free(tp_node->data, TRUE);
		}
	}

	for (tn_node = trace_names; tn_node; tn_node = g_list_next(tn_node)) {
		if (tn_node->data) {
			g_string_free(tn_node->data, TRUE);
		}
	}

	if (trace_paths) {
		g_list_free(trace_paths);
	}

	if (trace_names) {
		g_list_free(trace_names);
	}

	if (norm_path) {
		g_string_free(norm_path, TRUE);
	}

	return ret;
}

/* GCompareFunc to sort traces by UUID. */

static
int sort_traces_by_uuid(gconstpointer a, gconstpointer b)
{
	struct ctf_fs_trace *trace_a = *((struct ctf_fs_trace **) a);
	struct ctf_fs_trace *trace_b = *((struct ctf_fs_trace **) b);

	bool trace_a_has_uuid = trace_a->metadata->tc->is_uuid_set;
	bool trace_b_has_uuid = trace_b->metadata->tc->is_uuid_set;

	/* Order traces without uuid first. */
	if (!trace_a_has_uuid && trace_b_has_uuid) {
		return -1;
	}

	if (trace_a_has_uuid && !trace_b_has_uuid) {
		return 1;
	}

	if (!trace_a_has_uuid && !trace_b_has_uuid) {
		return 0;
	}

	return bt_uuid_compare(trace_a->metadata->tc->uuid, trace_b->metadata->tc->uuid);
}

/* Count the number of stream and event classes defined by this trace's metadata.

   This is used to determine which metadata is the "latest", out of multiple
   traces sharing the same UUID. */

static
unsigned int metadata_count_stream_and_event_classes(struct ctf_fs_trace *trace)
{
	unsigned int num = trace->metadata->tc->stream_classes->len;

	for (guint i = 0; i < trace->metadata->tc->stream_classes->len; i++) {
		struct ctf_stream_class *sc = trace->metadata->tc->stream_classes->pdata[i];
		num += sc->event_classes->len;
	}

	return num;
}

/* Merge the src ds_file_group into dest.  This consists of merging their
   ds_file_infos, making sure to keep the result sorted. */

static
void merge_ctf_fs_ds_file_groups(struct ctf_fs_ds_file_group *dest, struct ctf_fs_ds_file_group *src)
{
	for (guint i = 0; i < src->ds_file_infos->len; i++) {
		struct ctf_fs_ds_file_info *ds_file_info =
			g_ptr_array_index(src->ds_file_infos, i);

		/* Ownership of the ds_file_info is transferred to dest. */
		g_ptr_array_index(src->ds_file_infos, i) = NULL;

		ds_file_group_insert_ds_file_info_sorted(dest, ds_file_info);
	}
}

/* Merge src_trace's data stream file groups into dest_trace's. */

static
void merge_matching_ctf_fs_ds_file_groups(
		struct ctf_fs_trace *dest_trace,
		struct ctf_fs_trace *src_trace)
{

	GPtrArray *dest = dest_trace->ds_file_groups;
	GPtrArray *src = src_trace->ds_file_groups;

	/* Save the initial length of dest: we only want to check against the
	   original elements in the inner loop.  */
	const guint dest_len = dest->len;


	for (guint s_i = 0; s_i < src->len; s_i++) {
		struct ctf_fs_ds_file_group *src_group = g_ptr_array_index(src, s_i);
		struct ctf_fs_ds_file_group *dest_group = NULL;

		/* A stream instance without ID can't match a stream in the other trace.  */
		if (src_group->stream_id != -1) {
			/* Let's search for a matching ds_file_group in the destination.  */
			for (guint d_i = 0; d_i < dest_len; d_i++) {
				struct ctf_fs_ds_file_group *candidate_dest = g_ptr_array_index(dest, d_i);

				/* Can't match a stream instance without ID.  */
				if (candidate_dest->stream_id == -1) {
					continue;
				}

				/* If the two groups have the same stream instance id
				   and belong to the same stream class (stream instance
				   ids are per-stream class), they represent the same
				   stream instance. */
				if (candidate_dest->stream_id != src_group->stream_id ||
					candidate_dest->sc->id != src_group->sc->id) {
					continue;
				}

				dest_group = candidate_dest;
				break;
			}
		}

		/* Didn't find a friend in dest to merge our src_group into?
		   Create a new empty one. */
		if (!dest_group) {
			struct ctf_stream_class *sc = ctf_trace_class_borrow_stream_class_by_id(
				dest_trace->metadata->tc, src_group->sc->id);
			BT_ASSERT(sc);

			dest_group = ctf_fs_ds_file_group_create(dest_trace, sc,
				src_group->stream_id);

			g_ptr_array_add(dest_trace->ds_file_groups, dest_group);
		}

		BT_ASSERT(dest_group);
		merge_ctf_fs_ds_file_groups(dest_group, src_group);
	}
}

/* Collapse the given traces, which must all share the same UUID, in a single
   one.

   The trace with the most expansive metadata is chosen and all other traces
   are merged into that one.  The array slots of all the traces that get merged
   in the chosen one are set to NULL, so only the slot of the chosen trace
   remains non-NULL. */

static
void merge_ctf_fs_traces(struct ctf_fs_trace **traces, unsigned int num_traces)
{
	BT_ASSERT(num_traces >= 2);

	unsigned int winner_count = metadata_count_stream_and_event_classes(traces[0]);
	struct ctf_fs_trace *winner = traces[0];

	/* Find the trace with the largest metadata. */
	for (guint i = 1; i < num_traces; i++) {
		struct ctf_fs_trace *candidate = traces[i];

		/* A bit of sanity check. */
		BT_ASSERT(bt_uuid_compare(winner->metadata->tc->uuid, candidate->metadata->tc->uuid) == 0);

		unsigned int candidate_count = metadata_count_stream_and_event_classes(candidate);

		if (candidate_count > winner_count) {
			winner_count = candidate_count;
			winner = candidate;
		}
	}

	/* Merge all the other traces in the winning trace. */
	for (guint i = 0; i < num_traces; i++) {
		struct ctf_fs_trace *trace = traces[i];

		/* Don't merge the winner into itself. */
		if (trace == winner) {
			continue;
		}

		/* Merge trace's data stream file groups into winner's. */
		merge_matching_ctf_fs_ds_file_groups(winner, trace);

		/* Free the trace that got merged into winner, clear the slot in the array. */
		ctf_fs_trace_destroy(trace);
		traces[i] = NULL;
	}

	/* Use the string representation of the UUID as the trace name. */
	char uuid_str[BABELTRACE_UUID_STR_LEN];
	bt_uuid_unparse(winner->metadata->tc->uuid, uuid_str);
	g_string_printf(winner->name, "%s", uuid_str);
}

/* Merge all traces of `ctf_fs` that share the same UUID in a single trace.
   Traces with no UUID are not merged. */

static
void merge_traces_with_same_uuid(struct ctf_fs_component *ctf_fs)
{
	GPtrArray *traces = ctf_fs->traces;

	/* Sort the traces by uuid, then collapse traces with the same uuid in a single one. */
	g_ptr_array_sort(traces, sort_traces_by_uuid);

	guint range_start_idx = 0;
	unsigned int num_traces = 0;

	/* Find ranges of consecutive traces that share the same UUID.  */
	while (range_start_idx < traces->len) {
		struct ctf_fs_trace *range_start_trace = g_ptr_array_index(traces, range_start_idx);

		/* Exclusive end of range. */
		guint range_end_exc_idx = range_start_idx + 1;

		while (range_end_exc_idx < traces->len) {
			struct ctf_fs_trace *this_trace = g_ptr_array_index(traces, range_end_exc_idx);

			if (!range_start_trace->metadata->tc->is_uuid_set ||
				(bt_uuid_compare(range_start_trace->metadata->tc->uuid, this_trace->metadata->tc->uuid) != 0)) {
				break;
			}

			range_end_exc_idx++;
		}

		/* If we have two or more traces with matching UUIDs, merge them. */
		guint range_len = range_end_exc_idx - range_start_idx;
		if (range_len > 1) {
			struct ctf_fs_trace **range_start = (struct ctf_fs_trace **) &traces->pdata[range_start_idx];
			merge_ctf_fs_traces(range_start, range_len);
		}

		num_traces++;
		range_start_idx = range_end_exc_idx;
	}

	/* Clear any NULL slot (traces that got merged in another one) in the array.  */
	for (guint i = 0; i < traces->len;) {
		if (g_ptr_array_index(traces, i) == NULL) {
			g_ptr_array_remove_index_fast(traces, i);
		} else {
			i++;
		}
	}

	BT_ASSERT(num_traces == traces->len);
}

int create_ctf_fs_traces(bt_self_component_source *self_comp,
		struct ctf_fs_component *ctf_fs,
		const bt_value *paths_value)
{
	int ret = 0;

	for (uint64_t i = 0; i < bt_value_array_get_size(paths_value); i++) {
		const bt_value *path_value = bt_value_array_borrow_element_by_index_const(paths_value, i);
		const char *path = bt_value_string_get(path_value);

		ret = create_ctf_fs_traces_one_root(self_comp, ctf_fs, path);
		if (ret) {
			goto error;
		}
	}

	merge_traces_with_same_uuid(ctf_fs);

error:
	return ret;
}

/* Create the IR stream objects for ctf_fs_trace. */

static
int create_streams_for_trace(struct ctf_fs_trace *ctf_fs_trace)
{
	int ret = -1;

	for (guint i = 0; i < ctf_fs_trace->ds_file_groups->len; i++) {
		struct ctf_fs_ds_file_group *ds_file_group =
			g_ptr_array_index(ctf_fs_trace->ds_file_groups, i);
		GString *name = get_stream_instance_unique_name(ds_file_group);

		if (!name) {
			goto error;
		}

		if (ds_file_group->sc->ir_sc) {
			BT_ASSERT(ctf_fs_trace->trace);

			if (ds_file_group->stream_id == UINT64_C(-1)) {
				/* No stream ID: use 0 */
				ds_file_group->stream = bt_stream_create_with_id(
					ds_file_group->sc->ir_sc,
					ctf_fs_trace->trace,
					ctf_fs_trace->next_stream_id);
				ctf_fs_trace->next_stream_id++;
			} else {
				/* Specific stream ID */
				ds_file_group->stream = bt_stream_create_with_id(
					ds_file_group->sc->ir_sc,
					ctf_fs_trace->trace,
					(uint64_t) ds_file_group->stream_id);
			}
		} else {
			ds_file_group->stream = NULL;
		}

		if (!ds_file_group->stream) {
			BT_LOGE("Cannot create stream for DS file group: "
				"addr=%p, stream-name=\"%s\"",
				ds_file_group, name->str);
			g_string_free(name, TRUE);
			goto error;
		}

		ret = bt_stream_set_name(ds_file_group->stream,
			name->str);
		if (ret) {
			BT_LOGE("Cannot set stream's name: "
				"addr=%p, stream-name=\"%s\"",
				ds_file_group->stream, name->str);
			g_string_free(name, TRUE);
			goto error;
		}

		g_string_free(name, TRUE);
	}

	ret = 0;

error:
	return ret;
}

bool validate_paths_parameter(const bt_value *paths)
{
	if (!paths) {
		BT_LOGE("missing \"paths\" parameter");
		return false;
	}

	bt_value_type type = bt_value_get_type(paths);
	if (type != BT_VALUE_TYPE_ARRAY) {
		BT_LOGE("type of \"paths\" parameter is %s, expecting BT_VALUE_TYPE_ARRAY",
			bt_common_value_type_string(type));
		return false;
	}

	for (uint64_t i = 0; i < bt_value_array_get_size(paths); i++) {
		const bt_value *elem = bt_value_array_borrow_element_by_index_const(paths, i);
		type = bt_value_get_type(elem);
		if (type != BT_VALUE_TYPE_STRING) {
			BT_LOGE("type of element at index %" PRIu64 " of \"paths\" parameter is %s, expecting BT_VALUE_TYPE_STRING",
				i, bt_common_value_type_string(type));
			return false;
		}
	}

	return true;
}

static
struct ctf_fs_component *ctf_fs_create(
		bt_self_component_source *self_comp,
		const bt_value *params)
{
	struct ctf_fs_component *ctf_fs = NULL;
	const bt_value *value = NULL;
	const bt_value *paths_value = NULL;

	paths_value = bt_value_map_borrow_entry_value_const(params, "paths");
	if (!validate_paths_parameter(paths_value)) {
		goto error;
	}

	ctf_fs = ctf_fs_new();
	if (!ctf_fs) {
		goto end;
	}

	bt_self_component_set_data(
		bt_self_component_source_as_self_component(self_comp),
		ctf_fs);

	/*
	 * We don't need to get a new reference here because as long as
	 * our private ctf_fs_component object exists, the containing
	 * private component should also exist.
	 */
	ctf_fs->self_comp = self_comp;

	value = bt_value_map_borrow_entry_value_const(params,
		"clock-class-offset-s");
	if (value) {
		if (!bt_value_is_integer(value)) {
			BT_LOGE("clock-class-offset-s should be an integer");
			goto error;
		}
		ctf_fs->metadata_config.clock_class_offset_s = bt_value_integer_get(value);
	}

	value = bt_value_map_borrow_entry_value_const(params,
		"clock-class-offset-ns");
	if (value) {
		if (!bt_value_is_integer(value)) {
			BT_LOGE("clock-class-offset-ns should be an integer");
			goto error;
		}
		ctf_fs->metadata_config.clock_class_offset_ns = bt_value_integer_get(value);
	}

	if (create_ctf_fs_traces(self_comp, ctf_fs, paths_value)) {
		goto error;
	}

	for (guint i = 0; i < ctf_fs->traces->len; i++) {
		struct ctf_fs_trace *trace = g_ptr_array_index(ctf_fs->traces, i);
		create_streams_for_trace(trace);
		create_ports_for_trace(ctf_fs, trace);
	}

	goto end;

error:
	ctf_fs_destroy(ctf_fs);
	ctf_fs = NULL;
	bt_self_component_set_data(
		bt_self_component_source_as_self_component(self_comp),
		NULL);

end:
	return ctf_fs;
}

BT_HIDDEN
bt_self_component_status ctf_fs_init(
		bt_self_component_source *self_comp,
		const bt_value *params, UNUSED_VAR void *init_method_data)
{
	struct ctf_fs_component *ctf_fs;
	bt_self_component_status ret = BT_SELF_COMPONENT_STATUS_OK;

	ctf_fs = ctf_fs_create(self_comp, params);
	if (!ctf_fs) {
		ret = BT_SELF_COMPONENT_STATUS_ERROR;
	}

	return ret;
}

BT_HIDDEN
bt_query_status ctf_fs_query(
		bt_self_component_class_source *comp_class,
		const bt_query_executor *query_exec,
		const char *object, const bt_value *params,
		const bt_value **result)
{
	bt_query_status status = BT_QUERY_STATUS_OK;

	if (!strcmp(object, "metadata-info")) {
		status = metadata_info_query(comp_class, params, result);
	} else if (!strcmp(object, "trace-info")) {
		status = trace_info_query(comp_class, params, result);
	} else {
		BT_LOGE("Unknown query object `%s`", object);
		status = BT_QUERY_STATUS_INVALID_OBJECT;
		goto end;
	}
end:
	return status;
}
