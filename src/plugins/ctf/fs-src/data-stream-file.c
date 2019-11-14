/*
 * Copyright 2016-2017 - Philippe Proulx <pproulx@efficios.com>
 * Copyright 2016 - Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2010-2011 - EfficiOS Inc. and Linux Foundation
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

#define BT_COMP_LOG_SELF_COMP (self_comp)
#define BT_LOG_OUTPUT_LEVEL (log_level)
#define BT_LOG_TAG "PLUGIN/SRC.CTF.FS/DS"
#include "logging/comp-logging.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <glib.h>
#include <inttypes.h>
#include "compat/mman.h"
#include "compat/endian.h"
#include <babeltrace2/babeltrace.h>
#include "common/common.h"
#include "file.h"
#include "metadata.h"
#include "../common/msg-iter/msg-iter.h"
#include "common/assert.h"
#include "data-stream-file.h"
#include <string.h>

static inline
size_t remaining_mmap_bytes(struct ctf_fs_ds_file *ds_file)
{
	BT_ASSERT_DBG(ds_file->mmap_len >= ds_file->request_offset);
	return ds_file->mmap_len - ds_file->request_offset;
}

static
int ds_file_munmap(struct ctf_fs_ds_file *ds_file)
{
	int ret = 0;
	bt_self_component *self_comp = ds_file->self_comp;
	bt_logging_level log_level = ds_file->log_level;

	if (!ds_file || !ds_file->mmap_addr) {
		goto end;
	}

	if (bt_munmap(ds_file->mmap_addr, ds_file->mmap_len)) {
		BT_COMP_LOGE_ERRNO("Cannot memory-unmap file",
			": address=%p, size=%zu, file_path=\"%s\", file=%p",
			ds_file->mmap_addr, ds_file->mmap_len,
			ds_file->file ? ds_file->file->path->str : "NULL",
			ds_file->file ? ds_file->file->fp : NULL);
		ret = -1;
		goto end;
	}

	ds_file->mmap_addr = NULL;

end:
	return ret;
}

static
enum ctf_msg_iter_medium_status ds_file_mmap_next(
		struct ctf_fs_ds_file *ds_file)
{
	enum ctf_msg_iter_medium_status ret =
			CTF_MSG_ITER_MEDIUM_STATUS_OK;
	bt_self_component *self_comp = ds_file->self_comp;
	bt_logging_level log_level = ds_file->log_level;

	/* Unmap old region */
	if (ds_file->mmap_addr) {
		if (ds_file_munmap(ds_file)) {
			goto error;
		}

		/*
		 * mmap_len is guaranteed to be page-aligned except on the
		 * last mapping where it may not be possible (since the file's
		 * size itself may not be a page multiple).
		 */
		ds_file->mmap_offset += ds_file->mmap_len;
		ds_file->request_offset = 0;
	}

	ds_file->mmap_len = MIN(ds_file->file->size - ds_file->mmap_offset,
			ds_file->mmap_max_len);
	if (ds_file->mmap_len == 0) {
		ret = CTF_MSG_ITER_MEDIUM_STATUS_EOF;
		goto end;
	}
	/* Map new region */
	BT_ASSERT(ds_file->mmap_len);
	ds_file->mmap_addr = bt_mmap((void *) 0, ds_file->mmap_len,
			PROT_READ, MAP_PRIVATE, fileno(ds_file->file->fp),
			ds_file->mmap_offset, ds_file->log_level);
	if (ds_file->mmap_addr == MAP_FAILED) {
		BT_COMP_LOGE("Cannot memory-map address (size %zu) of file \"%s\" (%p) at offset %jd: %s",
				ds_file->mmap_len, ds_file->file->path->str,
				ds_file->file->fp, (intmax_t) ds_file->mmap_offset,
				strerror(errno));
		goto error;
	}

	goto end;
error:
	ds_file_munmap(ds_file);
	ret = CTF_MSG_ITER_MEDIUM_STATUS_ERROR;
end:
	return ret;
}

static
enum ctf_msg_iter_medium_status medop_request_bytes(
		size_t request_sz, uint8_t **buffer_addr,
		size_t *buffer_sz, void *data)
{
	enum ctf_msg_iter_medium_status status =
		CTF_MSG_ITER_MEDIUM_STATUS_OK;
	struct ctf_fs_ds_file *ds_file = data;
	bt_self_component *self_comp = ds_file->self_comp;
	bt_logging_level log_level = ds_file->log_level;

	if (request_sz == 0) {
		goto end;
	}

	/*
	 * Check if we have at least one memory-mapped byte left. If we don't,
	 * mmap the next file.
	 */
	if (remaining_mmap_bytes(ds_file) == 0) {
		/* Are we at the end of the file? */
		if (ds_file->mmap_offset >= ds_file->file->size) {
			BT_COMP_LOGD("Reached end of file \"%s\" (%p)",
				ds_file->file->path->str, ds_file->file->fp);
			status = CTF_MSG_ITER_MEDIUM_STATUS_EOF;
			goto end;
		}

		status = ds_file_mmap_next(ds_file);
		switch (status) {
		case CTF_MSG_ITER_MEDIUM_STATUS_OK:
			break;
		case CTF_MSG_ITER_MEDIUM_STATUS_EOF:
			goto end;
		default:
			BT_COMP_LOGE("Cannot memory-map next region of file \"%s\" (%p)",
					ds_file->file->path->str,
					ds_file->file->fp);
			goto error;
		}
	}

	*buffer_sz = MIN(remaining_mmap_bytes(ds_file), request_sz);
	BT_ASSERT(ds_file->mmap_addr);
	*buffer_addr = ((uint8_t *) ds_file->mmap_addr) + ds_file->request_offset;
	ds_file->request_offset += *buffer_sz;
	goto end;

error:
	status = CTF_MSG_ITER_MEDIUM_STATUS_ERROR;

end:
	return status;
}

static
bt_stream *medop_borrow_stream(bt_stream_class *stream_class, int64_t stream_id,
		void *data)
{
	struct ctf_fs_ds_file *ds_file = data;
	bt_stream_class *ds_file_stream_class;
	bt_stream *stream = NULL;

	ds_file_stream_class = bt_stream_borrow_class(
		ds_file->stream);

	if (stream_class != ds_file_stream_class) {
		/*
		 * Not supported: two packets described by two different
		 * stream classes within the same data stream file.
		 */
		goto end;
	}

	stream = ds_file->stream;

end:
	return stream;
}

static
enum ctf_msg_iter_medium_status medop_seek(enum ctf_msg_iter_seek_whence whence,
		off_t offset, void *data)
{
	enum ctf_msg_iter_medium_status ret =
			CTF_MSG_ITER_MEDIUM_STATUS_OK;
	struct ctf_fs_ds_file *ds_file = data;
	off_t offset_in_mapping, file_size = ds_file->file->size;
	bt_self_component *self_comp = ds_file->self_comp;
	bt_logging_level log_level = ds_file->log_level;

	if (whence != CTF_MSG_ITER_SEEK_WHENCE_SET ||
		offset < 0 || offset > file_size) {
		BT_COMP_LOGE("Invalid medium seek request: whence=%d, offset=%jd, "
				"file-size=%jd", (int) whence, (intmax_t) offset,
				(intmax_t) file_size);
		ret = CTF_MSG_ITER_MEDIUM_STATUS_INVAL;
		goto end;
	}

	/* If there is no current mapping, map the right file directly. */
	if (!ds_file->mmap_addr) {
		goto map_requested_offset;
	}

	/*
	 * Determine whether or not the destination is contained within the
	 * current mapping.
	 */
	if (offset < ds_file->mmap_offset ||
			offset >= ds_file->mmap_offset + ds_file->mmap_len) {
		int unmap_ret;
		BT_COMP_LOGD("Medium seek request cannot be accomodated by the current "
				"file mapping: offset=%jd, mmap-offset=%jd, "
				"mmap-len=%zu", (intmax_t) offset, (intmax_t) ds_file->mmap_offset,
				ds_file->mmap_len);
		unmap_ret = ds_file_munmap(ds_file);
		if (unmap_ret) {
			ret = CTF_MSG_ITER_MEDIUM_STATUS_ERROR;
			goto end;
		}
		goto map_requested_offset;
	} else {
		ds_file->request_offset = offset - ds_file->mmap_offset;
		goto test_end;
	}

map_requested_offset:
	offset_in_mapping = offset %
		bt_mmap_get_offset_align_size(ds_file->log_level);

	ds_file->mmap_offset = offset - offset_in_mapping;
	ds_file->request_offset = offset_in_mapping;
	ret = ds_file_mmap_next(ds_file);
	if (ret != CTF_MSG_ITER_MEDIUM_STATUS_OK) {
		goto end;
	}

test_end:
	ds_file->end_reached = (offset == file_size);
end:
	return ret;
}

BT_HIDDEN
struct ctf_msg_iter_medium_ops ctf_fs_ds_file_medops = {
	.request_bytes = medop_request_bytes,
	.borrow_stream = medop_borrow_stream,
	.seek = medop_seek,
};

static
struct ctf_fs_ds_index_entry *ctf_fs_ds_index_entry_create(
		bt_self_component *self_comp, bt_logging_level log_level)
{
	struct ctf_fs_ds_index_entry *entry;

	entry = g_new0(struct ctf_fs_ds_index_entry, 1);
	if (!entry) {
		BT_COMP_LOGE_APPEND_CAUSE(self_comp, "Failed to allocate a ctf_fs_ds_index_entry.");
		goto end;
	}

	entry->packet_seq_num = UINT64_MAX;

end:
	return entry;
}

static
int convert_cycles_to_ns(struct ctf_clock_class *clock_class,
		uint64_t cycles, int64_t *ns)
{
	return bt_util_clock_cycles_to_ns_from_origin(cycles,
			clock_class->frequency, clock_class->offset_seconds,
			clock_class->offset_cycles, ns);
}

static
struct ctf_fs_ds_index *build_index_from_idx_file(
		struct ctf_fs_ds_file *ds_file,
		struct ctf_fs_ds_file_info *file_info,
		struct ctf_msg_iter *msg_iter)
{
	int ret;
	gchar *directory = NULL;
	gchar *basename = NULL;
	GString *index_basename = NULL;
	gchar *index_file_path = NULL;
	GMappedFile *mapped_file = NULL;
	gsize filesize;
	const char *mmap_begin = NULL, *file_pos = NULL;
	const struct ctf_packet_index_file_hdr *header = NULL;
	struct ctf_fs_ds_index *index = NULL;
	struct ctf_fs_ds_index_entry *index_entry = NULL, *prev_index_entry = NULL;
	uint64_t total_packets_size = 0;
	size_t file_index_entry_size;
	size_t file_entry_count;
	size_t i;
	struct ctf_stream_class *sc;
	struct ctf_msg_iter_packet_properties props;
	uint32_t version_major, version_minor;
	bt_self_component *self_comp = ds_file->self_comp;
	bt_logging_level log_level = ds_file->log_level;

	BT_COMP_LOGI("Building index from .idx file of stream file %s",
			ds_file->file->path->str);
	ret = ctf_msg_iter_get_packet_properties(msg_iter, &props);
	if (ret) {
		BT_COMP_LOGI_STR("Cannot read first packet's header and context fields.");
		goto error;
	}

	sc = ctf_trace_class_borrow_stream_class_by_id(ds_file->metadata->tc,
		props.stream_class_id);
	BT_ASSERT(sc);
	if (!sc->default_clock_class) {
		BT_COMP_LOGI_STR("Cannot find stream class's default clock class.");
		goto error;
	}

	/* Look for index file in relative path index/name.idx. */
	basename = g_path_get_basename(ds_file->file->path->str);
	if (!basename) {
		BT_COMP_LOGE("Cannot get the basename of datastream file %s",
				ds_file->file->path->str);
		goto error;
	}

	directory = g_path_get_dirname(ds_file->file->path->str);
	if (!directory) {
		BT_COMP_LOGE("Cannot get dirname of datastream file %s",
				ds_file->file->path->str);
		goto error;
	}

	index_basename = g_string_new(basename);
	if (!index_basename) {
		BT_COMP_LOGE_STR("Cannot allocate index file basename string");
		goto error;
	}

	g_string_append(index_basename, ".idx");
	index_file_path = g_build_filename(directory, "index",
			index_basename->str, NULL);
	mapped_file = g_mapped_file_new(index_file_path, FALSE, NULL);
	if (!mapped_file) {
		BT_COMP_LOGD("Cannot create new mapped file %s",
				index_file_path);
		goto error;
	}

	/*
	 * The g_mapped_file API limits us to 4GB files on 32-bit.
	 * Traces with such large indexes have never been seen in the wild,
	 * but this would need to be adjusted to support them.
	 */
	filesize = g_mapped_file_get_length(mapped_file);
	if (filesize < sizeof(*header)) {
		BT_COMP_LOGW("Invalid LTTng trace index file: "
			"file size (%zu bytes) < header size (%zu bytes)",
			filesize, sizeof(*header));
		goto error;
	}

	mmap_begin = g_mapped_file_get_contents(mapped_file);
	header = (struct ctf_packet_index_file_hdr *) mmap_begin;

	file_pos = g_mapped_file_get_contents(mapped_file) + sizeof(*header);
	if (be32toh(header->magic) != CTF_INDEX_MAGIC) {
		BT_COMP_LOGW_STR("Invalid LTTng trace index: \"magic\" field validation failed");
		goto error;
	}

	version_major = be32toh(header->index_major);
	version_minor = be32toh(header->index_minor);
	if (version_major != 1) {
		BT_COMP_LOGW(
			"Unknown LTTng trace index version: "
			"major=%" PRIu32 ", minor=%" PRIu32,
			version_major, version_minor);
		goto error;
	}

	file_index_entry_size = be32toh(header->packet_index_len);
	file_entry_count = (filesize - sizeof(*header)) / file_index_entry_size;
	if ((filesize - sizeof(*header)) % file_index_entry_size) {
		BT_COMP_LOGW("Invalid LTTng trace index: the index's size after the header "
			"(%zu bytes) is not a multiple of the index entry size "
			"(%zu bytes)", (filesize - sizeof(*header)),
			sizeof(*header));
		goto error;
	}

	index = ctf_fs_ds_index_create(ds_file->log_level, ds_file->self_comp);
	if (!index) {
		goto error;
	}

	for (i = 0; i < file_entry_count; i++) {
		struct ctf_packet_index *file_index =
				(struct ctf_packet_index *) file_pos;
		uint64_t packet_size = be64toh(file_index->packet_size);

		if (packet_size % CHAR_BIT) {
			BT_COMP_LOGW("Invalid packet size encountered in LTTng trace index file");
			goto error;
		}

		index_entry = ctf_fs_ds_index_entry_create(
			ds_file->self_comp, ds_file->log_level);
		if (!index_entry) {
			BT_COMP_LOGE_APPEND_CAUSE(ds_file->self_comp,
				"Failed to create a ctf_fs_ds_index_entry.");
			goto error;
		}

		/* Set path to stream file. */
		index_entry->path = file_info->path->str;

		/* Convert size in bits to bytes. */
		packet_size /= CHAR_BIT;
		index_entry->packet_size = packet_size;

		index_entry->offset = be64toh(file_index->offset);
		if (i != 0 && index_entry->offset < prev_index_entry->offset) {
			BT_COMP_LOGW("Invalid, non-monotonic, packet offset encountered in LTTng trace index file: "
				"previous offset=%" PRIu64 ", current offset=%" PRIu64,
				prev_index_entry->offset, index_entry->offset);
			goto error;
		}

		index_entry->timestamp_begin = be64toh(file_index->timestamp_begin);
		index_entry->timestamp_end = be64toh(file_index->timestamp_end);
		if (index_entry->timestamp_end < index_entry->timestamp_begin) {
			BT_COMP_LOGW("Invalid packet time bounds encountered in LTTng trace index file (begin > end): "
				"timestamp_begin=%" PRIu64 "timestamp_end=%" PRIu64,
				index_entry->timestamp_begin,
				index_entry->timestamp_end);
			goto error;
		}

		/* Convert the packet's bound to nanoseconds since Epoch. */
		ret = convert_cycles_to_ns(sc->default_clock_class,
				index_entry->timestamp_begin,
				&index_entry->timestamp_begin_ns);
		if (ret) {
			BT_COMP_LOGI_STR("Failed to convert raw timestamp to nanoseconds since Epoch during index parsing");
			goto error;
		}
		ret = convert_cycles_to_ns(sc->default_clock_class,
				index_entry->timestamp_end,
				&index_entry->timestamp_end_ns);
		if (ret) {
			BT_COMP_LOGI_STR("Failed to convert raw timestamp to nanoseconds since Epoch during LTTng trace index parsing");
			goto error;
		}

		if (version_minor >= 1) {
			index_entry->packet_seq_num = be64toh(file_index->packet_seq_num);
		}

		total_packets_size += packet_size;
		file_pos += file_index_entry_size;

		prev_index_entry = index_entry;

		/* Give ownership of `index_entry` to `index->entries`. */
		g_ptr_array_add(index->entries, index_entry);
		index_entry = NULL;
	}

	/* Validate that the index addresses the complete stream. */
	if (ds_file->file->size != total_packets_size) {
		BT_COMP_LOGW("Invalid LTTng trace index file; indexed size != stream file size: "
			"file-size=%" PRIu64 ", total-packets-size=%" PRIu64,
			ds_file->file->size, total_packets_size);
		goto error;
	}
end:
	g_free(directory);
	g_free(basename);
	g_free(index_file_path);
	if (index_basename) {
		g_string_free(index_basename, TRUE);
	}
	if (mapped_file) {
		g_mapped_file_unref(mapped_file);
	}
	return index;
error:
	ctf_fs_ds_index_destroy(index);
	g_free(index_entry);
	index = NULL;
	goto end;
}

static
int init_index_entry(struct ctf_fs_ds_index_entry *entry,
		struct ctf_fs_ds_file *ds_file,
		struct ctf_msg_iter_packet_properties *props,
		off_t packet_size, off_t packet_offset)
{
	int ret = 0;
	struct ctf_stream_class *sc;
	bt_self_component *self_comp = ds_file->self_comp;
	bt_logging_level log_level = ds_file->log_level;

	sc = ctf_trace_class_borrow_stream_class_by_id(ds_file->metadata->tc,
		props->stream_class_id);
	BT_ASSERT(sc);
	BT_ASSERT(packet_offset >= 0);
	entry->offset = packet_offset;
	BT_ASSERT(packet_size >= 0);
	entry->packet_size = packet_size;

	if (props->snapshots.beginning_clock != UINT64_C(-1)) {
		entry->timestamp_begin = props->snapshots.beginning_clock;

		/* Convert the packet's bound to nanoseconds since Epoch. */
		ret = convert_cycles_to_ns(sc->default_clock_class,
					   props->snapshots.beginning_clock,
					   &entry->timestamp_begin_ns);
		if (ret) {
			BT_COMP_LOGI_STR("Failed to convert raw timestamp to nanoseconds since Epoch.");
			goto end;
		}
	} else {
		entry->timestamp_begin = UINT64_C(-1);
		entry->timestamp_begin_ns = UINT64_C(-1);
	}

	if (props->snapshots.end_clock != UINT64_C(-1)) {
		entry->timestamp_end = props->snapshots.end_clock;

		/* Convert the packet's bound to nanoseconds since Epoch. */
		ret = convert_cycles_to_ns(sc->default_clock_class,
					   props->snapshots.end_clock,
					   &entry->timestamp_end_ns);
		if (ret) {
			BT_COMP_LOGI_STR("Failed to convert raw timestamp to nanoseconds since Epoch.");
			goto end;
		}
	} else {
		entry->timestamp_end = UINT64_C(-1);
		entry->timestamp_end_ns = UINT64_C(-1);
	}

end:
	return ret;
}

static
struct ctf_fs_ds_index *build_index_from_stream_file(
		struct ctf_fs_ds_file *ds_file,
		struct ctf_fs_ds_file_info *file_info,
		struct ctf_msg_iter *msg_iter)
{
	int ret;
	struct ctf_fs_ds_index *index = NULL;
	enum ctf_msg_iter_status iter_status = CTF_MSG_ITER_STATUS_OK;
	off_t current_packet_offset_bytes = 0;
	bt_self_component *self_comp = ds_file->self_comp;
	bt_logging_level log_level = ds_file->log_level;

	BT_COMP_LOGI("Indexing stream file %s", ds_file->file->path->str);

	index = ctf_fs_ds_index_create(ds_file->log_level, ds_file->self_comp);
	if (!index) {
		goto error;
	}

	while (true) {
		off_t current_packet_size_bytes;
		struct ctf_fs_ds_index_entry *index_entry;
		struct ctf_msg_iter_packet_properties props;

		if (current_packet_offset_bytes < 0) {
			BT_COMP_LOGE_STR("Cannot get the current packet's offset.");
			goto error;
		} else if (current_packet_offset_bytes > ds_file->file->size) {
			BT_COMP_LOGE_STR("Unexpected current packet's offset (larger than file).");
			goto error;
		} else if (current_packet_offset_bytes == ds_file->file->size) {
			/* No more data */
			break;
		}

		iter_status = ctf_msg_iter_seek(msg_iter,
				current_packet_offset_bytes);
		if (iter_status != CTF_MSG_ITER_STATUS_OK) {
			goto error;
		}

		iter_status = ctf_msg_iter_get_packet_properties(
			msg_iter, &props);
		if (iter_status != CTF_MSG_ITER_STATUS_OK) {
			goto error;
		}

		if (props.exp_packet_total_size >= 0) {
			current_packet_size_bytes =
				(uint64_t) props.exp_packet_total_size / 8;
		} else {
			current_packet_size_bytes = ds_file->file->size;
		}

		if (current_packet_offset_bytes + current_packet_size_bytes >
				ds_file->file->size) {
			BT_COMP_LOGW("Invalid packet size reported in file: stream=\"%s\", "
					"packet-offset=%jd, packet-size-bytes=%jd, "
					"file-size=%jd",
					ds_file->file->path->str,
					(intmax_t) current_packet_offset_bytes,
					(intmax_t) current_packet_size_bytes,
					(intmax_t) ds_file->file->size);
			goto error;
		}

		index_entry = ctf_fs_ds_index_entry_create(
			ds_file->self_comp, ds_file->log_level);
		if (!index_entry) {
			BT_COMP_LOGE_APPEND_CAUSE(ds_file->self_comp,
				"Failed to create a ctf_fs_ds_index_entry.");
			goto error;
		}

		/* Set path to stream file. */
		index_entry->path = file_info->path->str;

		ret = init_index_entry(index_entry, ds_file, &props,
			current_packet_size_bytes, current_packet_offset_bytes);
		if (ret) {
			g_free(index_entry);
			goto error;
		}

		g_ptr_array_add(index->entries, index_entry);

		current_packet_offset_bytes += current_packet_size_bytes;
		BT_COMP_LOGD("Seeking to next packet: current-packet-offset=%jd, "
			"next-packet-offset=%jd",
			(intmax_t) (current_packet_offset_bytes - current_packet_size_bytes),
			(intmax_t) current_packet_offset_bytes);
	}

end:
	return index;

error:
	ctf_fs_ds_index_destroy(index);
	index = NULL;
	goto end;
}

BT_HIDDEN
struct ctf_fs_ds_file *ctf_fs_ds_file_create(
		struct ctf_fs_trace *ctf_fs_trace,
		bt_self_message_iterator *self_msg_iter,
		bt_stream *stream, const char *path,
		bt_logging_level log_level)
{
	int ret;
	const size_t offset_align = bt_mmap_get_offset_align_size(log_level);
	struct ctf_fs_ds_file *ds_file = g_new0(struct ctf_fs_ds_file, 1);

	if (!ds_file) {
		goto error;
	}

	ds_file->log_level = log_level;
	ds_file->self_comp = ctf_fs_trace->self_comp;
	ds_file->self_msg_iter = self_msg_iter;
	ds_file->file = ctf_fs_file_create(log_level, ds_file->self_comp);
	if (!ds_file->file) {
		goto error;
	}

	ds_file->stream = stream;
	bt_stream_get_ref(ds_file->stream);
	ds_file->metadata = ctf_fs_trace->metadata;
	g_string_assign(ds_file->file->path, path);
	ret = ctf_fs_file_open(ds_file->file, "rb");
	if (ret) {
		goto error;
	}

	ds_file->mmap_max_len = offset_align * 2048;

	goto end;

error:
	/* Do not touch "borrowed" file. */
	ctf_fs_ds_file_destroy(ds_file);
	ds_file = NULL;

end:
	return ds_file;
}

BT_HIDDEN
struct ctf_fs_ds_index *ctf_fs_ds_file_build_index(
		struct ctf_fs_ds_file *ds_file,
		struct ctf_fs_ds_file_info *file_info,
		struct ctf_msg_iter *msg_iter)
{
	struct ctf_fs_ds_index *index;
	bt_self_component *self_comp = ds_file->self_comp;
	bt_logging_level log_level = ds_file->log_level;

	index = build_index_from_idx_file(ds_file, file_info, msg_iter);
	if (index) {
		goto end;
	}

	BT_COMP_LOGI("Failed to build index from .index file; "
		"falling back to stream indexing.");
	index = build_index_from_stream_file(ds_file, file_info, msg_iter);
end:
	return index;
}

BT_HIDDEN
struct ctf_fs_ds_index *ctf_fs_ds_index_create(bt_logging_level log_level,
		bt_self_component *self_comp)
{
	struct ctf_fs_ds_index *index = g_new0(struct ctf_fs_ds_index, 1);

	if (!index) {
		BT_COMP_LOG_CUR_LVL(BT_LOG_ERROR, log_level, self_comp,
			"Failed to allocate index");
		goto error;
	}

	index->entries = g_ptr_array_new_with_free_func((GDestroyNotify) g_free);
	if (!index->entries) {
		BT_COMP_LOG_CUR_LVL(BT_LOG_ERROR, log_level, self_comp,
			"Failed to allocate index entries.");
		goto error;
	}

	goto end;

error:
	ctf_fs_ds_index_destroy(index);
	index = NULL;
end:
	return index;
}

BT_HIDDEN
void ctf_fs_ds_file_destroy(struct ctf_fs_ds_file *ds_file)
{
	if (!ds_file) {
		return;
	}

	bt_stream_put_ref(ds_file->stream);
	(void) ds_file_munmap(ds_file);

	if (ds_file->file) {
		ctf_fs_file_destroy(ds_file->file);
	}

	g_free(ds_file);
}

BT_HIDDEN
bt_component_class_message_iterator_next_method_status ctf_fs_ds_file_next(
		struct ctf_msg_iter *msg_iter,
		const bt_message **msg)
{
	enum ctf_msg_iter_status msg_iter_status;
	bt_component_class_message_iterator_next_method_status status;

	msg_iter_status = ctf_msg_iter_get_next_message(msg_iter, msg);

	switch (msg_iter_status) {
	case CTF_MSG_ITER_STATUS_EOF:
		status = BT_COMPONENT_CLASS_MESSAGE_ITERATOR_NEXT_METHOD_STATUS_END;
		break;
	case CTF_MSG_ITER_STATUS_OK:
		status = BT_COMPONENT_CLASS_MESSAGE_ITERATOR_NEXT_METHOD_STATUS_OK;
		break;
	case CTF_MSG_ITER_STATUS_AGAIN:
		/*
		 * Should not make it this far as this is
		 * medium-specific; there is nothing for the user to do
		 * and it should have been handled upstream.
		 */
		bt_common_abort();
	case CTF_MSG_ITER_STATUS_INVAL:
	case CTF_MSG_ITER_STATUS_ERROR:
	default:
		status = BT_COMPONENT_CLASS_MESSAGE_ITERATOR_NEXT_METHOD_STATUS_ERROR;
		break;
	}
	return status;
}

BT_HIDDEN
void ctf_fs_ds_index_destroy(struct ctf_fs_ds_index *index)
{
	if (!index) {
		return;
	}

	if (index->entries) {
		g_ptr_array_free(index->entries, TRUE);
	}
	g_free(index);
}
