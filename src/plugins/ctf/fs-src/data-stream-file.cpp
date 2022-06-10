/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2016-2017 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2010-2011 EfficiOS Inc. and Linux Foundation
 */

#define BT_COMP_LOG_SELF_COMP (logCfg.selfComp)
#define BT_LOG_OUTPUT_LEVEL   (logCfg.logLevel)
#define BT_LOG_TAG            "PLUGIN/SRC.CTF.FS/DS"
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
#include "file.hpp"
#include "metadata.hpp"
#include "../common/src/msg-iter/msg-iter.hpp"
#include "common/assert.h"
#include "data-stream-file.hpp"
#include <string.h>
#include "cpp-common/make-unique.hpp"
#include "fs.hpp"

static inline size_t remaining_mmap_bytes(struct ctf_fs_ds_file *ds_file)
{
    BT_ASSERT_DBG(ds_file->mmap_len >= ds_file->request_offset_in_mapping);
    return ds_file->mmap_len - ds_file->request_offset_in_mapping;
}

/*
 * Return true if `offset_in_file` is in the current mapping.
 */

static bool offset_ist_mapped(struct ctf_fs_ds_file *ds_file, off_t offset_in_file)
{
    if (!ds_file->mmap_addr)
        return false;

    return offset_in_file >= ds_file->mmap_offset_in_file &&
           offset_in_file < (ds_file->mmap_offset_in_file + ds_file->mmap_len);
}

enum ds_file_status
{
    DS_FILE_STATUS_OK = 0,
    DS_FILE_STATUS_ERROR = -1,
    DS_FILE_STATUS_EOF = 1,
};

static ds_file_status ds_file_munmap(struct ctf_fs_ds_file *ds_file)
{
    const ctf::LogCfg& logCfg = ds_file->logCfg;

    BT_ASSERT(ds_file);

    if (!ds_file->mmap_addr) {
        return DS_FILE_STATUS_OK;
    }

    if (bt_munmap(ds_file->mmap_addr, ds_file->mmap_len)) {
        BT_COMP_LOGE_ERRNO("Cannot memory-unmap file",
                           ": address=%p, size=%zu, file_path=\"%s\", file=%p", ds_file->mmap_addr,
                           ds_file->mmap_len, ds_file->file ? ds_file->file->path.c_str() : "NULL",
                           ds_file->file ? ds_file->file->fp.get() : NULL);
        return DS_FILE_STATUS_ERROR;
    }

    ds_file->mmap_addr = NULL;

    return DS_FILE_STATUS_OK;
}

/*
 * mmap a region of `ds_file` such that `requested_offset_in_file` is in the
 * mapping.  If the currently mmap-ed region already contains
 * `requested_offset_in_file`, the mapping is kept.
 *
 * `requested_offset_in_file` must be a valid offset in the file.
 */
static ds_file_status ds_file_mmap(struct ctf_fs_ds_file *ds_file, off_t requested_offset_in_file)
{
    const ctf::LogCfg& logCfg = ds_file->logCfg;

    /* Ensure the requested offset is in the file range. */
    BT_ASSERT(requested_offset_in_file >= 0);
    BT_ASSERT(requested_offset_in_file < ds_file->file->size);

    /*
     * If the mapping already contains the requested range, we have nothing to
     * do.
     */
    if (offset_ist_mapped(ds_file, requested_offset_in_file)) {
        return DS_FILE_STATUS_OK;
    }

    /* Unmap old region */
    ds_file_status status = ds_file_munmap(ds_file);
    if (status != DS_FILE_STATUS_OK) {
        return status;
    }

    /*
     * Compute a mapping that has the required alignment properties and
     * contains `requested_offset_in_file`.
     */
    ds_file->request_offset_in_mapping =
        requested_offset_in_file % bt_mmap_get_offset_align_size(logCfg.logLevel);
    ds_file->mmap_offset_in_file = requested_offset_in_file - ds_file->request_offset_in_mapping;
    ds_file->mmap_len =
        MIN(ds_file->file->size - ds_file->mmap_offset_in_file, ds_file->mmap_max_len);

    BT_ASSERT(ds_file->mmap_len > 0);

    ds_file->mmap_addr =
        bt_mmap((void *) 0, ds_file->mmap_len, PROT_READ, MAP_PRIVATE,
                fileno(ds_file->file->fp.get()), ds_file->mmap_offset_in_file, logCfg.logLevel);
    if (ds_file->mmap_addr == MAP_FAILED) {
        BT_COMP_LOGE("Cannot memory-map address (size %zu) of file \"%s\" (%p) at offset %jd: %s",
                     ds_file->mmap_len, ds_file->file->path.c_str(), ds_file->file->fp.get(),
                     (intmax_t) ds_file->mmap_offset_in_file, strerror(errno));
        return DS_FILE_STATUS_ERROR;
    }

    return DS_FILE_STATUS_OK;
}

static ctf_fs_ds_index_entry::UP ctf_fs_ds_index_entry_create(const bt2_common::DataLen offset,
                                                              const bt2_common::DataLen packetSize)
{
    ctf_fs_ds_index_entry::UP entry =
        bt2_common::makeUnique<ctf_fs_ds_index_entry>(offset, packetSize);

    entry->packet_seq_num = UINT64_MAX;

    return entry;
}

static int convert_cycles_to_ns(struct ctf_clock_class *clock_class, uint64_t cycles, int64_t *ns)
{
    return bt_util_clock_cycles_to_ns_from_origin(cycles, clock_class->frequency,
                                                  clock_class->offset_seconds,
                                                  clock_class->offset_cycles, ns);
}

static ctf_fs_ds_index::UP build_index_from_idx_file(struct ctf_fs_ds_file *ds_file,
                                                     struct ctf_fs_ds_file_info *file_info,
                                                     struct ctf_msg_iter *msg_iter)
{
    bt2_common::GCharUP directory;
    bt2_common::GCharUP basename;
    std::string index_basename;
    bt2_common::GCharUP index_file_path;
    bt2_common::GMappedFileUP mapped_file;
    gsize filesize;
    const char *mmap_begin = NULL, *file_pos = NULL;
    const struct ctf_packet_index_file_hdr *header = NULL;
    ctf_fs_ds_index::UP index;
    bt2_common::DataLen totalPacketsSize = bt2_common::DataLen::fromBytes(0);
    size_t file_index_entry_size;
    size_t file_entry_count;
    size_t i;
    struct ctf_stream_class *sc;
    struct ctf_msg_iter_packet_properties props;
    uint32_t version_major, version_minor;
    const ctf::LogCfg& logCfg = ds_file->logCfg;

    BT_COMP_LOGI("Building index from .idx file of stream file %s", ds_file->file->path.c_str());
    int ret = ctf_msg_iter_get_packet_properties(msg_iter, &props);
    if (ret) {
        BT_COMP_LOGI_STR("Cannot read first packet's header and context fields.");
        return nullptr;
    }

    sc = ctf_trace_class_borrow_stream_class_by_id(ds_file->metadata->tc, props.stream_class_id);
    BT_ASSERT(sc);
    if (!sc->default_clock_class) {
        BT_COMP_LOGI_STR("Cannot find stream class's default clock class.");
        return nullptr;
    }

    /* Look for index file in relative path index/name.idx. */
    basename.reset(g_path_get_basename(ds_file->file->path.c_str()));
    if (!basename) {
        BT_COMP_LOGE("Cannot get the basename of datastream file %s", ds_file->file->path.c_str());
        return nullptr;
    }

    directory.reset(g_path_get_dirname(ds_file->file->path.c_str()));
    if (!directory) {
        BT_COMP_LOGE("Cannot get dirname of datastream file %s", ds_file->file->path.c_str());
        return nullptr;
    }

    index_basename = basename.get();
    index_basename += ".idx";

    index_file_path.reset(g_build_filename(directory.get(), "index", index_basename.c_str(), NULL));
    mapped_file.reset(g_mapped_file_new(index_file_path.get(), FALSE, NULL));
    if (!mapped_file) {
        BT_COMP_LOGD("Cannot create new mapped file %s", index_file_path.get());
        return nullptr;
    }

    /*
     * The g_mapped_file API limits us to 4GB files on 32-bit.
     * Traces with such large indexes have never been seen in the wild,
     * but this would need to be adjusted to support them.
     */
    filesize = g_mapped_file_get_length(mapped_file.get());
    if (filesize < sizeof(*header)) {
        BT_COMP_LOGW("Invalid LTTng trace index file: "
                     "file size (%zu bytes) < header size (%zu bytes)",
                     filesize, sizeof(*header));
        return nullptr;
    }

    mmap_begin = g_mapped_file_get_contents(mapped_file.get());
    header = (struct ctf_packet_index_file_hdr *) mmap_begin;

    file_pos = g_mapped_file_get_contents(mapped_file.get()) + sizeof(*header);
    if (be32toh(header->magic) != CTF_INDEX_MAGIC) {
        BT_COMP_LOGW_STR("Invalid LTTng trace index: \"magic\" field validation failed");
        return nullptr;
    }

    version_major = be32toh(header->index_major);
    version_minor = be32toh(header->index_minor);
    if (version_major != 1) {
        BT_COMP_LOGW("Unknown LTTng trace index version: "
                     "major=%" PRIu32 ", minor=%" PRIu32,
                     version_major, version_minor);
        return nullptr;
    }

    file_index_entry_size = be32toh(header->packet_index_len);
    if (file_index_entry_size < CTF_INDEX_1_0_SIZE) {
        BT_COMP_LOGW(
            "Invalid `packet_index_len` in LTTng trace index file (`packet_index_len` < CTF index 1.0 index entry size): "
            "packet_index_len=%zu, CTF_INDEX_1_0_SIZE=%zu",
            file_index_entry_size, CTF_INDEX_1_0_SIZE);
        return nullptr;
    }

    file_entry_count = (filesize - sizeof(*header)) / file_index_entry_size;
    if ((filesize - sizeof(*header)) % file_index_entry_size) {
        BT_COMP_LOGW("Invalid LTTng trace index: the index's size after the header "
                     "(%zu bytes) is not a multiple of the index entry size "
                     "(%zu bytes)",
                     (filesize - sizeof(*header)), sizeof(*header));
        return nullptr;
    }

    index = bt2_common::makeUnique<ctf_fs_ds_index>();

    for (i = 0; i < file_entry_count; i++) {
        struct ctf_packet_index *file_index = (struct ctf_packet_index *) file_pos;
        bt2_common::DataLen packetSize =
            bt2_common::DataLen::fromBits(be64toh(file_index->packet_size));

        if (packetSize.hasExtraBits()) {
            BT_COMP_LOGW("Invalid packet size encountered in LTTng trace index file");
            return nullptr;
        }

        bt2_common::DataLen offset = bt2_common::DataLen::fromBytes(be64toh(file_index->offset));
        if (i != 0 && offset < prev_index_entry->offset) {
            BT_COMP_LOGW(
                "Invalid, non-monotonic, packet offset encountered in LTTng trace index file: "
                "previous offset=%llu bytes, current offset=%llu bytes",
                prev_index_entry->offset.bytes(), offset.bytes());
            return nullptr;
        }

        ctf_fs_ds_index_entry index_entry {offset, packetSize};

        /* Set path to stream file. */
        index_entry.path = file_info->path.c_str();

        index_entry.timestamp_begin = be64toh(file_index->timestamp_begin);
        index_entry.timestamp_end = be64toh(file_index->timestamp_end);
        if (index_entry.timestamp_end < index_entry.timestamp_begin) {
            BT_COMP_LOGW(
                "Invalid packet time bounds encountered in LTTng trace index file (begin > end): "
                "timestamp_begin=%" PRIu64 "timestamp_end=%" PRIu64,
                index_entry.timestamp_begin, index_entry.timestamp_end);
            return nullptr;
        }

        /* Convert the packet's bound to nanoseconds since Epoch. */
        ret = convert_cycles_to_ns(sc->default_clock_class, index_entry.timestamp_begin,
                                   &index_entry.timestamp_begin_ns);
        if (ret) {
            BT_COMP_LOGI_STR(
                "Failed to convert raw timestamp to nanoseconds since Epoch during index parsing");
            return nullptr;
        }
        ret = convert_cycles_to_ns(sc->default_clock_class, index_entry.timestamp_end,
                                   &index_entry.timestamp_end_ns);
        if (ret) {
            BT_COMP_LOGI_STR(
                "Failed to convert raw timestamp to nanoseconds since Epoch during LTTng trace index parsing");
            return nullptr;
        }

        if (version_minor >= 1) {
            index_entry.packet_seq_num = be64toh(file_index->packet_seq_num);
        }

        totalPacketsSize += packetSize;
        file_pos += file_index_entry_size;

        prev_index_entry = index_entry.get();

        index->entries.emplace_back(index_entry);
    }

    /* Validate that the index addresses the complete stream. */
    if (ds_file->file->size != totalPacketsSize.bytes()) {
        BT_COMP_LOGW("Invalid LTTng trace index file; indexed size != stream file size: "
                     "file-size=%" PRIu64 " bytes, total-packets-size=%llu bytes",
                     ds_file->file->size, totalPacketsSize.bytes());
        return nullptr;
    }

    return index;
}

static int init_index_entry(struct ctf_fs_ds_index_entry *entry, struct ctf_fs_ds_file *ds_file,
                            struct ctf_msg_iter_packet_properties *props)
{
    struct ctf_stream_class *sc;

    sc = ctf_trace_class_borrow_stream_class_by_id(ds_file->metadata->tc, props->stream_class_id);
    BT_ASSERT(sc);
    const ctf::LogCfg& logCfg = ds_file->logCfg;

    if (props->snapshots.beginning_clock != UINT64_C(-1)) {
        entry->timestamp_begin = props->snapshots.beginning_clock;

        /* Convert the packet's bound to nanoseconds since Epoch. */
        int ret = convert_cycles_to_ns(sc->default_clock_class, props->snapshots.beginning_clock,
                                       &entry->timestamp_begin_ns);
        if (ret) {
            BT_COMP_LOGI_STR("Failed to convert raw timestamp to nanoseconds since Epoch.");
            return ret;
        }
    } else {
        entry->timestamp_begin = UINT64_C(-1);
        entry->timestamp_begin_ns = UINT64_C(-1);
    }

    if (props->snapshots.end_clock != UINT64_C(-1)) {
        entry->timestamp_end = props->snapshots.end_clock;

        /* Convert the packet's bound to nanoseconds since Epoch. */
        int ret = convert_cycles_to_ns(sc->default_clock_class, props->snapshots.end_clock,
                                       &entry->timestamp_end_ns);
        if (ret) {
            BT_COMP_LOGI_STR("Failed to convert raw timestamp to nanoseconds since Epoch.");
            return ret;
        }
    } else {
        entry->timestamp_end = UINT64_C(-1);
        entry->timestamp_end_ns = UINT64_C(-1);
    }

    return 0;
}

static ctf_fs_ds_index::UP build_index_from_stream_file(struct ctf_fs_ds_file *ds_file,
                                                        struct ctf_fs_ds_file_info *file_info,
                                                        struct ctf_msg_iter *msg_iter)
{
    int ret;
    enum ctf_msg_iter_status iter_status = CTF_MSG_ITER_STATUS_OK;
    bt2_common::DataLen currentPacketOffset = bt2_common::DataLen::fromBytes(0);
    const ctf::LogCfg& logCfg = ds_file->logCfg;

    BT_COMP_LOGI("Indexing stream file %s", ds_file->file->path.c_str());

    ctf_fs_ds_index::UP index = bt2_common::makeUnique<ctf_fs_ds_index>();

    while (true) {
        ctf_fs_ds_index_entry::UP index_entry;
        struct ctf_msg_iter_packet_properties props;

        if (currentPacketOffset.bytes() > ds_file->file->size) {
            BT_COMP_LOGE_STR("Unexpected current packet's offset (larger than file).");
            return nullptr;
        } else if (currentPacketOffset.bytes() == ds_file->file->size) {
            /* No more data */
            break;
        }

        iter_status = ctf_msg_iter_seek(msg_iter, currentPacketOffset.bytes());
        if (iter_status != CTF_MSG_ITER_STATUS_OK) {
            return nullptr;
        }

        iter_status = ctf_msg_iter_get_packet_properties(msg_iter, &props);
        if (iter_status != CTF_MSG_ITER_STATUS_OK) {
            return nullptr;
        }

        /*
         * Get the current packet size from the packet header, if set.  Else,
         * assume there is a single packet in the file, so take the file size
         * as the packet size.
         */
        bt2_common::DataLen currentPacketSize =
            props.exp_packet_total_size >= 0 ?
                bt2_common::DataLen::fromBits(props.exp_packet_total_size) :
                bt2_common::DataLen::fromBytes(ds_file->file->size);

        if ((currentPacketOffset + currentPacketSize).bytes() > ds_file->file->size) {
            BT_COMP_LOGW("Invalid packet size reported in file: stream=\"%s\", "
                         "packet-offset-bytes=%llu, packet-size-bytes=%llu, "
                         "file-size-bytes=%jd",
                         ds_file->file->path.c_str(), currentPacketOffset.bytes(),
                         currentPacketSize.bytes(), (intmax_t) ds_file->file->size);
            return nullptr;
        }

        index_entry =
            bt2_common::makeUnique<ctf_fs_ds_index_entry>(currentPacketOffset, currentPacketSize);
        if (!index_entry) {
            BT_COMP_LOGE_APPEND_CAUSE(logCfg.selfComp, "Failed to create a ctf_fs_ds_index_entry.");
            return nullptr;
        }

        /* Set path to stream file. */
        index_entry->path = file_info->path.c_str();

        ret = init_index_entry(index_entry.get(), ds_file, &props);
        if (ret) {
            return nullptr;
        }

        index->entries.emplace_back(std::move(index_entry));

        currentPacketOffset += currentPacketSize;
        BT_COMP_LOGD("Seeking to next packet: current-packet-offset-bytes=%llu, "
                     "next-packet-offset-bytes=%llu",
                     (currentPacketOffset - currentPacketSize).bytes(),
                     currentPacketOffset.bytes());
    }

    return index;
}

BT_HIDDEN
ctf_fs_ds_file::UP ctf_fs_ds_file_create(struct ctf_fs_trace *ctf_fs_trace,
                                         nonstd::optional<bt2::Stream::Shared> stream,
                                         const char *path, const ctf::LogCfg& logCfg)
{
    int ret;
    const size_t offset_align = bt_mmap_get_offset_align_size(logCfg.logLevel);
    ctf_fs_ds_file::UP ds_file = bt2_common::makeUnique<ctf_fs_ds_file>(logCfg);

    ds_file->file = bt2_common::makeUnique<ctf_fs_file>(logCfg);
    ds_file->stream = std::move(stream);
    ds_file->metadata = ctf_fs_trace->metadata.get();
    ds_file->file->path = path;
    ret = ctf_fs_file_open(ds_file->file.get(), "rb");
    if (ret) {
        return nullptr;
    }

    ds_file->mmap_max_len = offset_align * 2048;

    return ds_file;
}

BT_HIDDEN
ctf_fs_ds_index::UP ctf_fs_ds_file_build_index(struct ctf_fs_ds_file *ds_file,
                                               struct ctf_fs_ds_file_info *file_info,
                                               struct ctf_msg_iter *msg_iter)
{
    ctf_fs_ds_index::UP index;
    const ctf::LogCfg& logCfg = ds_file->logCfg;

    index = build_index_from_idx_file(ds_file, file_info, msg_iter);
    if (index) {
        return index;
    }

    BT_COMP_LOGI("Failed to build index from .index file; "
                 "falling back to stream indexing.");
    return build_index_from_stream_file(ds_file, file_info, msg_iter);
}

ctf_fs_ds_file::~ctf_fs_ds_file()
{
    (void) ds_file_munmap(this);
}

BT_HIDDEN ctf_fs_ds_file_info::UP ctf_fs_ds_file_info_create(const char *path, int64_t begin_ns)
{
    ctf_fs_ds_file_info::UP ds_file_info = bt2_common::makeUnique<ctf_fs_ds_file_info>();

    ds_file_info->path = path;
    ds_file_info->begin_ns = begin_ns;

    return ds_file_info;
}

BT_HIDDEN ctf_fs_ds_file_group::UP ctf_fs_ds_file_group_create(struct ctf_fs_trace *ctf_fs_trace,
                                                               struct ctf_stream_class *sc,
                                                               uint64_t stream_instance_id,
                                                               ctf_fs_ds_index::UP index)
{
    ctf_fs_ds_file_group::UP ds_file_group {new ctf_fs_ds_file_group};

    ds_file_group->index = std::move(index);

    ds_file_group->stream_id = stream_instance_id;
    BT_ASSERT(sc);
    ds_file_group->sc = sc;
    ds_file_group->ctf_fs_trace = ctf_fs_trace;

    return ds_file_group;
}
