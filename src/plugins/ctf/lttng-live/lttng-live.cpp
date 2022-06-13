/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2019 Francis Deslauriers <francis.deslauriers@efficios.com>
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2016 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * Babeltrace CTF LTTng-live Client Component
 */

#define BT_COMP_LOG_SELF_COMP logCfg.selfComp
#define BT_LOG_OUTPUT_LEVEL   logCfg.logLevel
#define BT_LOG_TAG            "PLUGIN/SRC.CTF.LTTNG-LIVE"
#include "logging/comp-logging.h"

#include <inttypes.h>
#include <stdbool.h>
#include <unistd.h>

#include <glib.h>

#include "common/assert.h"
#include <babeltrace2/babeltrace.h>
#include "compat/compiler.h"
#include <babeltrace2/types.h>
#include "cpp-common/comp-logging.hpp"
#include "cpp-common/exc.hpp"

#include "plugins/common/muxing/muxing.h"
#include "plugins/common/param-validation/param-validation.h"

#include "data-stream.hpp"
#include "metadata.hpp"
#include "lttng-live.hpp"
#include "../common/src/pkt-props.hpp"

#define MAX_QUERY_SIZE                     (256 * 1024)
#define URL_PARAM                          "url"
#define INPUTS_PARAM                       "inputs"
#define SESS_NOT_FOUND_ACTION_PARAM        "session-not-found-action"
#define SESS_NOT_FOUND_ACTION_CONTINUE_STR "continue"
#define SESS_NOT_FOUND_ACTION_FAIL_STR     "fail"
#define SESS_NOT_FOUND_ACTION_END_STR      "end"

#define print_dbg(fmt, ...) BT_COMP_LOGD(fmt, ##__VA_ARGS__)

static const char *lttng_live_iterator_status_string(enum lttng_live_iterator_status status)
{
    switch (status) {
    case LTTNG_LIVE_ITERATOR_STATUS_CONTINUE:
        return "LTTNG_LIVE_ITERATOR_STATUS_CONTINUE";
    case LTTNG_LIVE_ITERATOR_STATUS_AGAIN:
        return "LTTNG_LIVE_ITERATOR_STATUS_AGAIN";
    case LTTNG_LIVE_ITERATOR_STATUS_END:
        return "LTTNG_LIVE_ITERATOR_STATUS_END";
    case LTTNG_LIVE_ITERATOR_STATUS_OK:
        return "LTTNG_LIVE_ITERATOR_STATUS_OK";
    case LTTNG_LIVE_ITERATOR_STATUS_INVAL:
        return "LTTNG_LIVE_ITERATOR_STATUS_INVAL";
    case LTTNG_LIVE_ITERATOR_STATUS_ERROR:
        return "LTTNG_LIVE_ITERATOR_STATUS_ERROR";
    case LTTNG_LIVE_ITERATOR_STATUS_NOMEM:
        return "LTTNG_LIVE_ITERATOR_STATUS_NOMEM";
    case LTTNG_LIVE_ITERATOR_STATUS_UNSUPPORTED:
        return "LTTNG_LIVE_ITERATOR_STATUS_UNSUPPORTED";
    default:
        bt_common_abort();
    }
}

static const char *lttng_live_stream_state_string(enum lttng_live_stream_state state)
{
    switch (state) {
    case LTTNG_LIVE_STREAM_ACTIVE_NO_DATA:
        return "ACTIVE_NO_DATA";
    case LTTNG_LIVE_STREAM_QUIESCENT_NO_DATA:
        return "QUIESCENT_NO_DATA";
    case LTTNG_LIVE_STREAM_QUIESCENT:
        return "QUIESCENT";
    case LTTNG_LIVE_STREAM_ACTIVE_DATA:
        return "ACTIVE_DATA";
    case LTTNG_LIVE_STREAM_EOF:
        return "EOF";
    default:
        return "ERROR";
    }
}

void lttng_live_stream_iterator_set_state(struct lttng_live_stream_iterator *stream_iter,
                                          enum lttng_live_stream_state new_state)
{
    const ctf::LogCfg& logCfg = stream_iter->logCfg;

    BT_COMP_LOGD("Setting live stream iterator state: viewer-stream-id=%" PRIu64
                 ", old-state=%s, new-state=%s",
                 stream_iter->viewer_stream_id, lttng_live_stream_state_string(stream_iter->state),
                 lttng_live_stream_state_string(new_state));

    stream_iter->state = new_state;
}

#define LTTNG_LIVE_LOGD_STREAM_ITER(live_stream_iter)                                              \
    do {                                                                                           \
        BT_COMP_LOGD("Live stream iterator state=%s, "                                             \
                     "last-inact-ts-is-set=%d, last-inact-ts-value=%" PRId64 ", "                  \
                     "curr-inact-ts=%" PRId64,                                                     \
                     lttng_live_stream_state_string(live_stream_iter->state),                      \
                     live_stream_iter->last_inactivity_ts.is_set,                                  \
                     live_stream_iter->last_inactivity_ts.value,                                   \
                     live_stream_iter->current_inactivity_ts);                                     \
    } while (0);

BT_HIDDEN
bool lttng_live_graph_is_canceled(struct lttng_live_msg_iter *msg_iter)
{
    bool ret;

    if (!msg_iter) {
        ret = false;
        goto end;
    }

    ret = bt_self_message_iterator_is_interrupted(msg_iter->self_msg_iter);

end:
    return ret;
}

static struct lttng_live_trace *
lttng_live_session_borrow_trace_by_id(struct lttng_live_session *session, uint64_t trace_id)
{
    for (lttng_live_trace::UP& trace : session->traces) {
        if (trace->id == trace_id) {
            return trace.get();
        }
    }

    return nullptr;
}

static struct lttng_live_trace *lttng_live_create_trace(struct lttng_live_session *session,
                                                        uint64_t trace_id)
{
    const ctf::LogCfg& logCfg = session->logCfg;

    BT_COMP_LOGD("Creating live trace: "
                 "session-id=%" PRIu64 ", trace-id=%" PRIu64,
                 session->id, trace_id);

    lttng_live_trace::UP trace = bt2_common::makeUnique<lttng_live_trace>(logCfg);
    trace->session = session;
    trace->id = trace_id;
    trace->metadata_stream_state = LTTNG_LIVE_METADATA_STREAM_STATE_NEEDED;

    lttng_live_trace *ret = trace.get();
    session->traces.emplace_back(std::move(trace));

    return ret;
}

BT_HIDDEN
struct lttng_live_trace *
lttng_live_session_borrow_or_create_trace_by_id(struct lttng_live_session *session,
                                                uint64_t trace_id)
{
    struct lttng_live_trace *trace;

    trace = lttng_live_session_borrow_trace_by_id(session, trace_id);
    if (trace) {
        goto end;
    }

    /* The session is the owner of the newly created trace. */
    trace = lttng_live_create_trace(session, trace_id);

end:
    return trace;
}

BT_HIDDEN
int lttng_live_add_session(struct lttng_live_msg_iter *lttng_live_msg_iter, uint64_t session_id,
                           const char *hostname, const char *session_name)
{
    const ctf::LogCfg& logCfg = lttng_live_msg_iter->logCfg;

    BT_COMP_LOGD("Adding live session: "
                 "session-id=%" PRIu64 ", hostname=\"%s\" session-name=\"%s\"",
                 session_id, hostname, session_name);

    lttng_live_session::UP session = bt2_common::makeUnique<lttng_live_session>(logCfg);
    session->self_comp = lttng_live_msg_iter->self_comp;
    session->id = session_id;
    session->lttng_live_msg_iter = lttng_live_msg_iter;
    session->new_streams_needed = true;
    session->hostname = hostname;
    session->session_name = session_name;

    lttng_live_msg_iter->sessions.emplace_back(std::move(session));
    return 0;
}

lttng_live_session::~lttng_live_session()
{
    BT_COMP_LOGD("Destroying live session: "
                 "session-id=%" PRIu64 ", session-name=\"%s\"",
                 this->id, this->session_name.c_str());
    if (this->id != -1ULL) {
        if (lttng_live_session_detach(this)) {
            if (!lttng_live_graph_is_canceled(this->lttng_live_msg_iter)) {
                /* Old relayd cannot detach sessions. */
                BT_COMP_LOGD("Unable to detach lttng live session %" PRIu64, this->id);
            }
        }
        this->id = -1ULL;
    }
}

lttng_live_msg_iter::~lttng_live_msg_iter()
{
    BT_ASSERT(this->lttng_live_comp);
    BT_ASSERT(this->lttng_live_comp->has_msg_iter);

    /* All stream iterators must be destroyed at this point. */
    BT_ASSERT(this->active_stream_iter == 0);
    this->lttng_live_comp->has_msg_iter = false;
}

BT_HIDDEN
void lttng_live_msg_iter_finalize(bt_self_message_iterator *self_msg_iter)
{
    lttng_live_msg_iter::UP {
        (lttng_live_msg_iter *) bt_self_message_iterator_get_data(self_msg_iter)};
}

static enum lttng_live_iterator_status
lttng_live_iterator_next_check_stream_state(struct lttng_live_stream_iterator *lttng_live_stream)
{
    const ctf::LogCfg& logCfg = lttng_live_stream->logCfg;

    switch (lttng_live_stream->state) {
    case LTTNG_LIVE_STREAM_QUIESCENT:
    case LTTNG_LIVE_STREAM_ACTIVE_DATA:
        break;
    case LTTNG_LIVE_STREAM_ACTIVE_NO_DATA:
        /* Invalid state. */
        BT_COMP_LOGF("Unexpected stream state \"ACTIVE_NO_DATA\"");
        bt_common_abort();
    case LTTNG_LIVE_STREAM_QUIESCENT_NO_DATA:
        /* Invalid state. */
        BT_COMP_LOGF("Unexpected stream state \"QUIESCENT_NO_DATA\"");
        bt_common_abort();
    case LTTNG_LIVE_STREAM_EOF:
        break;
    }
    return LTTNG_LIVE_ITERATOR_STATUS_OK;
}

/*
 * For active no data stream, fetch next index. As a result of that it can
 * become either:
 * - quiescent: won't have events for a bit,
 * - have data: need to get that data and produce the event,
 * - have no data on this stream at this point: need to retry (AGAIN) or return
 *   EOF.
 */
static enum lttng_live_iterator_status lttng_live_iterator_next_handle_one_no_data_stream(
    struct lttng_live_msg_iter *lttng_live_msg_iter,
    struct lttng_live_stream_iterator *lttng_live_stream)
{
    const ctf::LogCfg& logCfg = lttng_live_msg_iter->logCfg;
    enum lttng_live_iterator_status ret = LTTNG_LIVE_ITERATOR_STATUS_OK;
    enum lttng_live_stream_state orig_state = lttng_live_stream->state;
    struct packet_index index;

    if (lttng_live_stream->trace->metadata_stream_state ==
        LTTNG_LIVE_METADATA_STREAM_STATE_NEEDED) {
        BT_COMP_LOGD(
            "Need to get an update for the metadata stream before proceeding further with this stream: "
            "stream-name=\"%s\"",
            lttng_live_stream->name.c_str());
        ret = LTTNG_LIVE_ITERATOR_STATUS_CONTINUE;
        goto end;
    }

    if (lttng_live_stream->trace->session->new_streams_needed) {
        BT_COMP_LOGD(
            "Need to get an update of all streams before proceeding further with this stream: "
            "stream-name=\"%s\"",
            lttng_live_stream->name.c_str());
        ret = LTTNG_LIVE_ITERATOR_STATUS_CONTINUE;
        goto end;
    }

    if (lttng_live_stream->state != LTTNG_LIVE_STREAM_ACTIVE_NO_DATA &&
        lttng_live_stream->state != LTTNG_LIVE_STREAM_QUIESCENT_NO_DATA) {
        goto end;
    }
    ret = lttng_live_get_next_index(lttng_live_msg_iter, lttng_live_stream, &index);
    if (ret != LTTNG_LIVE_ITERATOR_STATUS_OK) {
        goto end;
    }

    BT_ASSERT_DBG(lttng_live_stream->state != LTTNG_LIVE_STREAM_EOF);

    if (lttng_live_stream->state == LTTNG_LIVE_STREAM_QUIESCENT) {
        uint64_t last_inact_ts = lttng_live_stream->last_inactivity_ts.value,
                 curr_inact_ts = lttng_live_stream->current_inactivity_ts;

        if (orig_state == LTTNG_LIVE_STREAM_QUIESCENT_NO_DATA && last_inact_ts == curr_inact_ts) {
            /*
             * Because the stream is in the QUIESCENT_NO_DATA
             * state, we can assert that the last_inactivity_ts was
             * set and can be safely used in the `if` above.
             */
            BT_ASSERT(lttng_live_stream->last_inactivity_ts.is_set);

            ret = LTTNG_LIVE_ITERATOR_STATUS_AGAIN;
            LTTNG_LIVE_LOGD_STREAM_ITER(lttng_live_stream);
        } else {
            ret = LTTNG_LIVE_ITERATOR_STATUS_CONTINUE;
        }
        goto end;
    }

    lttng_live_stream->base_offset = index.offset;
    lttng_live_stream->offset = index.offset;
    lttng_live_stream->len = index.packet_size / CHAR_BIT;

    BT_COMP_LOGD("Setting live stream reading info: stream-name=\"%s\", "
                 "viewer-stream-id=%" PRIu64 ", stream-base-offset=%" PRIu64
                 ", stream-offset=%" PRIu64 ", stream-len=%" PRIu64,
                 lttng_live_stream->name.c_str(), lttng_live_stream->viewer_stream_id,
                 lttng_live_stream->base_offset, lttng_live_stream->offset, lttng_live_stream->len);

end:
    if (ret == LTTNG_LIVE_ITERATOR_STATUS_OK) {
        ret = lttng_live_iterator_next_check_stream_state(lttng_live_stream);
    }
    return ret;
}

/*
 * Creation of the message requires the ctf trace class to be created
 * beforehand, but the live protocol gives us all streams (including metadata)
 * at once. So we split it in three steps: getting streams, getting metadata
 * (which creates the ctf trace class), and then creating the per-stream
 * messages.
 */
static enum lttng_live_iterator_status
lttng_live_get_session(struct lttng_live_msg_iter *lttng_live_msg_iter,
                       struct lttng_live_session *session)
{
    const ctf::LogCfg& logCfg = lttng_live_msg_iter->logCfg;

    if (!session->attached) {
        BT_COMP_LOGD("Attach to session: session-id=%" PRIu64, session->id);
        enum lttng_live_viewer_status attach_status =
            lttng_live_session_attach(session, lttng_live_msg_iter->self_msg_iter);
        if (attach_status != LTTNG_LIVE_VIEWER_STATUS_OK) {
            if (lttng_live_graph_is_canceled(lttng_live_msg_iter)) {
                /*
                 * Clear any causes appended in
                 * `lttng_live_attach_session()` as we want to
                 * return gracefully since the graph was
                 * cancelled.
                 */
                bt_current_thread_clear_error();
                return LTTNG_LIVE_ITERATOR_STATUS_AGAIN;
            } else {
                BT_COMP_LOGE_APPEND_CAUSE(logCfg.selfComp, "Error attaching to LTTng live session");
                return LTTNG_LIVE_ITERATOR_STATUS_ERROR;
            }
        }
    }

    BT_COMP_LOGD("Updating all data streams: "
                 "session-id=%" PRIu64 ", session-name=\"%s\"",
                 session->id, session->session_name.c_str());

    lttng_live_iterator_status status =
        lttng_live_session_get_new_streams(session, lttng_live_msg_iter->self_msg_iter);
    switch (status) {
    case LTTNG_LIVE_ITERATOR_STATUS_OK:
        break;
    case LTTNG_LIVE_ITERATOR_STATUS_END:
        /*
         * We received a `_END` from the `_get_new_streams()` function,
         * which means no more data will ever be received from the data
         * streams of this session. But it's possible that the metadata
         * is incomplete.
         * The live protocol guarantees that we receive all the
         * metadata needed before we receive data streams needing it.
         * But it's possible to receive metadata NOT needed by
         * data streams after the session was closed. For example, this
         * could happen if a new event is registered and the session is
         * stopped before any tracepoint for that event is actually
         * fired.
         */
        BT_COMP_LOGD(
            "Updating streams returned _END status. Override status to _OK in order fetch any remaining metadata:"
            "session-id=%" PRIu64 ", session-name=\"%s\"",
            session->id, session->session_name.c_str());
        status = LTTNG_LIVE_ITERATOR_STATUS_OK;
        break;
    default:
        return status;
    }

    BT_COMP_LOGD("Updating metadata stream for session: "
                 "session-id=%" PRIu64 ", session-name=\"%s\"",
                 session->id, session->session_name.c_str());

    uint64_t trace_idx = 0;
    while (trace_idx < session->traces.size()) {
        lttng_live_trace *trace = session->traces[trace_idx].get();

        status = lttng_live_metadata_update(trace);
        switch (status) {
        case LTTNG_LIVE_ITERATOR_STATUS_END:
        case LTTNG_LIVE_ITERATOR_STATUS_OK:
            trace_idx++;
            break;
        case LTTNG_LIVE_ITERATOR_STATUS_CONTINUE:
        case LTTNG_LIVE_ITERATOR_STATUS_AGAIN:
            return status;
        default:
            BT_COMP_LOGE_APPEND_CAUSE(logCfg.selfComp,
                                      "Error updating trace metadata: "
                                      "stream-iter-status=%s, trace-id=%" PRIu64,
                                      lttng_live_iterator_status_string(status), trace->id);
            return status;
        }
    }

    return LTTNG_LIVE_ITERATOR_STATUS_OK;
    /*
     * Now that we have the metadata we can initialize the downstream
     * iterator.
     */
    return lttng_live_lazy_msg_init(session, lttng_live_msg_iter->self_msg_iter);
}

static void
lttng_live_force_new_streams_and_metadata(struct lttng_live_msg_iter *lttng_live_msg_iter)
{
    const ctf::LogCfg& logCfg = lttng_live_msg_iter->logCfg;

    for (lttng_live_session::UP& session : lttng_live_msg_iter->sessions) {
        BT_COMP_LOGD("Force marking session as needing new streams: "
                     "session-id=%" PRIu64,
                     session->id);
        session->new_streams_needed = true;
        for (lttng_live_trace::UP& trace : session->traces) {
            BT_COMP_LOGD("Force marking trace metadata state as needing an update: "
                         "session-id=%" PRIu64 ", trace-id=%" PRIu64,
                         session->id, trace->id);

            BT_ASSERT(trace->metadata_stream_state != LTTNG_LIVE_METADATA_STREAM_STATE_CLOSED);

            trace->metadata_stream_state = LTTNG_LIVE_METADATA_STREAM_STATE_NEEDED;
        }
    }
}

static enum lttng_live_iterator_status
lttng_live_iterator_handle_new_streams_and_metadata(struct lttng_live_msg_iter *lttng_live_msg_iter)
{
    enum lttng_live_viewer_status viewer_status;
    const ctf::LogCfg& logCfg = lttng_live_msg_iter->logCfg;
    uint64_t nr_sessions_opened = 0;
    enum session_not_found_action sess_not_found_act =
        lttng_live_msg_iter->lttng_live_comp->params.sess_not_found_act;

    BT_COMP_LOGD("Update data and metadata of all sessions: "
                 "live-msg-iter-addr=%p",
                 lttng_live_msg_iter);
    /*
     * In a remotely distant future, we could add a "new
     * session" flag to the protocol, which would tell us that we
     * need to query for new sessions even though we have sessions
     * currently ongoing.
     */
    if (lttng_live_msg_iter->sessions.empty()) {
        if (sess_not_found_act != SESSION_NOT_FOUND_ACTION_CONTINUE) {
            BT_COMP_LOGD(
                "No session found. Exiting in accordance with the `session-not-found-action` parameter");
            return LTTNG_LIVE_ITERATOR_STATUS_END;
        } else {
            BT_COMP_LOGD(
                "No session found. Try creating a new one in accordance with the `session-not-found-action` parameter");
            /*
             * Retry to create a viewer session for the requested
             * session name.
             */
            viewer_status = lttng_live_create_viewer_session(lttng_live_msg_iter);
            if (viewer_status != LTTNG_LIVE_VIEWER_STATUS_OK) {
                if (viewer_status == LTTNG_LIVE_VIEWER_STATUS_ERROR) {
                    BT_COMP_LOGE_APPEND_CAUSE(logCfg.selfComp,
                                              "Error creating LTTng live viewer session");
                    return LTTNG_LIVE_ITERATOR_STATUS_ERROR;
                } else if (viewer_status == LTTNG_LIVE_VIEWER_STATUS_INTERRUPTED) {
                    return LTTNG_LIVE_ITERATOR_STATUS_AGAIN;
                } else {
                    bt_common_abort();
                }
            }
        }
    }

    for (lttng_live_session::UP& session : lttng_live_msg_iter->sessions) {
        lttng_live_iterator_status status =
            lttng_live_get_session(lttng_live_msg_iter, session.get());
        switch (status) {
        case LTTNG_LIVE_ITERATOR_STATUS_OK:
        case LTTNG_LIVE_ITERATOR_STATUS_END:
            /*
             * A session returned `_END`. Other sessions may still
             * be active so we override the status and continue
             * looping if needed.
             */
            break;
        default:
            return status;
        }

        if (!session->closed) {
            nr_sessions_opened++;
        }
    }

    if (sess_not_found_act != SESSION_NOT_FOUND_ACTION_CONTINUE && nr_sessions_opened == 0) {
        return LTTNG_LIVE_ITERATOR_STATUS_END;
    }

    return LTTNG_LIVE_ITERATOR_STATUS_OK;
}

static enum lttng_live_iterator_status
emit_inactivity_message(struct lttng_live_msg_iter *lttng_live_msg_iter,
                        struct lttng_live_stream_iterator *stream_iter,
                        nonstd::optional<bt2::ConstMessage::Shared>& message, uint64_t timestamp)
{
    enum lttng_live_iterator_status ret = LTTNG_LIVE_ITERATOR_STATUS_OK;
    const ctf::LogCfg& logCfg = lttng_live_msg_iter->logCfg;
    bt_message *msg = NULL;

    BT_ASSERT(stream_iter->trace->clockClass);

    BT_COMP_LOGD("Emitting inactivity message for stream: ctf-stream-id=%" PRIu64
                 ", viewer-stream-id=%" PRIu64 ", timestamp=%" PRIu64,
                 stream_iter->ctf_stream_class_id.value, stream_iter->viewer_stream_id, timestamp);

    msg = bt_message_message_iterator_inactivity_create(
        lttng_live_msg_iter->self_msg_iter, stream_iter->trace->clockClass->libObjPtr(), timestamp);
    if (!msg) {
        BT_COMP_LOGE_APPEND_CAUSE(logCfg.selfComp,
                                  "Error emitting message iterator inactivity message");
        goto error;
    }

    message = bt2::ConstMessage::Shared::createWithoutRef(msg);
end:
    return ret;

error:
    ret = LTTNG_LIVE_ITERATOR_STATUS_ERROR;
    bt_message_put_ref(msg);
    goto end;
}

static enum lttng_live_iterator_status lttng_live_iterator_next_handle_one_quiescent_stream(
    struct lttng_live_msg_iter *lttng_live_msg_iter,
    struct lttng_live_stream_iterator *lttng_live_stream,
    nonstd::optional<bt2::ConstMessage::Shared>& message)
{
    enum lttng_live_iterator_status ret = LTTNG_LIVE_ITERATOR_STATUS_OK;

    if (lttng_live_stream->state != LTTNG_LIVE_STREAM_QUIESCENT) {
        return LTTNG_LIVE_ITERATOR_STATUS_OK;
    }

    /*
     * Check if we already sent an inactivty message downstream for this
     * `current_inactivity_ts` value.
     */
    if (lttng_live_stream->last_inactivity_ts.is_set &&
        lttng_live_stream->current_inactivity_ts == lttng_live_stream->last_inactivity_ts.value) {
        lttng_live_stream_iterator_set_state(lttng_live_stream,
                                             LTTNG_LIVE_STREAM_QUIESCENT_NO_DATA);

        ret = LTTNG_LIVE_ITERATOR_STATUS_CONTINUE;
        goto end;
    }

    ret = emit_inactivity_message(lttng_live_msg_iter, lttng_live_stream, message,
                                  lttng_live_stream->current_inactivity_ts);

    lttng_live_stream->last_inactivity_ts.value = lttng_live_stream->current_inactivity_ts;
    lttng_live_stream->last_inactivity_ts.is_set = true;
end:
    return ret;
}

static int live_get_msg_ts_ns(struct lttng_live_stream_iterator *stream_iter,
                              struct lttng_live_msg_iter *lttng_live_msg_iter,
                              bt2::ConstMessage msg, int64_t last_msg_ts_ns, int64_t *ts_ns)
{
    nonstd::optional<bt2::ConstClockSnapshot> clockSnapshot;
    const ctf::LogCfg& logCfg = lttng_live_msg_iter->logCfg;

    BT_ASSERT_DBG(ts_ns);

    BT_COMP_LOGD("Getting message's timestamp: iter-data-addr=%p, msg-addr=%p, "
                 "last-msg-ts=%" PRId64,
                 lttng_live_msg_iter, msg.libObjPtr(), last_msg_ts_ns);

    switch (msg.type()) {
    case bt2::MessageType::EVENT:
        clockSnapshot = msg.asEvent().defaultClockSnapshot();
        break;

    case bt2::MessageType::PACKET_BEGINNING:

        clockSnapshot = msg.asPacketBeginning().defaultClockSnapshot();
        break;

    case bt2::MessageType::PACKET_END:
        clockSnapshot = msg.asPacketEnd().defaultClockSnapshot();
        break;

    case bt2::MessageType::DISCARDED_EVENTS:
        clockSnapshot = msg.asDiscardedEvents().beginningDefaultClockSnapshot();
        break;

    case bt2::MessageType::DISCARDED_PACKETS:
        clockSnapshot = msg.asDiscardedPackets().beginningDefaultClockSnapshot();
        break;

    case bt2::MessageType::MESSAGE_ITERATOR_INACTIVITY:
        clockSnapshot = msg.asMessageIteratorInactivity().clockSnapshot();
        break;

    default:
        /* All the other messages have a higher priority */
        BT_COMP_LOGD_STR("Message has no timestamp: using the last message timestamp.");
        *ts_ns = last_msg_ts_ns;
        return 0;
    }

    *ts_ns = clockSnapshot->nsFromOrigin();

    BT_COMP_LOGD("Found message's timestamp: "
                 "iter-data-addr=%p, msg-addr=%p, "
                 "last-msg-ts=%" PRId64 ", ts=%" PRId64,
                 lttng_live_msg_iter, msg.libObjPtr(), last_msg_ts_ns, *ts_ns);

    return 0;
}

static lttng_live_iterator_status
lttng_live_stream_iterator_create_msg_iter(lttng_live_stream_iterator *liveStreamIter)
{
    BT_ASSERT(!liveStreamIter->msgIter);
    BT_ASSERT(!liveStreamIter->stream);
    uint64_t recvLen;
    lttng_live_trace *trace = liveStreamIter->trace;
    lttng_live_msg_iter *liveMsgIter = trace->session->lttng_live_msg_iter;

    lttng_live_get_stream_bytes_status status =
        lttng_live_get_stream_bytes(liveMsgIter, liveStreamIter, liveStreamIter->buf.data(),
                                    liveStreamIter->offset, liveStreamIter->buf.size(), &recvLen);
    switch (status) {
    case LTTNG_LIVE_GET_STREAM_BYTES_STATUS_OK:
        break;

    case LTTNG_LIVE_GET_STREAM_BYTES_STATUS_AGAIN:
        return LTTNG_LIVE_ITERATOR_STATUS_AGAIN;

    case LTTNG_LIVE_GET_STREAM_BYTES_STATUS_EOF:
        return LTTNG_LIVE_ITERATOR_STATUS_END;

    case LTTNG_LIVE_GET_STREAM_BYTES_STATUS_ERROR:
        return LTTNG_LIVE_ITERATOR_STATUS_ERROR;
    }

    ctf::src::Medium::UP tempMedium =
        bt2_common::makeUnique<ctf::src::live::CtfLiveMedium>(*liveStreamIter);
    ctf::src::TraceCls *ctfTc = liveStreamIter->trace->metadata->irGenerator.ctfTraceCls();
    BT_ASSERT(ctfTc);
    ctf::src::PktProps pktProps =
        ctf::src::readPktProps(*ctfTc, std::move(tempMedium), bt2_common::DataLen::fromBytes(0));

    nonstd::optional<bt2::TraceClass> tc = trace->metadata->irGenerator.irTraceCls();
    BT_ASSERT(tc);
    BT_ASSERT(liveStreamIter->ctf_stream_class_id.is_set);
    BT_ASSERT(trace->trace);

    const ctf::LogCfg& logCfg = liveStreamIter->logCfg;

    nonstd::optional<bt2::StreamClass> sc =
        tc->streamClassById(liveStreamIter->ctf_stream_class_id.value);
    if (!sc) {
        BT_COMP_LOGE_APPEND_CAUSE_AND_THROW(bt2::Error, logCfg.selfComp,
                                            "No stream class with id %" PRId64,
                                            liveStreamIter->ctf_stream_class_id.value);
    }

    // FIXME: in the original, there is a fall back if the data stream id is not available.
    bt_stream *streamPtr = bt_stream_create_with_id(sc->libObjPtr(), (*trace->trace)->libObjPtr(),
                                                    *pktProps.dataStreamId);
    BT_ASSERT(streamPtr);
    liveStreamIter->stream = bt2::Stream::Shared::createWithoutRef(streamPtr);

    ctf::src::Medium::UP medium =
        bt2_common::makeUnique<ctf::src::live::CtfLiveMedium>(*liveStreamIter);
    liveStreamIter->msgIter.emplace(liveMsgIter->self_msg_iter, *ctfTc, **liveStreamIter->stream,
                                    std::move(medium), ctf::src::Quirks {}, logCfg);
    return LTTNG_LIVE_ITERATOR_STATUS_OK;
}

static enum lttng_live_iterator_status lttng_live_iterator_next_handle_one_active_data_stream(
    struct lttng_live_msg_iter *lttng_live_msg_iter,
    struct lttng_live_stream_iterator *lttng_live_stream,
    nonstd::optional<bt2::ConstMessage::Shared>& message)
{
    const ctf::LogCfg& logCfg = lttng_live_msg_iter->logCfg;

    for (lttng_live_session::UP& session : lttng_live_msg_iter->sessions) {
        if (session->new_streams_needed) {
            BT_COMP_LOGD("Need an update for streams: "
                         "session-id=%" PRIu64,
                         session->id);
            return LTTNG_LIVE_ITERATOR_STATUS_CONTINUE;
        }
        for (lttng_live_trace::UP& trace : session->traces) {
            if (trace->metadata_stream_state == LTTNG_LIVE_METADATA_STREAM_STATE_NEEDED) {
                BT_COMP_LOGD("Need an update for metadata stream: "
                             "session-id=%" PRIu64 ", trace-id=%" PRIu64,
                             session->id, trace->id);
                return LTTNG_LIVE_ITERATOR_STATUS_CONTINUE;
            }
        }
    }

    if (lttng_live_stream->state != LTTNG_LIVE_STREAM_ACTIVE_DATA) {
        BT_COMP_LOGE_APPEND_CAUSE(logCfg.selfComp,
                                  "Invalid state of live stream iterator"
                                  "stream-iter-status=%s",
                                  lttng_live_stream_state_string(lttng_live_stream->state));
        return LTTNG_LIVE_ITERATOR_STATUS_ERROR;
    }

    if (!lttng_live_stream->msgIter) {
        /* The first time we're called for this stream, the MsgIter is not instantiated.  */
        enum lttng_live_iterator_status ret =
            lttng_live_stream_iterator_create_msg_iter(lttng_live_stream);
        if (ret != LTTNG_LIVE_ITERATOR_STATUS_OK) {
            return ret;
        }
    }

    enum lttng_live_iterator_status ret;
    try {
        message = lttng_live_stream->msgIter->next();
        ret = LTTNG_LIVE_ITERATOR_STATUS_OK;
    } catch (const bt2::Error&) {
        ret = LTTNG_LIVE_ITERATOR_STATUS_ERROR;
        BT_COMP_LOGE_APPEND_CAUSE(logCfg.selfComp,
                                  "CTF message iterator failed to get next message: "
                                  "msg-iter=%p",
                                  &*lttng_live_stream->msgIter);
    } catch (const ctf::src::MsgIterEnded&) {
        ret = LTTNG_LIVE_ITERATOR_STATUS_END;
    }

    return ret;
}

static enum lttng_live_iterator_status
lttng_live_iterator_close_stream(struct lttng_live_msg_iter *lttng_live_msg_iter,
                                 struct lttng_live_stream_iterator *stream_iter,
                                 nonstd::optional<bt2::ConstMessage::Shared>& curr_msg)
{
    const ctf::LogCfg& logCfg = lttng_live_msg_iter->logCfg;

    BT_COMP_LOGD("Closing live stream iterator: stream-name=\"%s\", "
                 "viewer-stream-id=%" PRIu64,
                 stream_iter->name.c_str(), stream_iter->viewer_stream_id);

    /*
     * The viewer has hung up on us so we are closing the stream. The
     * `ctf_msg_iter` should simply realize that it needs to close the
     * stream properly by emitting the necessary stream end message.
     */
    try {
        curr_msg = stream_iter->msgIter->next();
    } catch (const bt2::Error&) {
        BT_COMP_LOGE_APPEND_CAUSE(logCfg.selfComp,
                                  "Error getting the next message from CTF message iterator");
        return LTTNG_LIVE_ITERATOR_STATUS_ERROR;
    } catch (const ctf::src::MsgIterEnded&) {
        BT_COMP_LOGI("Reached the end of the live stream iterator.");
        return LTTNG_LIVE_ITERATOR_STATUS_END;
    }

    return LTTNG_LIVE_ITERATOR_STATUS_OK;
}

/*
 * helper function:
 *            handle_no_data_streams()
 *              retry:
 *                - for each ACTIVE_NO_DATA stream:
 *                  - query relayd for stream data, or quiescence info.
 *                    - if need metadata, get metadata, goto retry.
 *                    - if new stream, get new stream as ACTIVE_NO_DATA, goto retry
 *                  - if quiescent, move to QUIESCENT streams
 *                  - if fetched data, move to ACTIVE_DATA streams
 *                (at this point each stream either has data, or is quiescent)
 *
 *
 * iterator_next:
 *            handle_new_streams_and_metadata()
 *                  - query relayd for known streams, add them as ACTIVE_NO_DATA
 *                  - query relayd for metadata
 *
 *            call handle_active_no_data_streams()
 *
 *            handle_quiescent_streams()
 *                - if at least one stream is ACTIVE_DATA:
 *                  - peek stream event with lowest timestamp -> next_ts
 *                  - for each quiescent stream
 *                    - if next_ts >= quiescent end
 *                      - set state to ACTIVE_NO_DATA
 *                - else
 *                  - for each quiescent stream
 *                      - set state to ACTIVE_NO_DATA
 *
 *            call handle_active_no_data_streams()
 *
 *            handle_active_data_streams()
 *                - if at least one stream is ACTIVE_DATA:
 *                    - get stream event with lowest timestamp from heap
 *                    - make that stream event the current message.
 *                    - move this stream heap position to its next event
 *                      - if we need to fetch data from relayd, move
 *                        stream to ACTIVE_NO_DATA.
 *                    - return OK
 *                - return AGAIN
 *
 * end criterion: ctrl-c on client. If relayd exits or the session
 * closes on the relay daemon side, we keep on waiting for streams.
 * Eventually handle --end timestamp (also an end criterion).
 *
 * When disconnected from relayd: try to re-connect endlessly.
 */
static enum lttng_live_iterator_status
lttng_live_iterator_next_msg_on_stream(struct lttng_live_msg_iter *lttng_live_msg_iter,
                                       struct lttng_live_stream_iterator *stream_iter,
                                       nonstd::optional<bt2::ConstMessage::Shared>& curr_msg)
{
    const ctf::LogCfg& logCfg = lttng_live_msg_iter->logCfg;
    enum lttng_live_iterator_status live_status;

    BT_COMP_LOGD("Advancing live stream iterator until next message if possible: "
                 "stream-name=\"%s\", viewer-stream-id=%" PRIu64,
                 stream_iter->name.c_str(), stream_iter->viewer_stream_id);

    if (stream_iter->has_stream_hung_up) {
        /*
         * The stream has hung up and the stream was properly closed
         * during the last call to the current function. Return _END
         * status now so that this stream iterator is removed for the
         * stream iterator list.
         */
        live_status = LTTNG_LIVE_ITERATOR_STATUS_END;
        goto end;
    }

retry:
    LTTNG_LIVE_LOGD_STREAM_ITER(stream_iter);

    /*
     * Make sure we have the most recent metadata and possibly some new
     * streams.
     */
    live_status = lttng_live_iterator_handle_new_streams_and_metadata(lttng_live_msg_iter);
    if (live_status != LTTNG_LIVE_ITERATOR_STATUS_OK) {
        goto end;
    }

    live_status =
        lttng_live_iterator_next_handle_one_no_data_stream(lttng_live_msg_iter, stream_iter);
    if (live_status != LTTNG_LIVE_ITERATOR_STATUS_OK) {
        if (live_status == LTTNG_LIVE_ITERATOR_STATUS_END) {
            /*
             * We overwrite `live_status` since `curr_msg` is
             * likely set to a valid message in this function.
             */
            live_status =
                lttng_live_iterator_close_stream(lttng_live_msg_iter, stream_iter, curr_msg);
        }
        goto end;
    }

    live_status = lttng_live_iterator_next_handle_one_quiescent_stream(lttng_live_msg_iter,
                                                                       stream_iter, curr_msg);
    if (live_status != LTTNG_LIVE_ITERATOR_STATUS_OK) {
        BT_ASSERT(!curr_msg);
        goto end;
    }

    if (curr_msg) {
        goto end;
    }

    live_status = lttng_live_iterator_next_handle_one_active_data_stream(lttng_live_msg_iter,
                                                                         stream_iter, curr_msg);
    if (live_status != LTTNG_LIVE_ITERATOR_STATUS_OK) {
        BT_ASSERT(!curr_msg);
    }

end:
    if (live_status == LTTNG_LIVE_ITERATOR_STATUS_CONTINUE) {
        BT_COMP_LOGD("Ask the relay daemon for an updated view of the data and metadata streams");
        goto retry;
    }

    BT_COMP_LOGD("Returning from advancing live stream iterator: status=%s, "
                 "stream-name=\"%s\", viewer-stream-id=%" PRIu64,
                 lttng_live_iterator_status_string(live_status), stream_iter->name.c_str(),
                 stream_iter->viewer_stream_id);

    return live_status;
}

static bool is_discarded_packet_or_event_message(bt2::ConstMessage msg)
{
    const bt2::MessageType msgType = msg.type();

    return msgType == bt2::MessageType::DISCARDED_EVENTS ||
           msgType == bt2::MessageType::DISCARDED_PACKETS;
}

static enum lttng_live_iterator_status adjust_discarded_packets_message(
    bt_self_message_iterator *iter, bt2::Stream stream, bt2::ConstDiscardedPacketsMessage msgIn,
    nonstd::optional<bt2::Message::Shared>& msgOut, uint64_t new_begin_ts)
{
    uint64_t end_ts = msgIn.endDefaultClockSnapshot().value();
    nonstd::optional<uint64_t> count = msgIn.count();
    BT_ASSERT_DBG(count);

    bt_message *msg = bt_message_discarded_packets_create_with_default_clock_snapshots(
        iter, stream.libObjPtr(), new_begin_ts, end_ts);
    if (!msg) {
        return LTTNG_LIVE_ITERATOR_STATUS_NOMEM;
    }

    bt_message_discarded_packets_set_count(msg, *count);
    msgOut = bt2::Message::Shared::createWithoutRef(msg);

    return LTTNG_LIVE_ITERATOR_STATUS_OK;
}

static enum lttng_live_iterator_status
adjust_discarded_events_message(bt_self_message_iterator *iter, const bt2::Stream stream,
                                bt2::ConstDiscardedEventsMessage msgIn,
                                nonstd::optional<bt2::Message::Shared>& msgOut,
                                uint64_t new_begin_ts)
{
    uint64_t end_ts = msgIn.endDefaultClockSnapshot().value();
    nonstd::optional<uint64_t> count = msgIn.count();
    BT_ASSERT_DBG(count);

    bt_message *msg = bt_message_discarded_events_create_with_default_clock_snapshots(
        iter, stream.libObjPtr(), new_begin_ts, end_ts);
    if (!msg) {
        return LTTNG_LIVE_ITERATOR_STATUS_NOMEM;
    }

    bt_message_discarded_events_set_count(msg, *count);
    msgOut = bt2::Message::Shared::createWithoutRef(msg);

    return LTTNG_LIVE_ITERATOR_STATUS_OK;
}

static enum lttng_live_iterator_status
handle_late_message(struct lttng_live_msg_iter *lttng_live_msg_iter,
                    struct lttng_live_stream_iterator *stream_iter, int64_t late_msg_ts_ns,
                    bt2::ConstMessage::Shared lateMsg)
{
    const ctf::LogCfg& logCfg = lttng_live_msg_iter->logCfg;

    /*
     * The timestamp of the current message is before the last message sent
     * by this component. We CANNOT send it as is.
     *
     * The only expected scenario in which that could happen is the
     * following, everything else is a bug in this component, relay deamon,
     * or CTF parser.
     *
     * Expected scenario: The CTF message iterator emitted discarded
     * packets and discarded events with synthesized beginning and end
     * timestamps from the bounds of the last known packet and the newly
     * decoded packet header. The CTF message iterator is not aware of
     * stream inactivity beacons. Hence, we have to adjust the beginning
     * timestamp of those types of messages if a stream signalled its
     * inactivity up until _after_ the last known packet's beginning
     * timestamp.
     *
     * Otherwise, the monotonicity guarantee of message timestamps would
     * not be preserved.
     *
     * In short, the only scenario in which it's okay and fixable to
     * received a late message is when:
     *  1. the late message is a discarded packets or discarded events
     *	message,
     *  2. this stream produced an inactivity message downstream, and
     *  3. the timestamp of the late message is within the inactivity
     *	timespan we sent downstream through the inactivity message.
     */

    BT_COMP_LOGD("Handling late message on live stream iterator: "
                 "stream-name=\"%s\", viewer-stream-id=%" PRIu64,
                 stream_iter->name.c_str(), stream_iter->viewer_stream_id);

    if (!stream_iter->last_inactivity_ts.is_set) {
        BT_COMP_LOGE_APPEND_CAUSE(logCfg.selfComp, "Invalid live stream state: "
                                                   "have a late message when no inactivity message "
                                                   "was ever sent for that stream.");
        return LTTNG_LIVE_ITERATOR_STATUS_ERROR;
    }

    if (!is_discarded_packet_or_event_message(*lateMsg)) {
        BT_COMP_LOGE_APPEND_CAUSE(logCfg.selfComp,
                                  "Invalid live stream state: "
                                  "have a late message that is not a packet discarded or "
                                  "event discarded message: late-msg-type=%s",
                                  bt2::MessageTypeStr(lateMsg->type()));
        return LTTNG_LIVE_ITERATOR_STATUS_ERROR;
    }

    bt2::StreamClass streamClass = (*stream_iter->stream)->cls();
    nonstd::optional<bt2::ClockClass> clockClass = streamClass.defaultClockClass();

    int64_t last_inactivity_ts_ns =
        clockClass->cyclesToNsFromOrigin(stream_iter->last_inactivity_ts.value);
    if (last_inactivity_ts_ns <= late_msg_ts_ns) {
        BT_COMP_LOGE_APPEND_CAUSE(logCfg.selfComp,
                                  "Invalid live stream state: "
                                  "have a late message that is none included in a stream "
                                  "inactivity timespan: last-inactivity-ts-ns=%" PRIu64
                                  "late-msg-ts-ns=%" PRIu64,
                                  last_inactivity_ts_ns, late_msg_ts_ns);
        return LTTNG_LIVE_ITERATOR_STATUS_ERROR;
    }

    /*
     * We now know that it's okay for this message to be late, we can now
     * adjust its timestamp to ensure monotonicity.
     */
    BT_COMP_LOGD("Adjusting the timestamp of late message: late-msg-type=%s, "
                 "msg-new-ts-ns=%" PRIu64,
                 bt2::MessageTypeStr(lateMsg->type()), stream_iter->last_inactivity_ts.value);
    nonstd::optional<bt2::Message::Shared> adjustedMessage;
    lttng_live_iterator_status adjust_status;
    switch (lateMsg->type()) {
    case bt2::MessageType::DISCARDED_EVENTS:
        adjust_status = adjust_discarded_events_message(
            lttng_live_msg_iter->self_msg_iter, **stream_iter->stream, lateMsg->asDiscardedEvents(),
            adjustedMessage, stream_iter->last_inactivity_ts.value);
        break;
    case bt2::MessageType::DISCARDED_PACKETS:
        adjust_status = adjust_discarded_packets_message(
            lttng_live_msg_iter->self_msg_iter, **stream_iter->stream,
            lateMsg->asDiscardedPackets(), adjustedMessage, stream_iter->last_inactivity_ts.value);
        break;
    default:
        bt_common_abort();
    }

    if (adjust_status != LTTNG_LIVE_ITERATOR_STATUS_OK) {
        return adjust_status;
    }

    BT_ASSERT_DBG(adjustedMessage);
    stream_iter->currentMsg = std::move(adjustedMessage);
    stream_iter->current_msg_ts_ns = last_inactivity_ts_ns;

    return LTTNG_LIVE_ITERATOR_STATUS_OK;
}

template <typename T>
static void vectorFastRemove(std::vector<T>& vec, size_t idx)
{
    BT_ASSERT_DBG(idx < vec.size());

    if (idx < vec.size() - 1) {
        vec[idx] = std::move(vec.back());
    }

    vec.pop_back();
}

static enum lttng_live_iterator_status
next_stream_iterator_for_trace(struct lttng_live_msg_iter *lttng_live_msg_iter,
                               struct lttng_live_trace *live_trace,
                               struct lttng_live_stream_iterator **youngest_trace_stream_iter)
{
    struct lttng_live_stream_iterator *youngest_candidate_stream_iter = NULL;
    const ctf::LogCfg& logCfg = lttng_live_msg_iter->logCfg;
    int64_t youngest_candidate_msg_ts = INT64_MAX;

    BT_ASSERT_DBG(live_trace);

    BT_COMP_LOGD("Finding the next stream iterator for trace: "
                 "trace-id=%" PRIu64,
                 live_trace->id);
    /*
     * Update the current message of every stream iterators of this trace.
     * The current msg of every stream must have a timestamp equal or
     * larger than the last message returned by this iterator. We must
     * ensure monotonicity.
     */
    uint64_t stream_iter_idx = 0;
    while (stream_iter_idx < live_trace->streamIterators.size()) {
        bool stream_iter_is_ended = false;
        lttng_live_stream_iterator *stream_iter =
            live_trace->streamIterators[stream_iter_idx].get();

        /*
         * If there is no current message for this stream, go fetch
         * one.
         */
        while (!stream_iter->currentMsg) {
            nonstd::optional<bt2::ConstMessage::Shared> msg;
            int64_t curr_msg_ts_ns = INT64_MAX;

            lttng_live_iterator_status stream_iter_status =
                lttng_live_iterator_next_msg_on_stream(lttng_live_msg_iter, stream_iter, msg);

            if (stream_iter_status == LTTNG_LIVE_ITERATOR_STATUS_END) {
                stream_iter_is_ended = true;
                break;
            }

            if (stream_iter_status != LTTNG_LIVE_ITERATOR_STATUS_OK) {
                return stream_iter_status;
            }

            BT_ASSERT_DBG(msg);

            BT_COMP_LOGD("Live stream iterator returned message: msg-type=%s, "
                         "stream-name=\"%s\", viewer-stream-id=%" PRIu64,
                         bt2::MessageTypeStr((*msg)->type()), stream_iter->name.c_str(),
                         stream_iter->viewer_stream_id);

            /*
             * Get the timestamp in nanoseconds from origin of this
             * messsage.
             */
            live_get_msg_ts_ns(stream_iter, lttng_live_msg_iter, **msg,
                               lttng_live_msg_iter->last_msg_ts_ns, &curr_msg_ts_ns);

            /*
             * Check if the message of the current live stream
             * iterator occurred at the exact same time or after the
             * last message returned by this component's message
             * iterator. If not, we need to handle it with care.
             */
            if (curr_msg_ts_ns >= lttng_live_msg_iter->last_msg_ts_ns) {
                stream_iter->currentMsg = std::move(msg);
                stream_iter->current_msg_ts_ns = curr_msg_ts_ns;
            } else {
                /*
                 * We received a message from the past. This
                 * may be fixable but it can also be an error.
                 */
                stream_iter_status = handle_late_message(lttng_live_msg_iter, stream_iter,
                                                         curr_msg_ts_ns, std::move(*msg));
                if (stream_iter_status != LTTNG_LIVE_ITERATOR_STATUS_OK) {
                    BT_COMP_LOGE_APPEND_CAUSE(logCfg.selfComp,
                                              "Late message could not be handled correctly: "
                                              "lttng-live-msg-iter-addr=%p, "
                                              "stream-name=\"%s\", "
                                              "curr-msg-ts=%" PRId64 ", last-msg-ts=%" PRId64,
                                              lttng_live_msg_iter, stream_iter->name.c_str(),
                                              curr_msg_ts_ns, lttng_live_msg_iter->last_msg_ts_ns);
                    return LTTNG_LIVE_ITERATOR_STATUS_ERROR;
                }
            }
        }

        BT_ASSERT_DBG(stream_iter != youngest_candidate_stream_iter);

        if (!stream_iter_is_ended) {
            if (G_UNLIKELY(youngest_candidate_stream_iter == NULL) ||
                stream_iter->current_msg_ts_ns < youngest_candidate_msg_ts) {
                /*
                 * Update the current best candidate message
                 * for the stream iterator of this live trace
                 * to be forwarded downstream.
                 */
                youngest_candidate_msg_ts = stream_iter->current_msg_ts_ns;
                youngest_candidate_stream_iter = stream_iter;
            } else if (stream_iter->current_msg_ts_ns == youngest_candidate_msg_ts) {
                /*
                 * Order the messages in an arbitrary but
                 * deterministic way.
                 */
                BT_ASSERT_DBG(stream_iter != youngest_candidate_stream_iter);
                int ret = common_muxing_compare_messages(
                    (*stream_iter->currentMsg)->libObjPtr(),
                    (*youngest_candidate_stream_iter->currentMsg)->libObjPtr());
                if (ret < 0) {
                    /*
                     * The `youngest_candidate_stream_iter->current_msg`
                     * should go first. Update the next
                     * iterator and the current timestamp.
                     */
                    youngest_candidate_msg_ts = stream_iter->current_msg_ts_ns;
                    youngest_candidate_stream_iter = stream_iter;
                } else if (ret == 0) {
                    /*
                     * Unable to pick which one should go
                     * first.
                     */
                    BT_COMP_LOGW(
                        "Cannot deterministically pick next live stream message iterator because they have identical next messages: "
                        "stream-iter-addr=%p"
                        "stream-iter-addr=%p",
                        stream_iter, youngest_candidate_stream_iter);
                }
            }

            stream_iter_idx++;
        } else {
            /*
             * The live stream iterator has ended. That
             * iterator is removed from the array, but
             * there is no need to increment
             * stream_iter_idx as
             * g_ptr_array_remove_index_fast replaces the
             * removed element with the array's last
             * element.
             */
            vectorFastRemove(live_trace->streamIterators, stream_iter_idx);
        }
    }

    if (youngest_candidate_stream_iter) {
        *youngest_trace_stream_iter = youngest_candidate_stream_iter;
        return LTTNG_LIVE_ITERATOR_STATUS_OK;
    } else {
        /*
         * The only case where we don't have a candidate for this trace
         * is if we reached the end of all the iterators.
         */
        BT_ASSERT(live_trace->streamIterators.empty());
        return LTTNG_LIVE_ITERATOR_STATUS_END;
    }
}

static enum lttng_live_iterator_status
next_stream_iterator_for_session(struct lttng_live_msg_iter *lttng_live_msg_iter,
                                 struct lttng_live_session *session,
                                 struct lttng_live_stream_iterator **youngest_session_stream_iter)
{
    const ctf::LogCfg& logCfg = lttng_live_msg_iter->logCfg;
    enum lttng_live_iterator_status stream_iter_status;
    uint64_t trace_idx = 0;
    int64_t youngest_candidate_msg_ts = INT64_MAX;
    struct lttng_live_stream_iterator *youngest_candidate_stream_iter = NULL;

    BT_COMP_LOGD("Finding the next stream iterator for session: "
                 "session-id=%" PRIu64,
                 session->id);
    /*
     * Make sure we are attached to the session and look for new streams
     * and metadata.
     */
    stream_iter_status = lttng_live_get_session(lttng_live_msg_iter, session);
    if (stream_iter_status != LTTNG_LIVE_ITERATOR_STATUS_OK &&
        stream_iter_status != LTTNG_LIVE_ITERATOR_STATUS_CONTINUE &&
        stream_iter_status != LTTNG_LIVE_ITERATOR_STATUS_END) {
        goto end;
    }

    while (trace_idx < session->traces.size()) {
        bool trace_is_ended = false;
        struct lttng_live_stream_iterator *stream_iter;
        lttng_live_trace *trace = session->traces[trace_idx].get();

        stream_iter_status =
            next_stream_iterator_for_trace(lttng_live_msg_iter, trace, &stream_iter);
        if (stream_iter_status == LTTNG_LIVE_ITERATOR_STATUS_END) {
            /*
             * All the live stream iterators for this trace are
             * ENDed. Remove the trace from this session.
             */
            trace_is_ended = true;
        } else if (stream_iter_status != LTTNG_LIVE_ITERATOR_STATUS_OK) {
            goto end;
        }

        if (!trace_is_ended) {
            BT_ASSERT_DBG(stream_iter);

            if (G_UNLIKELY(youngest_candidate_stream_iter == NULL) ||
                stream_iter->current_msg_ts_ns < youngest_candidate_msg_ts) {
                youngest_candidate_msg_ts = stream_iter->current_msg_ts_ns;
                youngest_candidate_stream_iter = stream_iter;
            } else if (stream_iter->current_msg_ts_ns == youngest_candidate_msg_ts) {
                /*
                 * Order the messages in an arbitrary but
                 * deterministic way.
                 */
                int ret = common_muxing_compare_messages(
                    (*stream_iter->currentMsg)->libObjPtr(),
                    (*youngest_candidate_stream_iter->currentMsg)->libObjPtr());
                if (ret < 0) {
                    /*
                     * The `youngest_candidate_stream_iter->current_msg`
                     * should go first. Update the next iterator
                     * and the current timestamp.
                     */
                    youngest_candidate_msg_ts = stream_iter->current_msg_ts_ns;
                    youngest_candidate_stream_iter = stream_iter;
                } else if (ret == 0) {
                    /* Unable to pick which one should go first. */
                    BT_COMP_LOGW(
                        "Cannot deterministically pick next live stream message iterator because they have identical next messages: "
                        "stream-iter-addr=%p"
                        "stream-iter-addr=%p",
                        stream_iter, youngest_candidate_stream_iter);
                }
            }
            trace_idx++;
        } else {
            /*
             * trace_idx is not incremented since
             * g_ptr_array_remove_index_fast replaces the
             * element at trace_idx with the array's last element.
             */
            vectorFastRemove(session->traces, trace_idx);
        }
    }
    if (youngest_candidate_stream_iter) {
        *youngest_session_stream_iter = youngest_candidate_stream_iter;
        stream_iter_status = LTTNG_LIVE_ITERATOR_STATUS_OK;
    } else {
        /*
         * The only cases where we don't have a candidate for this
         * trace is:
         *  1. if we reached the end of all the iterators of all the
         *  traces of this session,
         *  2. if we never had live stream iterator in the first place.
         *
         * In either cases, we return END.
         */
        BT_ASSERT(session->traces.empty());
        stream_iter_status = LTTNG_LIVE_ITERATOR_STATUS_END;
    }
end:
    return stream_iter_status;
}

static inline void put_messages(bt_message_array_const msgs, uint64_t count)
{
    uint64_t i;

    for (i = 0; i < count; i++) {
        BT_MESSAGE_PUT_REF_AND_RESET(msgs[i]);
    }
}

BT_HIDDEN
bt_message_iterator_class_next_method_status
lttng_live_msg_iter_next(bt_self_message_iterator *self_msg_it, bt_message_array_const msgs,
                         uint64_t capacity, uint64_t *count)
{
    struct lttng_live_msg_iter *lttng_live_msg_iter =
        (struct lttng_live_msg_iter *) bt_self_message_iterator_get_data(self_msg_it);
    const ctf::LogCfg& logCfg = lttng_live_msg_iter->logCfg;

    try {
        bt_message_iterator_class_next_method_status status;
        enum lttng_live_viewer_status viewer_status;

        struct lttng_live_component *lttng_live = lttng_live_msg_iter->lttng_live_comp;
        enum lttng_live_iterator_status stream_iter_status;

        *count = 0;

        BT_ASSERT_DBG(lttng_live_msg_iter);

        if (G_UNLIKELY(lttng_live_msg_iter->was_interrupted)) {
            /*
             * The iterator was interrupted in a previous call to the
             * `_next()` method. We currently do not support generating
             * messages after such event. The babeltrace2 CLI should never
             * be running the graph after being interrupted. So this check
             * is to prevent other graph users from using this live
             * iterator in an messed up internal state.
             */
            status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_ERROR;
            BT_COMP_LOGE_APPEND_CAUSE(
                logCfg.selfComp,
                "Message iterator was interrupted during a previous call to the `next()` and currently does not support continuing after such event.");
            goto end;
        }

        /*
         * Clear all the invalid message reference that might be left over in
         * the output array.
         */
        memset(msgs, 0, capacity * sizeof(*msgs));

        /*
         * If no session are exposed on the relay found at the url provided by
         * the user, session count will be 0. In this case, we return status
         * end to return gracefully.
         */
        if (lttng_live_msg_iter->sessions.empty()) {
            if (lttng_live->params.sess_not_found_act != SESSION_NOT_FOUND_ACTION_CONTINUE) {
                status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_END;
                goto end;
            } else {
                /*
                 * The are no more active session for this session
                 * name. Retry to create a viewer session for the
                 * requested session name.
                 */
                viewer_status = lttng_live_create_viewer_session(lttng_live_msg_iter);
                if (viewer_status != LTTNG_LIVE_VIEWER_STATUS_OK) {
                    if (viewer_status == LTTNG_LIVE_VIEWER_STATUS_ERROR) {
                        status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_ERROR;
                        BT_COMP_LOGE_APPEND_CAUSE(logCfg.selfComp,
                                                  "Error creating LTTng live viewer session");
                    } else if (viewer_status == LTTNG_LIVE_VIEWER_STATUS_INTERRUPTED) {
                        status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_AGAIN;
                    } else {
                        bt_common_abort();
                    }
                    goto end;
                }
            }
        }

        if (lttng_live_msg_iter->active_stream_iter == 0) {
            lttng_live_force_new_streams_and_metadata(lttng_live_msg_iter);
        }

        /*
         * Here the muxing of message is done.
         *
         * We need to iterate over all the streams of all the traces of all the
         * viewer sessions in order to get the message with the smallest
         * timestamp. In this case, a session is a viewer session and there is
         * one viewer session per consumer daemon. (UST 32bit, UST 64bit and/or
         * kernel). Each viewer session can have multiple traces, for example,
         * 64bit UST viewer sessions could have multiple per-pid traces.
         *
         * We iterate over the streams of each traces to update and see what is
         * their next message's timestamp. From those timestamps, we select the
         * message with the smallest timestamp as the best candidate message
         * for that trace and do the same thing across all the sessions.
         *
         * We then compare the timestamp of best candidate message of all the
         * sessions to pick the message with the smallest timestamp and we
         * return it.
         */
        while (*count < capacity) {
            struct lttng_live_stream_iterator *youngest_stream_iter = NULL,
                                              *candidate_stream_iter = NULL;
            int64_t youngest_msg_ts_ns = INT64_MAX;

            uint64_t session_idx = 0;
            while (session_idx < lttng_live_msg_iter->sessions.size()) {
                lttng_live_session *session = lttng_live_msg_iter->sessions[session_idx].get();

                /* Find the best candidate message to send downstream. */
                stream_iter_status = next_stream_iterator_for_session(lttng_live_msg_iter, session,
                                                                      &candidate_stream_iter);

                /*
                 * If we receive an END status, it means that either:
                 * - Those traces never had active streams (UST with no
                 *   data produced yet),
                 * - All live stream iterators have ENDed.
                 */
                if (stream_iter_status == LTTNG_LIVE_ITERATOR_STATUS_END) {
                    if (session->closed && session->traces.empty() == 0) {
                        /*
                         * Remove the session from the list.
                         * session_idx is not modified since
                         * g_ptr_array_remove_index_fast
                         * replaces the the removed element with
                         * the array's last element.
                         */
                        vectorFastRemove(lttng_live_msg_iter->sessions, session_idx);
                    } else {
                        session_idx++;
                    }
                    continue;
                }

                if (stream_iter_status != LTTNG_LIVE_ITERATOR_STATUS_OK) {
                    goto return_status;
                }

                if (G_UNLIKELY(youngest_stream_iter == NULL) ||
                    candidate_stream_iter->current_msg_ts_ns < youngest_msg_ts_ns) {
                    youngest_msg_ts_ns = candidate_stream_iter->current_msg_ts_ns;
                    youngest_stream_iter = candidate_stream_iter;
                } else if (candidate_stream_iter->current_msg_ts_ns == youngest_msg_ts_ns) {
                    /*
                     * The currently selected message to be sent
                     * downstream next has the exact same timestamp
                     * that of the current candidate message. We
                     * must break the tie in a predictable manner.
                     */
                    BT_COMP_LOGD_STR(
                        "Two of the next message candidates have the same timestamps, pick one deterministically.");
                    /*
                     * Order the messages in an arbitrary but
                     * deterministic way.
                     */
                    int ret = common_muxing_compare_messages(
                        (*candidate_stream_iter->currentMsg)->libObjPtr(),
                        (*youngest_stream_iter->currentMsg)->libObjPtr());
                    if (ret < 0) {
                        /*
                         * The `candidate_stream_iter->current_msg`
                         * should go first. Update the next
                         * iterator and the current timestamp.
                         */
                        youngest_msg_ts_ns = candidate_stream_iter->current_msg_ts_ns;
                        youngest_stream_iter = candidate_stream_iter;
                    } else if (ret == 0) {
                        /* Unable to pick which one should go first. */
                        BT_COMP_LOGW(
                            "Cannot deterministically pick next live stream message iterator because they have identical next messages: "
                            "next-stream-iter-addr=%p"
                            "candidate-stream-iter-addr=%p",
                            youngest_stream_iter, candidate_stream_iter);
                    }
                }

                session_idx++;
            }

            if (!youngest_stream_iter) {
                stream_iter_status = LTTNG_LIVE_ITERATOR_STATUS_AGAIN;
                goto return_status;
            }

            BT_ASSERT_DBG(youngest_stream_iter->currentMsg);
            /* Ensure monotonicity. */
            BT_ASSERT_DBG(lttng_live_msg_iter->last_msg_ts_ns <=
                          youngest_stream_iter->current_msg_ts_ns);

            /*
             * Insert the next message to the message batch. This will set
             * stream iterator current messsage to NULL so that next time
             * we fetch the next message of that stream iterator
             */
            msgs[*count] = youngest_stream_iter->currentMsg->release().libObjPtr();
            (*count)++;

            /* Update the last timestamp in nanoseconds sent downstream. */
            lttng_live_msg_iter->last_msg_ts_ns = youngest_msg_ts_ns;
            youngest_stream_iter->current_msg_ts_ns = INT64_MAX;

            stream_iter_status = LTTNG_LIVE_ITERATOR_STATUS_OK;
        }

return_status:
        switch (stream_iter_status) {
        case LTTNG_LIVE_ITERATOR_STATUS_OK:
        case LTTNG_LIVE_ITERATOR_STATUS_AGAIN:
            /*
             * If we gathered messages, return _OK even if the graph was
             * interrupted. This allows for the components downstream to at
             * least get the thoses messages. If the graph was indeed
             * interrupted there should not be another _next() call as the
             * application will tear down the graph. This component class
             * doesn't support restarting after an interruption.
             */
            if (*count > 0) {
                status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK;
            } else {
                status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_AGAIN;
            }
            break;
        case LTTNG_LIVE_ITERATOR_STATUS_END:
            status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_END;
            break;
        case LTTNG_LIVE_ITERATOR_STATUS_NOMEM:
            BT_COMP_LOGE_APPEND_CAUSE(logCfg.selfComp,
                                      "Memory error preparing the next batch of messages: "
                                      "live-iter-status=%s",
                                      lttng_live_iterator_status_string(stream_iter_status));
            status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_MEMORY_ERROR;
            break;
        case LTTNG_LIVE_ITERATOR_STATUS_ERROR:
        case LTTNG_LIVE_ITERATOR_STATUS_INVAL:
        case LTTNG_LIVE_ITERATOR_STATUS_UNSUPPORTED:
            BT_COMP_LOGE_APPEND_CAUSE(logCfg.selfComp,
                                      "Error preparing the next batch of messages: "
                                      "live-iter-status=%s",
                                      lttng_live_iterator_status_string(stream_iter_status));

            status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_ERROR;
            /* Put all existing messages on error. */
            put_messages(msgs, *count);
            break;
        default:
            bt_common_abort();
        }

end:
        return status;
    } catch (const std::bad_alloc&) {
        return BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_MEMORY_ERROR;
    } catch (const bt2_common::Error&) {
        BT_COMP_LOGE_APPEND_CAUSE(logCfg.selfComp, "Failed to fetch next messages");
        return BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_ERROR;
    }
}

static lttng_live_msg_iter::UP
lttng_live_msg_iter_create(struct lttng_live_component *lttng_live_comp,
                           bt_self_message_iterator *self_msg_it)
{
    const ctf::LogCfg& logCfg = lttng_live_comp->logCfg;
    lttng_live_msg_iter::UP lttng_live_msg_iter =
        bt2_common::makeUnique<struct lttng_live_msg_iter>(logCfg);
    lttng_live_msg_iter->self_comp = lttng_live_comp->self_comp;
    lttng_live_msg_iter->lttng_live_comp = lttng_live_comp;
    lttng_live_msg_iter->self_msg_iter = self_msg_it;

    lttng_live_msg_iter->active_stream_iter = 0;
    lttng_live_msg_iter->last_msg_ts_ns = INT64_MIN;
    lttng_live_msg_iter->was_interrupted = false;

    return lttng_live_msg_iter;
}

BT_HIDDEN
bt_message_iterator_class_initialize_method_status
lttng_live_msg_iter_init(bt_self_message_iterator *self_msg_it,
                         bt_self_message_iterator_configuration *config,
                         bt_self_component_port_output *self_port)
{
    bt_self_component *self_comp = bt_self_message_iterator_borrow_component(self_msg_it);
    lttng_live_component *lttng_live =
        (lttng_live_component *) bt_self_component_get_data(self_comp);
    const ctf::LogCfg& logCfg = lttng_live->logCfg;

    try {
        /* There can be only one downstream iterator at the same time. */
        BT_ASSERT(!lttng_live->has_msg_iter);
        lttng_live->has_msg_iter = true;

        lttng_live_msg_iter::UP lttng_live_msg_iter =
            lttng_live_msg_iter_create(lttng_live, self_msg_it);
        if (!lttng_live_msg_iter) {
            BT_COMP_LOGE_APPEND_CAUSE(logCfg.selfComp, "Failed to create lttng_live_msg_iter");
            return BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR;
        }

        lttng_live_viewer_status viewer_status = live_viewer_connection_create(
            lttng_live->params.url.c_str(), false, lttng_live_msg_iter.get(), logCfg,
            lttng_live_msg_iter->viewer_connection);
        if (viewer_status != LTTNG_LIVE_VIEWER_STATUS_OK) {
            if (viewer_status == LTTNG_LIVE_VIEWER_STATUS_ERROR) {
                BT_COMP_LOGE_APPEND_CAUSE(logCfg.selfComp, "Failed to create viewer connection");
            } else if (viewer_status == LTTNG_LIVE_VIEWER_STATUS_INTERRUPTED) {
                /*
                 * Interruption in the _iter_init() method is not
                 * supported. Return an error.
                 */
                BT_COMP_LOGE_APPEND_CAUSE(logCfg.selfComp,
                                          "Interrupted while creating viewer connection");
            }

            return BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
        }

        viewer_status = lttng_live_create_viewer_session(lttng_live_msg_iter.get());
        if (viewer_status != LTTNG_LIVE_VIEWER_STATUS_OK) {
            if (viewer_status == LTTNG_LIVE_VIEWER_STATUS_ERROR) {
                BT_COMP_LOGE_APPEND_CAUSE(logCfg.selfComp, "Failed to create viewer session");
            } else if (viewer_status == LTTNG_LIVE_VIEWER_STATUS_INTERRUPTED) {
                /*
                 * Interruption in the _iter_init() method is not
                 * supported. Return an error.
                 */
                BT_COMP_LOGE_APPEND_CAUSE(logCfg.selfComp,
                                          "Interrupted when creating viewer session");
            }

            return BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
        }

        if (lttng_live_msg_iter->sessions.empty()) {
            switch (lttng_live->params.sess_not_found_act) {
            case SESSION_NOT_FOUND_ACTION_CONTINUE:
                BT_COMP_LOGI(
                    "Unable to connect to the requested live viewer session. Keep trying to connect because of "
                    "%s=\"%s\" component parameter: url=\"%s\"",
                    SESS_NOT_FOUND_ACTION_PARAM, SESS_NOT_FOUND_ACTION_CONTINUE_STR,
                    lttng_live->params.url.c_str());
                break;

            case SESSION_NOT_FOUND_ACTION_FAIL:
                BT_COMP_LOGE_APPEND_CAUSE(
                    self_comp,
                    "Unable to connect to the requested live viewer session. Fail the message iterator initialization because of %s=\"%s\" "
                    "component parameter: url =\"%s\"",
                    SESS_NOT_FOUND_ACTION_PARAM, SESS_NOT_FOUND_ACTION_FAIL_STR,
                    lttng_live->params.url.c_str());
                return BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_ERROR;

            case SESSION_NOT_FOUND_ACTION_END:
                BT_COMP_LOGI(
                    "Unable to connect to the requested live viewer session. End gracefully at the first _next() "
                    "call because of %s=\"%s\" component parameter: "
                    "url=\"%s\"",
                    SESS_NOT_FOUND_ACTION_PARAM, SESS_NOT_FOUND_ACTION_END_STR,
                    lttng_live->params.url.c_str());
                break;

            default:
                bt_common_abort();
            }
        }

        bt_self_message_iterator_set_data(self_msg_it, lttng_live_msg_iter.release());

        return BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_OK;
    } catch (const std::bad_alloc&) {
        return BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR;
    } catch (const bt2_common::Error&) {
        BT_COMP_LOGE_APPEND_CAUSE(logCfg.selfComp, "Failed to initialize iterator");
        return BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
    }
}

static struct bt_param_validation_map_value_entry_descr list_sessions_params[] = {
    {URL_PARAM, BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY,
     bt_param_validation_value_descr::makeString()},
    BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END};

static bt_component_class_query_method_status
lttng_live_query_list_sessions(bt2::ConstMapValue params,
                               nonstd::optional<bt2::Value::Shared>& result,
                               const ctf::LogCfg& logCfg)
{
    bt_component_class_query_method_status status;
    enum lttng_live_viewer_status viewer_status;
    enum bt_param_validation_status validation_status;
    gchar *validate_error = NULL;

    validation_status =
        bt_param_validation_validate(params.libObjPtr(), list_sessions_params, &validate_error);
    if (validation_status == BT_PARAM_VALIDATION_STATUS_MEMORY_ERROR) {
        return BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_MEMORY_ERROR;
    } else if (validation_status == BT_PARAM_VALIDATION_STATUS_VALIDATION_ERROR) {
        BT_COMP_CLASS_LOGE_APPEND_CAUSE(logCfg.selfCompClass, "%s", validate_error);
        g_free(validate_error);
        return BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_ERROR;
    }

    bt2::ConstStringValue urlValue = params[URL_PARAM]->asString();
    const char *url = urlValue.value().c_str();

    live_viewer_connection::UP viewer_connection;
    viewer_status = live_viewer_connection_create(url, true, NULL, logCfg, viewer_connection);
    if (viewer_status != LTTNG_LIVE_VIEWER_STATUS_OK) {
        if (viewer_status == LTTNG_LIVE_VIEWER_STATUS_ERROR) {
            BT_COMP_CLASS_LOGE_APPEND_CAUSE(logCfg.selfCompClass,
                                            "Failed to create viewer connection");
            return BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_ERROR;
        } else if (viewer_status == LTTNG_LIVE_VIEWER_STATUS_INTERRUPTED) {
            return BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_AGAIN;
        } else {
            bt_common_abort();
        }
    }

    status = live_viewer_connection_list_sessions(viewer_connection.get(), result);
    if (status != BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_OK) {
        BT_COMP_CLASS_LOGE_APPEND_CAUSE(logCfg.selfCompClass, "Failed to list viewer sessions");
        return status;
    }

    return BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_OK;
}

static bt_component_class_query_method_status
lttng_live_query_support_info(bt2::ConstMapValue params,
                              nonstd::optional<bt2::Value::Shared>& result,
                              const ctf::LogCfg& logCfg)
{
    /* Used by the logging macros */
    __attribute__((unused)) bt_self_component *self_comp = NULL;

    nonstd::optional<bt2::ConstValue> typeValue = params["type"];
    if (!typeValue) {
        BT_COMP_CLASS_LOGE_APPEND_CAUSE(logCfg.selfCompClass, "Missing expected `type` parameter.");
        return BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_ERROR;
    }

    if (!typeValue->isString()) {
        BT_COMP_CLASS_LOGE_APPEND_CAUSE(logCfg.selfCompClass,
                                        "`type` parameter is not a string value.");
        return BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_ERROR;
    }

    if (typeValue->asString().value() != "string") {
        /* We don't handle file system paths */
        result = bt2::RealValue::create(0);
        return BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_OK;
    }

    nonstd::optional<bt2::ConstValue> inputValue = params["input"];
    if (!inputValue) {
        BT_COMP_CLASS_LOGE_APPEND_CAUSE(logCfg.selfCompClass,
                                        "Missing expected `input` parameter.");
        return BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_ERROR;
    }

    if (!inputValue->isString()) {
        BT_COMP_CLASS_LOGE_APPEND_CAUSE(logCfg.selfCompClass,
                                        "`input` parameter is not a string value.");
        return BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_ERROR;
    }

    bt_common_lttng_live_url_parts parts =
        bt_common_parse_lttng_live_url(inputValue->asString().value().c_str(), NULL, 0);
    double weight = 0;
    if (parts.session_name) {
        /*
         * Looks pretty much like an LTTng live URL: we got the
         * session name part, which forms a complete URL.
         */
        weight = .75;
    }

    bt_common_destroy_lttng_live_url_parts(&parts);

    result = bt2::RealValue::create(weight);
    return BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_OK;
}

BT_HIDDEN
bt_component_class_query_method_status lttng_live_query(bt_self_component_class_source *comp_class,
                                                        bt_private_query_executor *priv_query_exec,
                                                        const char *object, const bt_value *params,
                                                        __attribute__((unused)) void *method_data,
                                                        const bt_value **result)
{
    bt_self_component_class *self_comp_class =
        bt_self_component_class_source_as_self_component_class(comp_class);
    bt_logging_level log_level = bt_query_executor_get_logging_level(
        bt_private_query_executor_as_query_executor_const(priv_query_exec));
    ctf::LogCfg logCfg(log_level, self_comp_class);
    nonstd::optional<bt2::Value::Shared> resultObj;
    bt2::ConstMapValue paramsObj {params};

    try {
        bt_component_class_query_method_status status;

        if (strcmp(object, "sessions") == 0) {
            status = lttng_live_query_list_sessions(paramsObj, resultObj, logCfg);
        } else if (strcmp(object, "babeltrace.support-info") == 0) {
            status = lttng_live_query_support_info(paramsObj, resultObj, logCfg);
        } else {
            BT_COMP_LOGI("Unknown query object `%s`", object);
            status = BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_UNKNOWN_OBJECT;
        }

        if (status == BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_OK) {
            *result = resultObj->release().libObjPtr();
        }

        return status;
    } catch (const std::bad_alloc&) {
        return BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_MEMORY_ERROR;
    } catch (const bt2_common::Error&) {
        BT_COMP_LOGE_APPEND_CAUSE(logCfg.selfComp, "Failed to execute query");
        return BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_ERROR;
    }
}

BT_HIDDEN
void lttng_live_component_finalize(bt_self_component_source *component)
{
    lttng_live_component::UP {(lttng_live_component *) bt_self_component_get_data(
        bt_self_component_source_as_self_component(component))};
}

static enum session_not_found_action
parse_session_not_found_action_param(const bt_value *no_session_param)
{
    enum session_not_found_action action;
    const char *no_session_act_str = bt_value_string_get(no_session_param);

    if (strcmp(no_session_act_str, SESS_NOT_FOUND_ACTION_CONTINUE_STR) == 0) {
        action = SESSION_NOT_FOUND_ACTION_CONTINUE;
    } else if (strcmp(no_session_act_str, SESS_NOT_FOUND_ACTION_FAIL_STR) == 0) {
        action = SESSION_NOT_FOUND_ACTION_FAIL;
    } else {
        BT_ASSERT(strcmp(no_session_act_str, SESS_NOT_FOUND_ACTION_END_STR) == 0);
        action = SESSION_NOT_FOUND_ACTION_END;
    }

    return action;
}

static bt_param_validation_value_descr inputs_elem_descr =
    bt_param_validation_value_descr::makeString();

static const char *sess_not_found_action_choices[] = {
    SESS_NOT_FOUND_ACTION_CONTINUE_STR,
    SESS_NOT_FOUND_ACTION_FAIL_STR,
    SESS_NOT_FOUND_ACTION_END_STR,
};

static struct bt_param_validation_map_value_entry_descr params_descr[] = {
    {INPUTS_PARAM, BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY,
     bt_param_validation_value_descr::makeArray(1, 1, inputs_elem_descr)},
    {SESS_NOT_FOUND_ACTION_PARAM, BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeString(sess_not_found_action_choices)},
    BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END};

static bt_component_class_initialize_method_status
lttng_live_component_create(const bt_value *params, bt_self_component *self_comp,
                            const ctf::LogCfg& logCfg, lttng_live_component::UP& component)
{
    enum bt_param_validation_status validation_status;
    gchar *validation_error = NULL;

    validation_status = bt_param_validation_validate(params, params_descr, &validation_error);
    if (validation_status == BT_PARAM_VALIDATION_STATUS_MEMORY_ERROR) {
        return BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR;
    } else if (validation_status == BT_PARAM_VALIDATION_STATUS_VALIDATION_ERROR) {
        BT_COMP_LOGE_APPEND_CAUSE(logCfg.selfComp, "%s", validation_error);
        g_free(validation_error);
        return BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
    }

    lttng_live_component::UP lttng_live = bt2_common::makeUnique<lttng_live_component>(logCfg);
    lttng_live->self_comp = self_comp;
    lttng_live->max_query_size = MAX_QUERY_SIZE;
    lttng_live->has_msg_iter = false;

    const bt_value *inputs_value = bt_value_map_borrow_entry_value_const(params, INPUTS_PARAM);
    const bt_value *url_value = bt_value_array_borrow_element_by_index_const(inputs_value, 0);
    lttng_live->params.url = bt_value_string_get(url_value);

    const bt_value *value =
        bt_value_map_borrow_entry_value_const(params, SESS_NOT_FOUND_ACTION_PARAM);
    if (value) {
        lttng_live->params.sess_not_found_act = parse_session_not_found_action_param(value);
    } else {
        BT_COMP_LOGI("Optional `%s` parameter is missing: "
                     "defaulting to `%s`.",
                     SESS_NOT_FOUND_ACTION_PARAM, SESS_NOT_FOUND_ACTION_CONTINUE_STR);
        lttng_live->params.sess_not_found_act = SESSION_NOT_FOUND_ACTION_CONTINUE;
    }

    component = std::move(lttng_live);

    return BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_OK;
}

BT_HIDDEN
bt_component_class_initialize_method_status
lttng_live_component_init(bt_self_component_source *self_comp_src,
                          bt_self_component_source_configuration *config, const bt_value *params,
                          __attribute__((unused)) void *init_method_data)
{
    bt_self_component *self_comp = bt_self_component_source_as_self_component(self_comp_src);
    bt_logging_level log_level =
        bt_component_get_logging_level(bt_self_component_as_component(self_comp));
    ctf::LogCfg logCfg(log_level, self_comp);

    try {
        lttng_live_component::UP lttng_live;
        bt_component_class_initialize_method_status ret =
            lttng_live_component_create(params, self_comp, logCfg, lttng_live);
        if (ret != BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_OK) {
            return ret;
        }

        bt_self_component_add_port_status add_port_status =
            bt_self_component_source_add_output_port(self_comp_src, "out", NULL, NULL);
        if (add_port_status != BT_SELF_COMPONENT_ADD_PORT_STATUS_OK) {
            return (bt_component_class_initialize_method_status) add_port_status;
        }

        bt_self_component_set_data(self_comp, lttng_live.release());

        return BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_OK;
    } catch (const std::bad_alloc&) {
        return BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR;
    } catch (const bt2_common::Error&) {
        BT_COMP_LOGE_APPEND_CAUSE(logCfg.selfComp, "Failed to initialize component");
        return BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
    }
}
