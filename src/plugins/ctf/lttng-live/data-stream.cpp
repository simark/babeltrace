/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2019 Francis Deslauriers <francis.deslauriers@efficios.com>
 * Copyright 2016 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2010-2011 EfficiOS Inc. and Linux Foundation
 */

#define BT_COMP_LOG_SELF_COMP logCfg.selfComp
#define BT_LOG_OUTPUT_LEVEL   logCfg.logLevel
#define BT_LOG_TAG            "PLUGIN/SRC.CTF.LTTNG-LIVE/DS"
#include "logging/comp-logging.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib.h>

#include <babeltrace2/babeltrace.h>

#include "../common/src/msg-iter/msg-iter.hpp"
#include "common/assert.h"
#include "compat/mman.h"
#include "data-stream.hpp"
#include "cpp-common/exc.hpp"
#include "cpp-common/comp-logging.hpp"

#define STREAM_NAME_PREFIX "stream-"

namespace ctf {
namespace src {
namespace live {

Buf CtfLiveMedium::buf(bt2_common::DataLen offset, bt2_common::DataLen minSize)
{
    if (_mLiveStreamIter.has_stream_hung_up)
        throw NoData {};

    uint64_t recvLen;

    lttng_live_get_stream_bytes_status status =
        lttng_live_get_stream_bytes(_mLiveStreamIter.trace->session->lttng_live_msg_iter,
                                    &_mLiveStreamIter, _mLiveStreamIter.buf.data(),
                                    _mLiveStreamIter.offset, _mLiveStreamIter.buf.size(), &recvLen);
    switch (status) {
    case LTTNG_LIVE_GET_STREAM_BYTES_STATUS_OK:
        break;

    case LTTNG_LIVE_GET_STREAM_BYTES_STATUS_AGAIN:
        throw bt2_common::TryAgain();

    case LTTNG_LIVE_GET_STREAM_BYTES_STATUS_EOF:
        throw NoData();

    case LTTNG_LIVE_GET_STREAM_BYTES_STATUS_ERROR:
        throw bt2_common::Error();
    }

    return Buf {_mLiveStreamIter.buf.data(),
                bt2_common::DataLen::fromBytes(_mLiveStreamIter.buf.size())};
}

} /* namespace live */
} /* namespace src */
} /* namespace ctf */

BT_HIDDEN
enum lttng_live_iterator_status lttng_live_lazy_msg_init(struct lttng_live_session *session,
                                                         bt_self_message_iterator *self_msg_iter)
{
    const ctf::LogCfg& logCfg = session->logCfg;

    if (!session->lazy_stream_msg_init) {
        return LTTNG_LIVE_ITERATOR_STATUS_OK;
    }

    BT_COMP_LOGD("Lazily initializing self message iterator for live session: "
                 "session-id=%" PRIu64 ", self-msg-iter-addr=%p",
                 session->id, self_msg_iter);

    for (lttng_live_trace::UP& trace : session->traces) {
        for (lttng_live_stream_iterator::UP& streamIter : trace->streamIterators) {
            if (streamIter->msgIter) {
                continue;
            }

            ctf::src::TraceCls *ctfTraceCls = trace->metadata->irGenerator.ctfTraceCls();
            BT_COMP_LOGD("Creating CTF message iterator: "
                         "session-id=%" PRIu64 ", ctf-tc-addr=%p, "
                         "stream-iter-name=%s, self-msg-iter-addr=%p",
                         session->id, ctfTraceCls, streamIter->name.c_str(), self_msg_iter);
        }
    }

    session->lazy_stream_msg_init = false;

    return LTTNG_LIVE_ITERATOR_STATUS_OK;
}

BT_HIDDEN
struct lttng_live_stream_iterator *
lttng_live_stream_iterator_create(struct lttng_live_session *session, uint64_t ctf_trace_id,
                                  uint64_t stream_id, bt_self_message_iterator *self_msg_iter)
{
    struct lttng_live_component *lttng_live;
    struct lttng_live_trace *trace;

    BT_ASSERT(session);
    BT_ASSERT(session->lttng_live_msg_iter);
    BT_ASSERT(session->lttng_live_msg_iter->lttng_live_comp);

    const ctf::LogCfg& logCfg = session->logCfg;

    lttng_live = session->lttng_live_msg_iter->lttng_live_comp;

    lttng_live_stream_iterator::UP streamIter =
        bt2_common::makeUnique<lttng_live_stream_iterator>(logCfg);

    trace = lttng_live_session_borrow_or_create_trace_by_id(session, ctf_trace_id);
    if (!trace) {
        BT_COMP_LOGE_APPEND_CAUSE(logCfg.selfComp, "Failed to borrow CTF trace.");
        return nullptr;
    }

    streamIter->trace = trace;
    streamIter->state = LTTNG_LIVE_STREAM_ACTIVE_NO_DATA;
    streamIter->viewer_stream_id = stream_id;

    streamIter->ctf_stream_class_id.is_set = false;
    streamIter->ctf_stream_class_id.value = UINT64_MAX;

    streamIter->last_inactivity_ts.is_set = false;
    streamIter->last_inactivity_ts.value = 0;

    streamIter->buf.resize(lttng_live->max_query_size);

    std::stringstream ss;
    ss << STREAM_NAME_PREFIX << streamIter->viewer_stream_id;
    streamIter->name = ss.str();

    lttng_live_stream_iterator *ret = streamIter.get();
    trace->streamIterators.emplace_back(std::move(streamIter));

    /* Track the number of active stream iterator. */
    session->lttng_live_msg_iter->active_stream_iter++;

    return ret;
}

void lttng_live_stream_iterator_set_stream_class(lttng_live_stream_iterator *streamIter,
                                                 uint64_t ctfStreamClsId)
{
    if (streamIter->ctf_stream_class_id.is_set) {
        BT_ASSERT(streamIter->ctf_stream_class_id.value == ctfStreamClsId);
        return;
    } else {
        streamIter->ctf_stream_class_id.value = ctfStreamClsId;
        streamIter->ctf_stream_class_id.is_set = true;
    }
}

lttng_live_stream_iterator::~lttng_live_stream_iterator()
{
    /* Track the number of active stream iterator. */
    this->trace->session->lttng_live_msg_iter->active_stream_iter--;
}
