/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2015-2017 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * Babeltrace CTF file system Reader Component
 */

#define BT_COMP_LOG_SELF_COMP logCfg.selfComp
#define BT_LOG_OUTPUT_LEVEL   logCfg.logLevel
#define BT_LOG_TAG            "PLUGIN/SRC.CTF.FS"
#include "logging/comp-logging.h"

#include <memory>
#include <new>
#include <queue>
#include <stack>
#include <type_traits>

#include <babeltrace2/babeltrace.h>

#include <glib.h>

#include "common/assert.h"
#include "common/common.h"
#include "common/uuid.h"

#include "fs.hpp"
#include "data-stream-file.hpp"
#include "file.hpp"
#include "query.hpp"
#include "cpp-common/exc.hpp"
#include "cpp-common/make-unique.hpp"
#include "cpp-common/file-utils.hpp"
#include "cpp-common/data-len.hpp"
#include "cpp-common/glib-up.hpp"
#include "plugins/common/param-validation/param-validation.h"

#include "../common/src/item-seq/item-seq-iter.hpp"
#include "../common/src/item-seq/logging-item-visitor.hpp"
#include "../common/src/item-seq/medium.hpp"
#include "../common/src/metadata/ctf-ir.hpp"
#include "../common/src/metadata/tsdl/ctf-meta-configure-ir-trace.hpp"
#include "../common/src/msg-iter/msg-iter.hpp"
#include "../common/src/pkt-props.hpp"

using namespace ctf::src;
using namespace ctf;
using namespace bt2_common::literals::datalen;

struct tracer_info
{
    const char *name;
    int64_t major;
    int64_t minor;
    int64_t patch;
};

BT_HIDDEN
bt_message_iterator_class_next_method_status
ctf_fs_iterator_next(bt_self_message_iterator *iterator, bt_message_array_const msgs,
                     uint64_t capacity, uint64_t *count)
{
    struct ctf_fs_msg_iter_data *msg_iter_data =
        (struct ctf_fs_msg_iter_data *) bt_self_message_iterator_get_data(iterator);
    uint64_t i = 0;

    if (G_UNLIKELY(msg_iter_data->next_saved_error)) {
        /*
         * Last time we were called, we hit an error but had some
         * messages to deliver, so we stashed the error here.  Return
         * it now.
         */
        BT_CURRENT_THREAD_MOVE_ERROR_AND_RESET(msg_iter_data->next_saved_error);
        return msg_iter_data->next_saved_status;
    }

    bt_message_iterator_class_next_method_status status =
        BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK;

    do {
        try {
            bt2::ConstMessage::Shared msg = msg_iter_data->msgIter->next();
            msgs[i] = msg.release().libObjPtr();
            ++i;
        } catch (const bt2::Error& error) {
            status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_ERROR;
            break;
        } catch (const std::bad_alloc&) {
            status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_END;
            break;
        } catch (const MsgIterEnded&) {
            status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_END;
            break;
        }
    } while (i < capacity && status == BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK);

    if (i > 0) {
        /*
             * Even if ctf_fs_iterator_next_one() returned something
             * else than BT_MESSAGE_ITERATOR_NEXT_METHOD_STATUS_OK, we
             * accumulated message objects in the output
             * message array, so we need to return
             * BT_MESSAGE_ITERATOR_NEXT_METHOD_STATUS_OK so that they are
             * transfered to downstream. This other status occurs
             * again the next time muxer_msg_iter_do_next() is
             * called, possibly without any accumulated
             * message, in which case we'll return it.
             */
        if (status < 0) {
            /*
                 * Save this error for the next _next call.  Assume that
                 * this component always appends error causes when
                 * returning an error status code, which will cause the
                 * current thread error to be non-NULL.
                 */
            msg_iter_data->next_saved_error = bt_current_thread_take_error();
            BT_ASSERT(msg_iter_data->next_saved_error);
            msg_iter_data->next_saved_status = status;
        }

        *count = i;
        status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK;
    }

    return status;
}

static void instantiateMsgIter(ctf_fs_msg_iter_data *msg_iter_data)
{
    const LogCfg& logCfg = msg_iter_data->logCfg;
    ctf_fs_ds_file_group *ds_file_group = msg_iter_data->port_data->ds_file_group;

    ctf::src::Medium::UP medium =
        bt2_common::makeUnique<ctf::src::fs::CtfFsMedium>(ds_file_group->index, logCfg);
    msg_iter_data->msgIter.emplace(msg_iter_data->self_msg_iter,
                                   *ds_file_group->ctf_fs_trace->irGenerator.ctfTraceCls(),
                                   **ds_file_group->stream, std::move(medium),
                                   msg_iter_data->port_data->ctf_fs->quirks, logCfg);
}

BT_HIDDEN
bt_message_iterator_class_seek_beginning_method_status
ctf_fs_iterator_seek_beginning(bt_self_message_iterator *self_msg_iter)
{
    ctf_fs_msg_iter_data *msg_iter_data =
        (ctf_fs_msg_iter_data *) bt_self_message_iterator_get_data(self_msg_iter);
    BT_ASSERT(msg_iter_data);

    const ctf::LogCfg& logCfg = msg_iter_data->logCfg;

    try {
        instantiateMsgIter(msg_iter_data);

        return BT_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHOD_STATUS_OK;
    } catch (const std::bad_alloc&) {
        return BT_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHOD_STATUS_MEMORY_ERROR;
    } catch (const bt2_common::Error&) {
        BT_COMP_LOGE_APPEND_CAUSE(logCfg.selfComp, "Failed to seek beginning");
        return BT_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHOD_STATUS_ERROR;
    }
}

BT_HIDDEN
void ctf_fs_iterator_finalize(bt_self_message_iterator *it)
{
    ctf_fs_msg_iter_data::UP {(ctf_fs_msg_iter_data *) bt_self_message_iterator_get_data(it)};
}

BT_HIDDEN
bt_message_iterator_class_initialize_method_status
ctf_fs_iterator_init(bt_self_message_iterator *self_msg_iter,
                     bt_self_message_iterator_configuration *config,
                     bt_self_component_port_output *self_port)
{
    bt_self_component_port *self_comp_port =
        bt_self_component_port_output_as_self_component_port(self_port);
    ctf_fs_port_data *port_data =
        (ctf_fs_port_data *) bt_self_component_port_get_data(self_comp_port);
    BT_ASSERT(port_data);

    const ctf::LogCfg& logCfg = port_data->ctf_fs->logCfg;

    try {
        ctf_fs_msg_iter_data::UP msg_iter_data =
            bt2_common::makeUnique<ctf_fs_msg_iter_data>(logCfg);

        msg_iter_data->self_msg_iter = self_msg_iter;
        msg_iter_data->port_data = port_data;

        instantiateMsgIter(msg_iter_data.get());

        /*
         * This iterator can seek forward if its stream class has a default
         * clock class.
         */
        if (msg_iter_data->port_data->ds_file_group->dataStreamCls.defClkCls()) {
            bt_self_message_iterator_configuration_set_can_seek_forward(config, true);
        }

        bt_self_message_iterator_set_data(self_msg_iter, msg_iter_data.release());

        return BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_OK;
    } catch (const std::bad_alloc&) {
        return BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR;
    } catch (const bt2_common::Error&) {
        BT_COMP_LOGE_APPEND_CAUSE(logCfg.selfComp, "Failed to initialize iterator");
        return BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
    }
}

void ctf_fs_finalize(bt_self_component_source *component)
{
    ctf_fs_component::UP {(ctf_fs_component *) bt_self_component_get_data(
        bt_self_component_source_as_self_component(component))};
}

std::string ctf_fs_make_port_name(struct ctf_fs_ds_file_group *ds_file_group)
{
    std::stringstream ss;

    /*
     * The unique port name is generated by concatenating unique identifiers
     * for:
     *
     *   - the trace
     *   - the stream class
     *   - the stream
     */

    /* For the trace, use the uuid if present, else the path. */
    nonstd::optional<bt2_common::Uuid> uuid =
        ds_file_group->ctf_fs_trace->irGenerator.ctfTraceCls()->uuid();
    if (uuid) {
        ss << uuid->str();
    } else {
        ss << ds_file_group->ctf_fs_trace->path;
    }

    /*
     * For the stream class, use the id if present.  We can omit this field
     * otherwise, as there will only be a single stream class.
     */
    if (ds_file_group->dataStreamCls.id() != UINT64_C(-1)) {
        ss << " | " << ds_file_group->dataStreamCls.id();
    }

    /* For the stream, use the id if present, else, use the path. */
    if (ds_file_group->stream_id != UINT64_C(-1)) {
        ss << " | " << ds_file_group->stream_id;
    } else {
        BT_ASSERT(ds_file_group->ds_file_infos.size() == 1);
        ctf_fs_ds_file_info *ds_file_info = ds_file_group->ds_file_infos[0].get();
        ss << " | " << ds_file_info->path;
    }

    return ss.str();
}

static int create_one_port_for_trace(struct ctf_fs_component *ctf_fs,
                                     struct ctf_fs_trace *ctf_fs_trace,
                                     struct ctf_fs_ds_file_group *ds_file_group,
                                     bt_self_component_source *self_comp_src)
{
    const ctf::LogCfg& logCfg = ctf_fs->logCfg;
    std::string port_name = ctf_fs_make_port_name(ds_file_group);

    BT_COMP_LOGI("Creating one port named `%s`", port_name.c_str());

    /* Create output port for this file */
    ctf_fs_port_data::UP port_data = bt2_common::makeUnique<ctf_fs_port_data>();
    port_data->ctf_fs = ctf_fs;
    port_data->ds_file_group = ds_file_group;
    int ret = bt_self_component_source_add_output_port(self_comp_src, port_name.c_str(),
                                                       port_data.get(), NULL);
    if (ret) {
        return ret;
    }

    ctf_fs->port_data.emplace_back(std::move(port_data));
    return 0;
}

static int create_ports_for_trace(struct ctf_fs_component *ctf_fs,
                                  struct ctf_fs_trace *ctf_fs_trace,
                                  bt_self_component_source *self_comp_src)
{
    const ctf::LogCfg& logCfg = ctf_fs->logCfg;

    /* Create one output port for each stream file group */
    for (ctf_fs_ds_file_group::UP& ds_file_group : ctf_fs_trace->ds_file_groups) {
        int ret =
            create_one_port_for_trace(ctf_fs, ctf_fs_trace, ds_file_group.get(), self_comp_src);
        if (ret) {
            BT_COMP_LOGE_APPEND_CAUSE(logCfg.selfComp, "Cannot create output port.");
            return ret;
        }
    }

    return 0;
}

/*
 * Insert ds_file_info in ds_file_group's list of ds_file_infos at the right
 * place to keep it sorted.
 */

static void ds_file_group_insert_ds_file_info_sorted(struct ctf_fs_ds_file_group *ds_file_group,
                                                     ctf_fs_ds_file_info::UP ds_file_info)
{
    /* Find the spot where to insert this ds_file_info. */
    auto it = ds_file_group->ds_file_infos.begin();

    for (; it != ds_file_group->ds_file_infos.end(); ++it) {
        ctf_fs_ds_file_info *other_ds_file_info = it->get();

        if (ds_file_info->beginNs < other_ds_file_info->beginNs) {
            break;
        }
    }

    ds_file_group->ds_file_infos.insert(it, std::move(ds_file_info));
}

static bool ds_index_entries_equal(const ctf_fs_ds_index_entry& left,
                                   const ctf_fs_ds_index_entry& right)
{
    if (left.packetSize != right.packetSize) {
        return false;
    }

    if (left.timestamp_begin != right.timestamp_begin) {
        return false;
    }

    if (left.timestamp_end != right.timestamp_end) {
        return false;
    }

    if (left.packet_seq_num != right.packet_seq_num) {
        return false;
    }

    return true;
}

/*
 * Insert `entry` into `index`, without duplication.
 *
 * The entry is inserted only if there isn't an identical entry already.
 *
 * In any case, the ownership of `entry` is transferred to this function.  So if
 * the entry is not inserted, it is freed.
 */

static void ds_index_insert_ds_index_entry_sorted(ctf_fs_ds_index& index,
                                                  const ctf_fs_ds_index_entry& entry)
{
    /* Find the spot where to insert this index entry. */
    ctf_fs_ds_index::EntriesT::const_iterator otherEntry = index.entries.begin();
    for (; otherEntry != index.entries.end(); ++otherEntry) {
        if (entry.timestamp_begin_ns <= otherEntry->timestamp_begin_ns) {
            break;
        }
    }

    /*
     * Insert the entry only if a duplicate doesn't already exist.
     *
     * There can be duplicate packets if reading multiple overlapping
     * snapshots of the same trace.  We then want the index to contain
     * a reference to only one copy of that packet.
     */
    if (otherEntry == index.entries.end() || !ds_index_entries_equal(entry, *otherEntry)) {
        index.entries.insert(otherEntry, entry);
    }
}

static void merge_ctf_fs_ds_indexes(ctf_fs_ds_index& dest, ctf_fs_ds_index src)
{
    for (auto it = src.entries.begin(); it != src.entries.end(); ++it) {
        /*
		* Ownership of the ctf_fs_ds_index_entry is transferred to
		* ds_index_insert_ds_index_entry_sorted.
		*/
        ds_index_insert_ds_index_entry_sorted(dest, std::move(*it));
    }
}

static int add_ds_file_to_ds_file_group(struct ctf_fs_trace *ctf_fs_trace, const char *path)
{
    int64_t begin_ns = -1;
    ctf_fs_ds_file_group *ds_file_group = nullptr;
    ;
    int ret;

    const TraceCls& traceCls = *ctf_fs_trace->irGenerator.ctfTraceCls();

    const LogCfg& logCfg = ctf_fs_trace->logCfg;

    ctf_fs_ds_file_info::UP ds_file_info =
        bt2_common::makeUnique<ctf_fs_ds_file_info>(path, logCfg);

    ctf_fs_ds_index tempIndex;
    ctf_fs_ds_index_entry tempIndexEntry {path, 0_bytes, ds_file_info->size};
    tempIndex.entries.emplace_back(tempIndexEntry);
    Medium::UP medium = bt2_common::makeUnique<fs::CtfFsMedium>(tempIndex, logCfg);
    PktProps props = readPktProps(traceCls, std::move(medium), 0_bytes);

    const ctf::src::DataStreamCls *sc = props.dataStreamCls;
    BT_ASSERT(sc);
    nonstd::optional<unsigned long long> stream_instance_id = props.dataStreamId;

    if (props.snapshots.beginDefClk) {
        BT_ASSERT(sc->defClkCls());
        ret = bt_util_clock_cycles_to_ns_from_origin(
            *props.snapshots.beginDefClk, sc->defClkCls()->freq(),
            sc->defClkCls()->offset().seconds(), sc->defClkCls()->offset().cycles(), &begin_ns);
        if (ret) {
            BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(
                logCfg.selfComp, logCfg.selfCompClass,
                "Cannot convert clock cycles to nanoseconds from origin (`%s`).", path);
            return ret;
        }
    }

    nonstd::optional<ctf_fs_ds_index> index =
        ctf_fs_ds_file_build_index(*ds_file_info, traceCls, logCfg);
    if (!index) {
        BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(logCfg.selfComp, logCfg.selfCompClass,
                                                "Failed to index CTF stream file \'%s\'", path);
        return -1;
    }

    if (begin_ns == -1) {
        /*
         * No beginning timestamp to sort the stream files
         * within a stream file group, so consider that this
         * file must be the only one within its group.
         */
        stream_instance_id.reset();
    }

    if (!stream_instance_id) {
        /*
         * No stream instance ID or no beginning timestamp:
         * create a unique stream file group for this stream
         * file because, even if there's a stream instance ID,
         * there's no timestamp to order the file within its
         * group.
         */
        ctf_fs_ds_file_group::UP new_ds_file_group = bt2_common::makeUnique<ctf_fs_ds_file_group>(
            *sc, UINT64_C(-1), ctf_fs_trace, std::move(*index));

        ds_file_group_insert_ds_file_info_sorted(new_ds_file_group.get(), std::move(ds_file_info));
        ctf_fs_trace->ds_file_groups.emplace_back(std::move(new_ds_file_group));
        return 0;
    }

    BT_ASSERT(stream_instance_id);
    BT_ASSERT(begin_ns != -1);

    /* Find an existing stream file group with this ID */
    for (ctf_fs_ds_file_group::UP& group : ctf_fs_trace->ds_file_groups) {
        if (&group->dataStreamCls == sc && group->stream_id == stream_instance_id) {
            ds_file_group = group.get();
            break;
        }
    }

    if (!ds_file_group) {
        ctf_fs_ds_file_group::UP new_ds_file_group = bt2_common::makeUnique<ctf_fs_ds_file_group>(
            *sc, *stream_instance_id, ctf_fs_trace, std::move(*index));

        ds_file_group = new_ds_file_group.get();
        ctf_fs_trace->ds_file_groups.emplace_back(std::move(new_ds_file_group));

    } else {
        merge_ctf_fs_ds_indexes(ds_file_group->index, std::move(*index));
    }

    ds_file_group_insert_ds_file_info_sorted(ds_file_group, std::move(ds_file_info));

    return 0;
}

#define CTF_FS_METADATA_FILENAME "metadata"

static int create_ds_file_groups(struct ctf_fs_trace *ctf_fs_trace)
{
    const char *basename;
    GError *error = NULL;
    const ctf::LogCfg& logCfg = ctf_fs_trace->logCfg;

    /* Check each file in the path directory, except specific ones */
    bt2_common::GDirUP dir {g_dir_open(ctf_fs_trace->path.c_str(), 0, &error)};
    if (!dir) {
        BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(
            logCfg.selfComp, logCfg.selfCompClass, "Cannot open directory `%s`: %s (code %d)",
            ctf_fs_trace->path.c_str(), error->message, error->code);
        if (error) {
            g_error_free(error);
        }
        return -1;
    }

    while ((basename = g_dir_read_name(dir.get()))) {
        if (strcmp(basename, CTF_FS_METADATA_FILENAME) == 0) {
            /* Ignore the metadata stream. */
            BT_COMP_LOGI("Ignoring metadata file `%s" G_DIR_SEPARATOR_S "%s`",
                         ctf_fs_trace->path.c_str(), basename);
            continue;
        }

        if (basename[0] == '.') {
            BT_COMP_LOGI("Ignoring hidden file `%s" G_DIR_SEPARATOR_S "%s`",
                         ctf_fs_trace->path.c_str(), basename);
            continue;
        }

        /* Create the file. */
        ctf_fs_file::UP file = bt2_common::makeUnique<ctf_fs_file>(logCfg);
        if (!file) {
            BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(
                logCfg.selfComp, logCfg.selfCompClass,
                "Cannot create stream file object for file `%s" G_DIR_SEPARATOR_S "%s`",
                ctf_fs_trace->path.c_str(), basename);
            return -1;
        }

        /* Create full path string. */
        file->path = ctf_fs_trace->path;
        file->path += G_DIR_SEPARATOR_S;
        file->path += basename;
        if (!g_file_test(file->path.c_str(), G_FILE_TEST_IS_REGULAR)) {
            BT_COMP_LOGI("Ignoring non-regular file `%s`", file->path.c_str());
            continue;
        }

        int ret = ctf_fs_file_open(file.get(), "rb");
        if (ret) {
            BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(logCfg.selfComp, logCfg.selfCompClass,
                                                    "Cannot open stream file `%s`",
                                                    file->path.c_str());
            return ret;
        }

        if (file->size == 0) {
            /* Skip empty stream. */
            BT_COMP_LOGI("Ignoring empty file `%s`", file->path.c_str());
            continue;
        }

        ret = add_ds_file_to_ds_file_group(ctf_fs_trace, file->path.c_str());
        if (ret) {
            BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(
                logCfg.selfComp, logCfg.selfCompClass,
                "Cannot add stream file `%s` to stream file group", file->path.c_str());
            return ret;
        }
    }

    return 0;
}

static int set_trace_name(bt2::Trace trace, const char *name_suffix, const ctf::LogCfg& logCfg)
{
    const bt_value *val;
    std::string name;

    /*
     * Check if we have a trace environment string value named `hostname`.
     * If so, use it as the trace name's prefix.
     */
    val = bt_trace_borrow_environment_entry_value_by_name_const(trace.libObjPtr(), "hostname");
    if (val && bt_value_is_string(val)) {
        name += bt_value_string_get(val);

        if (name_suffix) {
            name += G_DIR_SEPARATOR;
        }
    }

    if (name_suffix) {
        name += name_suffix;
    }

    return bt_trace_set_name(trace.libObjPtr(), name.c_str());
}

static ctf_fs_trace::UP ctf_fs_trace_create(const char *path, const char *name,
                                            const ClkClsCfg clkClsCfg, bt_self_component *selfComp,
                                            const ctf::LogCfg& logCfg)
{
    ctf_fs_trace::UP ctf_fs_trace = bt2_common::makeUnique<struct ctf_fs_trace>(logCfg, clkClsCfg);
    ctf_fs_trace->path = path;

    std::string metadataPath = ctf_fs_trace->path;
    metadataPath += G_DIR_SEPARATOR;
    metadataPath += CTF_FS_METADATA_FILENAME;

    std::vector<uint8_t> contents = bt2_common::dataFromFile(metadataPath.c_str());
    ctf_fs_trace->irGenerator.appendContent(contents.data(),
                                            bt2_common::DataLen::fromBytes(contents.size()));

    BT_ASSERT(ctf_fs_trace->irGenerator.ctfTraceCls());

    if (ctf_fs_trace->irGenerator.irTraceCls()) {
        bt2::TraceClass traceCls = *ctf_fs_trace->irGenerator.irTraceCls();
        ctf_fs_trace->trace = traceCls.instantiate();
    }

    if (ctf_fs_trace->trace) {
        int ret = ctf_trace_class_configure_ir_trace(*ctf_fs_trace->irGenerator.ctfTraceCls(),
                                                     **ctf_fs_trace->trace);
        if (ret) {
            return nullptr;
        }

        ret = set_trace_name(**ctf_fs_trace->trace, name, logCfg);
        if (ret) {
            return nullptr;
        }
    }

    int ret = create_ds_file_groups(ctf_fs_trace.get());
    if (ret) {
        return nullptr;
    }

    return ctf_fs_trace;
}

static int path_is_ctf_trace(const char *path)
{
    std::string metadata_path = path;
    metadata_path += G_DIR_SEPARATOR;
    metadata_path += CTF_FS_METADATA_FILENAME;

    return g_file_test(metadata_path.c_str(), G_FILE_TEST_IS_REGULAR);
}

/* Helper for ctf_fs_component_create_ctf_fs_trace, to handle a single path. */

static int ctf_fs_component_create_ctf_fs_trace_one_path(struct ctf_fs_component *ctf_fs,
                                                         const char *path_param,
                                                         const char *trace_name,
                                                         std::vector<ctf_fs_trace::UP>& traces,
                                                         bt_self_component *selfComp)
{
    const ctf::LogCfg& logCfg = ctf_fs->logCfg;

    bt2_common::GStringUP norm_path(bt_common_normalize_path(path_param, NULL));
    if (!norm_path) {
        BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(logCfg.selfComp, logCfg.selfCompClass,
                                                "Failed to normalize path: `%s`.", path_param);
        return -1;
    }

    int ret = path_is_ctf_trace(norm_path->str);
    if (ret < 0) {
        BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(logCfg.selfComp, logCfg.selfCompClass,
                                                "Failed to check if path is a CTF trace: path=%s",
                                                norm_path->str);
        return -1;
    } else if (ret == 0) {
        BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(
            logCfg.selfComp, logCfg.selfCompClass,
            "Path is not a CTF trace (does not contain a metadata file): `%s`.", norm_path->str);
        return -1;
    }

    // FIXME: Remove or ifdef for __MINGW32__
    if (strcmp(norm_path->str, "/") == 0) {
        BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(logCfg.selfComp, logCfg.selfCompClass,
                                                "Opening a trace in `/` is not supported.");
        return -1;
    }

    ctf_fs_trace::UP ctf_fs_trace =
        ctf_fs_trace_create(norm_path->str, trace_name, ctf_fs->clkClsCfg, selfComp, logCfg);
    if (!ctf_fs_trace) {
        BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(logCfg.selfComp, logCfg.selfCompClass,
                                                "Cannot create trace for `%s`.", norm_path->str);
        return -1;
    }

    traces.emplace_back(std::move(ctf_fs_trace));
    return 0;
}

/*
 * Count the number of stream and event classes defined by this trace's metadata.
 *
 * This is used to determine which metadata is the "latest", out of multiple
 * traces sharing the same UUID.  It is assumed that amongst all these metadatas,
 * a bigger metadata is a superset of a smaller metadata.  Therefore, it is
 * enough to just count the classes.
 */

static unsigned int metadata_count_stream_and_event_classes(struct ctf_fs_trace *trace)
{
    const TraceCls::DataStreamClsSet& dataStreamClasses =
        trace->irGenerator.ctfTraceCls()->dataStreamClasses();
    unsigned int num = dataStreamClasses.size();

    for (const DataStreamCls::UP& dsc : dataStreamClasses) {
        num += dsc->eventRecordClasses().size();
    }

    return num;
}

/*
 * Merge the src ds_file_group into dest.  This consists of merging their
 * ds_file_infos, making sure to keep the result sorted.
 */

static void merge_ctf_fs_ds_file_groups(struct ctf_fs_ds_file_group *dest,
                                        ctf_fs_ds_file_group::UP src)
{
    for (ctf_fs_ds_file_info::UP& ds_file_info : src->ds_file_infos) {
        /* Ownership of the ds_file_info is transferred to dest. */
        ds_file_group_insert_ds_file_info_sorted(dest, std::move(ds_file_info));
    }

    /* Merge both indexes. */
    merge_ctf_fs_ds_indexes(dest->index, std::move(src->index));
}

/* Merge src_trace's data stream file groups into dest_trace's. */

static int merge_matching_ctf_fs_ds_file_groups(struct ctf_fs_trace *dest_trace,
                                                ctf_fs_trace::UP src_trace)
{
    std::vector<ctf_fs_ds_file_group::UP>& dest = dest_trace->ds_file_groups;
    std::vector<ctf_fs_ds_file_group::UP>& src = src_trace->ds_file_groups;

    /*
     * Save the initial length of dest: we only want to check against the
     * original elements in the inner loop.
     */
    size_t dest_len = dest.size();

    for (ctf_fs_ds_file_group::UP& src_group : src) {
        ctf_fs_ds_file_group *dest_group = nullptr;

        /* A stream instance without ID can't match a stream in the other trace.  */
        if (src_group->stream_id != -1) {
            /* Let's search for a matching ds_file_group in the destination.  */
            for (size_t d_i = 0; d_i < dest_len; ++d_i) {
                ctf_fs_ds_file_group *candidate_dest = dest[d_i].get();

                /* Can't match a stream instance without ID.  */
                if (candidate_dest->stream_id == -1) {
                    continue;
                }

                /*
                 * If the two groups have the same stream instance id
                 * and belong to the same stream class (stream instance
                 * ids are per-stream class), they represent the same
                 * stream instance.
                 */
                if (candidate_dest->stream_id != src_group->stream_id ||
                    candidate_dest->dataStreamCls.id() != src_group->dataStreamCls.id()) {
                    continue;
                }

                dest_group = candidate_dest;
                break;
            }
        }

        /*
         * Didn't find a friend in dest to merge our src_group into?
         * Create a new empty one. This can happen if a stream was
         * active in the source trace chunk but not in the destination
         * trace chunk.
         */
        if (!dest_group) {
            const DataStreamCls *sc =
                (*dest_trace->irGenerator.ctfTraceCls())[src_group->dataStreamCls.id()];
            BT_ASSERT(sc);

            ctf_fs_ds_index index;
            ctf_fs_ds_file_group::UP new_dest_group = bt2_common::makeUnique<ctf_fs_ds_file_group>(
                *sc, src_group->stream_id, dest_trace, std::move(index));
            if (!new_dest_group) {
                return -1;
            }

            dest_group = new_dest_group.get();
            dest_trace->ds_file_groups.emplace_back(std::move(new_dest_group));
        }

        BT_ASSERT(dest_group);
        merge_ctf_fs_ds_file_groups(dest_group, std::move(src_group));
    }

    return 0;
}

/*
 * Collapse the given traces, which must all share the same UUID, in a single
 * one.
 *
 * The trace with the most expansive metadata is chosen and all other traces
 * are merged into that one.  On return, the elements of `traces` are nullptr
 * and the merged trace is placed in `out_trace`.
 */

static int merge_ctf_fs_traces(std::vector<ctf_fs_trace::UP> traces, ctf_fs_trace::UP& out_trace)
{
    unsigned int winner_count;
    struct ctf_fs_trace *winner;
    guint i, winner_i;

    BT_ASSERT(traces.size() >= 2);

    winner_count = metadata_count_stream_and_event_classes(traces[0].get());
    winner = traces[0].get();
    winner_i = 0;

    /* Find the trace with the largest metadata. */
    for (i = 1; i < traces.size(); i++) {
        ctf_fs_trace *candidate = traces[i].get();

        /* A bit of sanity check. */
        BT_ASSERT(winner->irGenerator.ctfTraceCls()->uuid() ==
                  candidate->irGenerator.ctfTraceCls()->uuid());

        unsigned int candidate_count = metadata_count_stream_and_event_classes(candidate);

        if (candidate_count > winner_count) {
            winner_count = candidate_count;
            winner = candidate;
            winner_i = i;
        }
    }

    /* Merge all the other traces in the winning trace. */
    for (ctf_fs_trace::UP& trace : traces) {
        /* Don't merge the winner into itself. */
        if (trace.get() == winner) {
            continue;
        }

        /* Merge trace's data stream file groups into winner's. */
        int ret = merge_matching_ctf_fs_ds_file_groups(winner, std::move(trace));
        if (ret) {
            return ret;
        }
    }

    /*
     * Move the winner out of the array, into `out_trace`.
     */
    out_trace = std::move(traces[winner_i]);

    return 0;
}

enum target_event
{
    FIRST_EVENT,
    LAST_EVENT,
};

struct ClockSnapshotAfterEventItemVisitor : public ItemVisitor
{
    bool done() const
    {
        return _mDone;
    }

    nonstd::optional<unsigned long long> result() const
    {
        return _mResult;
    }

protected:
    nonstd::optional<unsigned long long> _mResult;
    bool _mDone = false;
};

struct ClockSnapshotAfterFirstEventItemVisitor : public ClockSnapshotAfterEventItemVisitor
{
    void visit(const EventRecordInfoItem& item) override
    {
        _mResult = item.defClkVal();
        _mDone = true;
    }
};

/*
 * Find the timestamp of the last event of the packet, if any, otherwise
 * find the timestamp of the beginning of the packet.
 */
struct ClockSnapshotAfterLastEventItemVisitor : public ClockSnapshotAfterEventItemVisitor
{
    void visit(const PktInfoItem& item) override
    {
        _mLastSeen = item.beginDefClkVal();
    }

    void visit(const EventRecordInfoItem& item) override
    {
        _mLastSeen = item.defClkVal();
    }

    void visit(const PktEndItem& item) override
    {
        _mResult = _mLastSeen;
        _mDone = true;
    }

private:
    nonstd::optional<unsigned long long> _mLastSeen;
};

static int decode_clock_snapshot_after_event(struct ctf_fs_trace *ctf_fs_trace,
                                             const ClkCls& defaultCC,
                                             const ctf_fs_ds_index_entry& indexEntry,
                                             enum target_event target_event, uint64_t *cs,
                                             int64_t *ts_ns)
{
    int ret = 0;
    const LogCfg& logCfg = ctf_fs_trace->logCfg;

    BT_ASSERT(ctf_fs_trace);
    BT_ASSERT(ctf_fs_trace->irGenerator.ctfTraceCls());
    BT_ASSERT(indexEntry.path);

    ctf_fs_ds_index tempIndex;
    tempIndex.entries.emplace_back(indexEntry);
    Medium::UP medium = bt2_common::makeUnique<fs::CtfFsMedium>(tempIndex, logCfg);
    ItemSeqIter itemSeqIter(std::move(medium), *ctf_fs_trace->irGenerator.ctfTraceCls(),
                            indexEntry.offsetInFile);

    std::unique_ptr<ClockSnapshotAfterEventItemVisitor> visitor;
    switch (target_event) {
    case FIRST_EVENT:
        visitor = bt2_common::makeUnique<ClockSnapshotAfterFirstEventItemVisitor>();
        break;

    case LAST_EVENT:
        visitor = bt2_common::makeUnique<ClockSnapshotAfterLastEventItemVisitor>();
        break;

    default:
        bt_common_abort();
    }

    LoggingItemVisitor loggingVisitor(logCfg);

    while (!visitor->done()) {
        if (BT_LOG_ON_TRACE) {
            itemSeqIter->accept(loggingVisitor);
        }
        itemSeqIter->accept(*visitor);
        ++itemSeqIter;
    }

    if (!visitor->result()) {
        BT_COMP_LOGE_APPEND_CAUSE(logCfg.selfComp, "Failed to get %s event clock snapshot.",
                                  target_event == FIRST_EVENT ? "first" : "last");
        return -1;
    }

    *cs = *visitor->result();

    /* Convert clock snapshot to timestamp. */
    ret = bt_util_clock_cycles_to_ns_from_origin(
        *cs, defaultCC.freq(), defaultCC.offset().seconds(), defaultCC.offset().cycles(), ts_ns);
    if (ret) {
        BT_COMP_LOGE_APPEND_CAUSE(logCfg.selfComp, "Failed to convert clock snapshot to timestamp");
        return ret;
    }

    return ret;
}

static int decode_packet_first_event_timestamp(struct ctf_fs_trace *ctf_fs_trace,
                                               const ClkCls& defaultCC,
                                               const ctf_fs_ds_index_entry& index_entry,
                                               uint64_t *cs, int64_t *ts_ns)
{
    return decode_clock_snapshot_after_event(ctf_fs_trace, defaultCC, index_entry, FIRST_EVENT, cs,
                                             ts_ns);
}

static int decode_packet_last_event_timestamp(struct ctf_fs_trace *ctf_fs_trace,
                                              const ClkCls& defaultCC,
                                              const ctf_fs_ds_index_entry& index_entry,
                                              uint64_t *cs, int64_t *ts_ns)
{
    return decode_clock_snapshot_after_event(ctf_fs_trace, defaultCC, index_entry, LAST_EVENT, cs,
                                             ts_ns);
}

/*
 * Fix up packet index entries for lttng's "event-after-packet" bug.
 * Some buggy lttng tracer versions may emit events with a timestamp that is
 * larger (after) than the timestamp_end of the their packets.
 *
 * To fix up this erroneous data we do the following:
 *  1. If it's not the stream file's last packet: set the packet index entry's
 *	end time to the next packet's beginning time.
 *  2. If it's the stream file's last packet, set the packet index entry's end
 *	time to the packet's last event's time, if any, or to the packet's
 *  	beginning time otherwise.
 *
 * Known buggy tracer versions:
 *  - before lttng-ust 2.11.0
 *  - before lttng-module 2.11.0
 *  - before lttng-module 2.10.10
 *  - before lttng-module 2.9.13
 */
static int fix_index_lttng_event_after_packet_bug(struct ctf_fs_trace *trace)
{
    const ctf::LogCfg& logCfg = trace->logCfg;

    for (ctf_fs_ds_file_group::UP& ds_file_group : trace->ds_file_groups) {
        BT_ASSERT(ds_file_group);
        ctf_fs_ds_index& index = ds_file_group->index;

        BT_ASSERT(!index.entries.empty());

        /*
         * Iterate over all entries but the last one. The last one is
         * fixed differently after.
         */
        for (size_t entry_i = 0; entry_i < index.entries.size() - 1; ++entry_i) {
            ctf_fs_ds_index_entry& curr_entry = index.entries[entry_i];
            ctf_fs_ds_index_entry& next_entry = index.entries[entry_i + 1];

            /*
             * 1. Set the current index entry `end` timestamp to
             * the next index entry `begin` timestamp.
             */
            curr_entry.timestamp_end = next_entry.timestamp_begin;
            curr_entry.timestamp_end_ns = next_entry.timestamp_begin_ns;
        }

        /*
         * 2. Fix the last entry by decoding the last event of the last
         * packet.
         */
        ctf_fs_ds_index_entry& last_entry = index.entries.back();

        BT_ASSERT(ds_file_group->dataStreamCls.defClkCls());
        const ClkCls& defaultCC = *ds_file_group->dataStreamCls.defClkCls();

        /*
         * Decode packet to read the timestamp of the last event of the
         * entry.
         */
        int ret = decode_packet_last_event_timestamp(
            trace, defaultCC, last_entry, &last_entry.timestamp_end, &last_entry.timestamp_end_ns);
        if (ret) {
            BT_COMP_LOGE_APPEND_CAUSE(
                logCfg.selfComp,
                "Failed to decode stream's last packet to get its last event's clock snapshot.");
            return ret;
        }
    }

    return 0;
}

/*
 * Fix up packet index entries for barectf's "event-before-packet" bug.
 * Some buggy barectf tracer versions may emit events with a timestamp that is
 * less than the timestamp_begin of the their packets.
 *
 * To fix up this erroneous data we do the following:
 *  1. Starting at the second index entry, set the timestamp_begin of the
 *     current entry to the timestamp of the first event of the packet.
 *  2. Set the previous entry's timestamp_end to the timestamp_begin of the
 *     current packet.
 *
 * Known buggy tracer versions:
 *  - before barectf 2.3.1
 */
static int fix_index_barectf_event_before_packet_bug(struct ctf_fs_trace *trace)
{
    const ctf::LogCfg& logCfg = trace->logCfg;

    for (ctf_fs_ds_file_group::UP& ds_file_group : trace->ds_file_groups) {
        ctf_fs_ds_index& index = ds_file_group->index;

        BT_ASSERT(!index.entries.empty());

        BT_ASSERT(ds_file_group->dataStreamCls.defClkCls());
        const ClkCls& defaultCC = *ds_file_group->dataStreamCls.defClkCls();

        /*
         * 1. Iterate over the index, starting from the second entry
         * (index = 1).
         */
        for (size_t entry_i = 1; entry_i < index.entries.size(); ++entry_i) {
            ctf_fs_ds_index_entry& prev_entry = index.entries[entry_i - 1];
            ctf_fs_ds_index_entry& curr_entry = index.entries[entry_i];

            /*
             * 2. Set the current entry `begin` timestamp to the
             * timestamp of the first event of the current packet.
             */
            int ret = decode_packet_first_event_timestamp(trace, defaultCC, curr_entry,
                                                          &curr_entry.timestamp_begin,
                                                          &curr_entry.timestamp_begin_ns);
            if (ret) {
                BT_COMP_LOGE_APPEND_CAUSE(logCfg.selfComp,
                                          "Failed to decode first event's clock snapshot");
                return ret;
            }

            /*
             * 3. Set the previous entry `end` timestamp to the
             * timestamp of the first event of the current packet.
             */
            prev_entry.timestamp_end = curr_entry.timestamp_begin;
            prev_entry.timestamp_end_ns = curr_entry.timestamp_begin_ns;
        }
    }

    return 0;
}

/*
 * When using the lttng-crash feature it's likely that the last packets of each
 * stream have their timestamp_end set to zero. This is caused by the fact that
 * the tracer crashed and was not able to properly close the packets.
 *
 * To fix up this erroneous data we do the following:
 * For each index entry, if the entry's timestamp_end is 0 and the
 * timestamp_begin is not 0:
 *  - If it's the stream file's last packet: set the packet index entry's end
 *    time to the packet's last event's time, if any, or to the packet's
 *    beginning time otherwise.
 *  - If it's not the stream file's last packet: set the packet index
 *    entry's end time to the next packet's beginning time.
 *
 * Affected versions:
 * - All current and future lttng-ust and lttng-modules versions.
 */
static int fix_index_lttng_crash_quirk(struct ctf_fs_trace *trace)
{
    const ctf::LogCfg& logCfg = trace->logCfg;

    for (ctf_fs_ds_file_group::UP& ds_file_group : trace->ds_file_groups) {
        BT_ASSERT(ds_file_group);
        ctf_fs_ds_index& index = ds_file_group->index;

        BT_ASSERT(ds_file_group->dataStreamCls.defClkCls());
        const ClkCls& defaultCC = *ds_file_group->dataStreamCls.defClkCls();

        BT_ASSERT(!index.entries.empty());

        ctf_fs_ds_index_entry& last_entry = index.entries.back();

        /* 1. Fix the last entry first. */
        if (last_entry.timestamp_end == 0 && last_entry.timestamp_begin != 0) {
            /*
             * Decode packet to read the timestamp of the
             * last event of the stream file.
             */
            int ret = decode_packet_last_event_timestamp(trace, defaultCC, last_entry,
                                                         &last_entry.timestamp_end,
                                                         &last_entry.timestamp_end_ns);
            if (ret) {
                BT_COMP_LOGE_APPEND_CAUSE(logCfg.selfComp,
                                          "Failed to decode last event's clock snapshot");
                return ret;
            }
        }

        /* Iterate over all entries but the last one. */
        for (size_t entry_idx = 0; entry_idx < index.entries.size() - 1; ++entry_idx) {
            ctf_fs_ds_index_entry& curr_entry = index.entries[entry_idx];
            ctf_fs_ds_index_entry& next_entry = index.entries[entry_idx + 1];

            if (curr_entry.timestamp_end == 0 && curr_entry.timestamp_begin != 0) {
                /*
                 * 2. Set the current index entry `end` timestamp to
                 * the next index entry `begin` timestamp.
                 */
                curr_entry.timestamp_end = next_entry.timestamp_begin;
                curr_entry.timestamp_end_ns = next_entry.timestamp_begin_ns;
            }
        }
    }

    return 0;
}

/*
 * Extract the tracer information necessary to compare versions.
 * Returns 0 on success, and -1 if the extraction is not successful because the
 * necessary fields are absents in the trace metadata.
 */
static int extract_tracer_info(struct ctf_fs_trace *trace, struct tracer_info *current_tracer_info)
{
    nonstd::optional<bt2::ConstMapValue> optEnv = trace->irGenerator.ctfTraceCls()->env();
    if (!optEnv) {
        return -1;
    }

    bt2::ConstMapValue env = *optEnv;

    /* Clear the current_tracer_info struct */
    memset(current_tracer_info, 0, sizeof(*current_tracer_info));

    /*
     * To compare 2 tracer versions, at least the tracer name and its
     * major version are needed. If one of these is missing, consider it an
     * extraction failure.
     */
    nonstd::optional<bt2::ConstValue> tracerName = env["tracer_name"];
    if (!tracerName || !tracerName->isString()) {
        return -1;
    }

    /* Set tracer name. */
    current_tracer_info->name = tracerName->asString().value().c_str();

    nonstd::optional<bt2::ConstValue> tracerMajor = env["tracer_major"];
    if (!tracerMajor || !tracerMajor->isSignedInteger()) {
        return -1;
    }

    /* Set major version number. */
    current_tracer_info->major = tracerMajor->asSignedInteger().value();

    nonstd::optional<bt2::ConstValue> tracerMinor = env["tracer_minor"];
    if (!tracerMinor || !tracerMinor->isSignedInteger()) {
        return 0;
    }

    /* Set minor version number. */
    current_tracer_info->minor = tracerMinor->asSignedInteger().value();

    /*
     * If `tracer_patch` doesn't exist `tracer_patchlevel` might.
     * For example, `lttng-modules` uses entry name `tracer_patchlevel`.
     */
    nonstd::optional<bt2::ConstValue> tracerPatch = env["tracer_patch"];
    if (!tracerPatch)
        tracerPatch = env["tracer_patchlevel"];

    if (!tracerPatch || !tracerPatch->isSignedInteger()) {
        return 0;
    }

    /* Set patch version number. */
    current_tracer_info->patch = tracerPatch->asSignedInteger().value();

    return 0;
}

static bool is_tracer_affected_by_lttng_event_after_packet_bug(struct tracer_info *curr_tracer_info)
{
    bool is_affected = false;

    if (strcmp(curr_tracer_info->name, "lttng-ust") == 0) {
        if (curr_tracer_info->major < 2) {
            is_affected = true;
        } else if (curr_tracer_info->major == 2) {
            /* fixed in lttng-ust 2.11.0 */
            if (curr_tracer_info->minor < 11) {
                is_affected = true;
            }
        }
    } else if (strcmp(curr_tracer_info->name, "lttng-modules") == 0) {
        if (curr_tracer_info->major < 2) {
            is_affected = true;
        } else if (curr_tracer_info->major == 2) {
            /* fixed in lttng-modules 2.11.0 */
            if (curr_tracer_info->minor == 10) {
                /* fixed in lttng-modules 2.10.10 */
                if (curr_tracer_info->patch < 10) {
                    is_affected = true;
                }
            } else if (curr_tracer_info->minor == 9) {
                /* fixed in lttng-modules 2.9.13 */
                if (curr_tracer_info->patch < 13) {
                    is_affected = true;
                }
            } else if (curr_tracer_info->minor < 9) {
                is_affected = true;
            }
        }
    }

    return is_affected;
}

static bool
is_tracer_affected_by_barectf_event_before_packet_bug(struct tracer_info *curr_tracer_info)
{
    bool is_affected = false;

    if (strcmp(curr_tracer_info->name, "barectf") == 0) {
        if (curr_tracer_info->major < 2) {
            is_affected = true;
        } else if (curr_tracer_info->major == 2) {
            if (curr_tracer_info->minor < 3) {
                is_affected = true;
            } else if (curr_tracer_info->minor == 3) {
                /* fixed in barectf 2.3.1 */
                if (curr_tracer_info->patch < 1) {
                    is_affected = true;
                }
            }
        }
    }

    return is_affected;
}

static bool is_tracer_affected_by_lttng_crash_quirk(struct tracer_info *curr_tracer_info)
{
    bool is_affected = false;

    /* All LTTng tracer may be affected by this lttng crash quirk. */
    if (strcmp(curr_tracer_info->name, "lttng-ust") == 0) {
        is_affected = true;
    } else if (strcmp(curr_tracer_info->name, "lttng-modules") == 0) {
        is_affected = true;
    }

    return is_affected;
}

/*
 * Looks for trace produced by known buggy tracers and fix up the index
 * produced earlier.
 */
static int fix_packet_index_tracer_bugs(struct ctf_fs_component *ctf_fs)
{
    struct tracer_info current_tracer_info;
    const ctf::LogCfg& logCfg = ctf_fs->logCfg;

    int ret = extract_tracer_info(ctf_fs->trace.get(), &current_tracer_info);
    if (ret) {
        /*
         * A trace may not have all the necessary environment
         * entries to do the tracer version comparison.
         * At least, the tracer name and major version number
         * are needed. Failing to extract these entries is not
         * an error.
         */
        BT_LOGI_STR("Cannot extract tracer information necessary to compare with buggy versions.");
        return 0;
    }

    /* Check if the trace may be affected by old tracer bugs. */
    if (is_tracer_affected_by_lttng_event_after_packet_bug(&current_tracer_info)) {
        BT_LOGI_STR("Trace may be affected by LTTng tracer packet timestamp bug. Fixing up.");
        ret = fix_index_lttng_event_after_packet_bug(ctf_fs->trace.get());
        if (ret) {
            BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(logCfg.selfComp, logCfg.selfCompClass,
                                                    "Failed to fix LTTng event-after-packet bug.");
            return ret;
        }
        ctf_fs->quirks.lttngEventAfterPacket = true;
    }

    if (is_tracer_affected_by_barectf_event_before_packet_bug(&current_tracer_info)) {
        BT_LOGI_STR("Trace may be affected by barectf tracer packet timestamp bug. Fixing up.");
        ret = fix_index_barectf_event_before_packet_bug(ctf_fs->trace.get());
        if (ret) {
            BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(
                logCfg.selfComp, logCfg.selfCompClass,
                "Failed to fix barectf event-before-packet bug.");
            return ret;
        }
        ctf_fs->quirks.barectfEventBeforePacket = true;
    }

    if (is_tracer_affected_by_lttng_crash_quirk(&current_tracer_info)) {
        ret = fix_index_lttng_crash_quirk(ctf_fs->trace.get());
        if (ret) {
            BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(logCfg.selfComp, logCfg.selfCompClass,
                                                    "Failed to fix lttng-crash timestamp quirks.");
            return ret;
        }
        ctf_fs->quirks.lttngCrash = true;
    }

    return 0;
}

static bool compare_ds_file_groups_by_first_path(const ctf_fs_ds_file_group::UP& ds_file_group_a,
                                                 const ctf_fs_ds_file_group::UP& ds_file_group_b)
{
    BT_ASSERT(!ds_file_group_a->ds_file_infos.empty());
    BT_ASSERT(!ds_file_group_b->ds_file_infos.empty());

    const ctf_fs_ds_file_info *first_ds_file_info_a = ds_file_group_a->ds_file_infos[0].get();
    const ctf_fs_ds_file_info *first_ds_file_info_b = ds_file_group_b->ds_file_infos[0].get();

    return first_ds_file_info_a->path < first_ds_file_info_b->path;
}

int ctf_fs_component_create_ctf_fs_trace(struct ctf_fs_component *ctf_fs,
                                         bt2::ConstArrayValue pathsValue,
                                         nonstd::optional<bt2::ConstStringValue> traceNameValue,
                                         bt_self_component *selfComp)
{
    const ctf::LogCfg& logCfg = ctf_fs->logCfg;
    BT_ASSERT(!pathsValue.isEmpty());

    std::vector<ctf_fs_trace::UP> traces;
    std::vector<std::string> paths;
    const char *trace_name = traceNameValue ? traceNameValue->value().c_str() : nullptr;

    /*
     * Create a sorted array of the paths, to make the execution of this
     * component deterministic.
     */
    for (bt2::ConstValue pathValue : pathsValue) {
        const char *input = pathValue.asString().value().c_str();
        paths.emplace_back(input);
    }

    std::sort(paths.begin(), paths.end());

    /* Create a separate ctf_fs_trace object for each path. */
    for (const std::string& path : paths) {
        int ret = ctf_fs_component_create_ctf_fs_trace_one_path(ctf_fs, path.c_str(), trace_name,
                                                                traces, selfComp);
        if (ret) {
            return ret;
        }
    }

    if (traces.size() > 1) {
        ctf_fs_trace *first_trace = traces[0].get();
        ctf_fs_trace::UP trace;

        /*
         * We have more than one trace, they must all share the same
         * UUID, verify that.
         */
        for (const ctf_fs_trace::UP& this_trace : traces) {
            if (!this_trace->irGenerator.ctfTraceCls()->uuid()) {
                BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(
                    logCfg.selfComp, logCfg.selfCompClass,
                    "Multiple traces given, but a trace does not have a UUID: path=%s",
                    this_trace->path.c_str());
                return -1;
            }

            const bt2_common::Uuid first_trace_uuid =
                *first_trace->irGenerator.ctfTraceCls()->uuid();
            const bt2_common::Uuid this_trace_uuid = *this_trace->irGenerator.ctfTraceCls()->uuid();

            if (first_trace_uuid != this_trace_uuid) {
                std::string firstTraceUUidStr = first_trace_uuid.str();
                std::string thisTraceUuidStr = this_trace_uuid.str();

                BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(
                    logCfg.selfComp, logCfg.selfCompClass,
                    "Multiple traces given, but UUIDs don't match: "
                    "first-trace-uuid=%s, first-trace-path=%s, "
                    "trace-uuid=%s, trace-path=%s",
                    firstTraceUUidStr.c_str(), first_trace->path.c_str(), thisTraceUuidStr.c_str(),
                    this_trace->path.c_str());
                return -1;
            }
        }

        int ret = merge_ctf_fs_traces(std::move(traces), ctf_fs->trace);
        if (ret) {
            BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(logCfg.selfComp, logCfg.selfCompClass,
                                                    "Failed to merge traces with the same UUID.");
            return ret;
        }
    } else {
        /* Just one trace, it may or may not have a UUID, both are fine. */
        ctf_fs->trace = std::move(traces[0]);
    }

    int ret = fix_packet_index_tracer_bugs(ctf_fs);
    if (ret) {
        BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(logCfg.selfComp, logCfg.selfCompClass,
                                                "Failed to fix packet index tracer bugs.");
        return ret;
    }

    /*
     * Sort data stream file groups by first data stream file info
     * path to get a deterministic order. This order influences the
     * order of the output ports. It also influences the order of
     * the automatic stream IDs if the trace's packet headers do not
     * contain a `stream_instance_id` field, in which case the data
     * stream file to stream ID association is always the same,
     * whatever the build and the system.
     *
     * Having a deterministic order here can help debugging and
     * testing.
     */
    std::sort(ctf_fs->trace->ds_file_groups.begin(), ctf_fs->trace->ds_file_groups.end(),
              compare_ds_file_groups_by_first_path);

    return 0;
}

static const char *get_stream_instance_unique_name(struct ctf_fs_ds_file_group *ds_file_group)
{
    /*
     * The first (earliest) stream file's path is used as the stream's unique
     * name.
     */
    BT_ASSERT(!ds_file_group->ds_file_infos.empty() > 0);
    ctf_fs_ds_file_info *ds_file_info = ds_file_group->ds_file_infos[0].get();
    return ds_file_info->path.c_str();
}

/* Create the IR stream objects for ctf_fs_trace. */

static int create_streams_for_trace(struct ctf_fs_trace *ctf_fs_trace)
{
    const ctf::LogCfg& logCfg = ctf_fs_trace->logCfg;

    for (ctf_fs_ds_file_group::UP& ds_file_group : ctf_fs_trace->ds_file_groups) {
        const char *name = get_stream_instance_unique_name(ds_file_group.get());

        if (ds_file_group->dataStreamCls.libCls()) {
            BT_ASSERT(ctf_fs_trace->trace);
            bt2::StreamClass streamCls = *ds_file_group->dataStreamCls.libCls();
            bt2::Trace trace = **ctf_fs_trace->trace;

            if (ds_file_group->stream_id == UINT64_C(-1)) {
                /* No stream ID: use 0 */
                ds_file_group->stream = streamCls.instantiate(trace, ctf_fs_trace->next_stream_id);
                ctf_fs_trace->next_stream_id++;
            } else {
                /* Specific stream ID */
                ds_file_group->stream = streamCls.instantiate(trace, ds_file_group->stream_id);
            }
        }

        int ret = bt_stream_set_name((*ds_file_group->stream)->libObjPtr(), name);
        if (ret) {
            BT_COMP_LOGE_APPEND_CAUSE(logCfg.selfComp,
                                      "Cannot set stream's name: "
                                      "addr=%p, stream-name=\"%s\"",
                                      (*ds_file_group->stream)->libObjPtr(), name);
            return -1;
        }
    }

    return 0;
}

static const bt_param_validation_value_descr inputs_elem_descr =
    bt_param_validation_value_descr::makeString();

static bt_param_validation_map_value_entry_descr fs_params_entries_descr[] = {
    {"inputs", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY,
     bt_param_validation_value_descr::makeArray(1, BT_PARAM_VALIDATION_INFINITE,
                                                inputs_elem_descr)},
    {"trace-name", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeString()},
    {"clock-class-offset-s", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeSignedInteger()},
    {"clock-class-offset-ns", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeSignedInteger()},
    {"force-clock-class-origin-unix-epoch", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeBool()},
    BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END};

bool read_src_fs_parameters(bt2::ConstMapValue params,
                            nonstd::optional<bt2::ConstArrayValue>& inputs,
                            nonstd::optional<bt2::ConstStringValue>& traceName,
                            struct ctf_fs_component *ctf_fs)
{
    gchar *error = NULL;
    const ctf::LogCfg& logCfg = ctf_fs->logCfg;

    bt_param_validation_status validate_value_status =
        bt_param_validation_validate(params.libObjPtr(), fs_params_entries_descr, &error);
    if (validate_value_status != BT_PARAM_VALIDATION_STATUS_OK) {
        BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(logCfg.selfComp, logCfg.selfCompClass, "%s", error);
        g_free(error);
        return false;
    }

    /* inputs parameter */
    inputs = params["inputs"]->asArray();

    /* clock-class-offset-s parameter */
    nonstd::optional<bt2::ConstValue> clockClassOffsetS = params["clock-class-offset-s"];
    if (clockClassOffsetS) {
        ctf_fs->clkClsCfg.offsetSec = clockClassOffsetS->asSignedInteger().value();
    }

    /* clock-class-offset-ns parameter */
    nonstd::optional<bt2::ConstValue> clockClassOffsetNs = params["clock-class-offset-ns"];
    if (clockClassOffsetNs) {
        ctf_fs->clkClsCfg.offsetNanoSec = clockClassOffsetNs->asSignedInteger().value();
    }

    /* force-clock-class-origin-unix-epoch parameter */
    nonstd::optional<bt2::ConstValue> forceClockClassOriginUnixEpoch =
        params["force-clock-class-origin-unix-epoch"];
    if (forceClockClassOriginUnixEpoch) {
        ctf_fs->clkClsCfg.forceOriginUnixEpoch = forceClockClassOriginUnixEpoch->asBool().value();
    }

    /* trace-name parameter */
    nonstd::optional<bt2::ConstValue> traceNameLocal = params["trace-name"];
    if (traceNameLocal) {
        traceName = traceNameLocal->asString();
    }

    return true;
}

static ctf_fs_component::UP ctf_fs_create(bt2::ConstMapValue params,
                                          bt_self_component_source *self_comp_src,
                                          const ctf::LogCfg& logCfg)
{
    bt_self_component *self_comp = bt_self_component_source_as_self_component(self_comp_src);
    ctf_fs_component::UP ctf_fs = bt2_common::makeUnique<ctf_fs_component>(logCfg);

    nonstd::optional<bt2::ConstArrayValue> inputsValue;
    nonstd::optional<bt2::ConstStringValue> traceNameValue;
    if (!read_src_fs_parameters(params, inputsValue, traceNameValue, ctf_fs.get())) {
        return nullptr;
    }

    if (ctf_fs_component_create_ctf_fs_trace(ctf_fs.get(), *inputsValue, traceNameValue,
                                             self_comp)) {
        return nullptr;
    }

    if (create_streams_for_trace(ctf_fs->trace.get())) {
        return nullptr;
    }

    if (create_ports_for_trace(ctf_fs.get(), ctf_fs->trace.get(), self_comp_src)) {
        return nullptr;
    }

    return ctf_fs;
}

BT_HIDDEN
bt_component_class_initialize_method_status
ctf_fs_init(bt_self_component_source *self_comp_src, bt_self_component_source_configuration *config,
            const bt_value *params, __attribute__((unused)) void *init_method_data)
{
    bt_self_component *selfComp = bt_self_component_source_as_self_component(self_comp_src);
    const bt_component *comp = bt_self_component_as_component(selfComp);
    bt_logging_level logLevel = bt_component_get_logging_level(comp);
    ctf::LogCfg logCfg(logLevel, selfComp);

    try {
        ctf_fs_component::UP ctf_fs =
            ctf_fs_create(bt2::ConstMapValue(params), self_comp_src, logCfg);
        if (!ctf_fs) {
            return BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
        }

        bt_self_component_set_data(selfComp, ctf_fs.release());
        return BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_OK;
    } catch (const std::bad_alloc&) {
        return BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR;
    } catch (const bt2_common::Error&) {
        BT_COMP_LOGE_APPEND_CAUSE(logCfg.selfComp, "Failed to initialize component");
        return BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
    }
}

BT_HIDDEN
bt_component_class_query_method_status ctf_fs_query(bt_self_component_class_source *comp_class_src,
                                                    bt_private_query_executor *priv_query_exec,
                                                    const char *object, const bt_value *params,
                                                    __attribute__((unused)) void *method_data,
                                                    const bt_value **result)
{
    const bt_query_executor *query_exec =
        bt_private_query_executor_as_query_executor_const(priv_query_exec);
    bt_logging_level log_level = bt_query_executor_get_logging_level(query_exec);
    bt_self_component_class *comp_class =
        bt_self_component_class_source_as_self_component_class(comp_class_src);
    ctf::LogCfg logCfg(log_level, comp_class);

    try {
        bt2::ConstMapValue paramsObj(params);
        nonstd::optional<bt2::Value::Shared> resultObj;

        if (strcmp(object, "metadata-info") == 0) {
            resultObj = metadata_info_query(paramsObj, logCfg);
        } else if (strcmp(object, "babeltrace.trace-infos") == 0) {
            resultObj = trace_infos_query(paramsObj, logCfg);
        } else if (!strcmp(object, "babeltrace.support-info")) {
            resultObj = support_info_query(paramsObj, logCfg);
        } else {
            BT_LOGE("Unknown query object `%s`", object);
            return BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_UNKNOWN_OBJECT;
        }

        *result = resultObj->release().libObjPtr();

        return BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_OK;
    } catch (const std::bad_alloc&) {
        return BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_MEMORY_ERROR;
    } catch (const bt2_common::Error&) {
        BT_COMP_CLASS_LOGE_APPEND_CAUSE(logCfg.selfCompClass, "Failed to exectute query: object=%s",
                                        object);
        return BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_ERROR;
    }
}
