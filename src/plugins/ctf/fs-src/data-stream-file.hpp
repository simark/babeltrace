/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2016 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef CTF_FS_DS_FILE_H
#define CTF_FS_DS_FILE_H

#include <stdio.h>
#include <stdbool.h>
#include <glib.h>
#include <memory>
#include <string>
#include "common/macros.h"
#include <babeltrace2/babeltrace.h>
#include <vector>

#include "../common/src/msg-iter/msg-iter.hpp"
#include "../common/src/item-seq/medium.hpp"
#include "lttng-index.hpp"
#include "plugins/ctf/common/logging/log-cfg.hpp"
#include "file.hpp"
#include "../common/src/metadata/ctf-ir.hpp"

struct ctf_fs_component;
struct ctf_fs_file;
struct ctf_fs_trace;
struct ctf_fs_ds_group_medops_data;
struct ctf_fs_ds_file_info;

struct ctf_fs_ds_file_info
{
    using UP = std::unique_ptr<ctf_fs_ds_file_info>;

    ctf_fs_ds_file_info(std::string pathParam, ctf::LogCfg logCfg);

    std::string path;
    bt2_common::DataLen size;

    /* Guaranteed to be set, as opposed to the index. */
    int64_t beginNs = 0;
};

struct ctf_fs_metadata;

struct ctf_fs_ds_file
{
    using UP = std::unique_ptr<ctf_fs_ds_file>;

    explicit ctf_fs_ds_file(const ctf::LogCfg& logCfgParam, const size_t mmapMaxLenParam) noexcept :
        logCfg {logCfgParam}, mmapMaxLen {mmapMaxLenParam}
    {
    }

    ~ctf_fs_ds_file();

    const ctf::LogCfg logCfg;

    ctf_fs_file::UP file;

    void *mmap_addr = nullptr;

    /*
     * Max length of chunk to mmap() when updating the current mapping.
     * This value must be page-aligned.
     */
    const size_t mmapMaxLen;

    /* Length of the current mapping. Never exceeds the file's length. */
    size_t mmap_len = 0;

    /* Offset in the file where the current mapping starts. */
    off_t mmap_offset_in_file = 0;
};

struct ctf_fs_ds_index_entry
{
    ctf_fs_ds_index_entry(const char *pathParam, bt2_common::DataLen offsetInFileParam,
                          bt2_common::DataLen packetSizeParam) :
        path(pathParam),
        offsetInFile(offsetInFileParam), offsetInStream {offsetInFileParam},
        packetSize(packetSizeParam)
    {
        BT_ASSERT(path);
    }

    /* Weak, belongs to ctf_fs_ds_file_info. */
    const char *path;

    /* Position of the packet from the beginning of the file. */
    bt2_common::DataLen offsetInFile;

    /*
     * Position of the packet from the beginning of the stream.  Starts equal
     * to `offsetInFile`, but can change when multiple data stream files
     * belonging to the same stream are merged.
     */
    bt2_common::DataLen offsetInStream;

    /* Size of the packet. */
    bt2_common::DataLen packetSize;

    /*
     * Extracted from the packet context, relative to the respective fields'
     * mapped clock classes (in cycles).
     */
    uint64_t timestamp_begin, timestamp_end = 0;

    /*
     * Converted from the packet context, relative to the trace's EPOCH
     * (in ns since EPOCH).
     */
    int64_t timestamp_begin_ns, timestamp_end_ns = 0;

    /*
     * Packet sequence number, or UINT64_MAX if not present in the index.
     */
    uint64_t packet_seq_num = UINT64_MAX;
};

struct ctf_fs_ds_index
{
    using EntriesT = std::vector<ctf_fs_ds_index_entry>;

    EntriesT entries;

    void updateOffsetsInStream();
};

struct ctf_fs_ds_file_group
{
    using UP = std::unique_ptr<ctf_fs_ds_file_group>;

    ctf_fs_ds_file_group(const ctf::src::DataStreamCls& dataStreamClsParam,
                         uint64_t streamInstanceIdParam, struct ctf_fs_trace *ctfFsTraceParam,
                         ctf_fs_ds_index indexParam);

    /*
     * Array of struct ctf_fs_ds_file_info, owned by this.
     *
     * This is an _ordered_ array of data stream file infos which
     * belong to this group (a single stream instance).
     *
     * You can call ctf_fs_ds_file_create() with one of those paths
     * and the trace IR stream below.
     */
    std::vector<ctf_fs_ds_file_info::UP> ds_file_infos;

    const ctf::src::DataStreamCls& dataStreamCls;

    nonstd::optional<bt2::Stream::Shared> stream;

    /* Stream (instance) ID; -1ULL means none */
    uint64_t stream_id = 0;

    /* Weak, belongs to component */
    struct ctf_fs_trace *ctf_fs_trace = nullptr;

    ctf_fs_ds_index index;
};

BT_HIDDEN
ctf_fs_ds_file::UP ctf_fs_ds_file_create(const char *path, const ctf::LogCfg& logCfg);

BT_HIDDEN
nonstd::optional<ctf_fs_ds_index> ctf_fs_ds_file_build_index(const ctf_fs_ds_file_info& file_info,
                                                             const ctf::src::TraceCls& traceCls,
                                                             const ctf::LogCfg& logCfg);

namespace ctf {
namespace src {
namespace fs {

struct CtfFsMedium : public ctf::src::Medium
{
    CtfFsMedium(const ctf_fs_ds_index& index, const LogCfg& logCfg);

    ctf::src::Buf buf(bt2_common::DataLen offset, bt2_common::DataLen minSize) override;

private:
    ctf_fs_ds_index::EntriesT::const_iterator
    _mFindIndexEntryForOffset(bt2_common::DataLen offsetInStream) const noexcept;

    const ctf_fs_ds_index& _mIndex;
    const LogCfg _mLogCfg;

    ctf_fs_ds_file::UP _mCurrentDsFile;
};

} /* namespace fs */
} /* namespace src */
} /* namespace ctf */

#endif /* CTF_FS_DS_FILE_H */
