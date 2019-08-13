/*
 * Copyright 2019 - Francis Deslauriers <francis.deslauriers@efficios.com>
 * Copyright 2016 - Philippe Proulx <pproulx@efficios.com>
 * Copyright 2010-2011 - EfficiOS Inc. and Linux Foundation
 *
 * Some functions are based on older functions written by Mathieu Desnoyers.
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

#define BT_COMP_LOG_SELF_COMP self_comp
#define BT_LOG_OUTPUT_LEVEL log_level
#define BT_LOG_TAG "PLUGIN/SRC.CTF.LTTNG-LIVE/META"
#include "logging/comp-logging.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <glib.h>
#include "compat/memstream.h"
#include <babeltrace2/babeltrace.h>

#include "metadata.h"
#include "../common/metadata/decoder.h"
#include "../common/metadata/ctf-meta-configure-ir-trace.h"

#define TSDL_MAGIC	0x75d11d57

struct packet_header {
	uint32_t magic;
	uint8_t  uuid[16];
	uint32_t checksum;
	uint32_t content_size;
	uint32_t packet_size;
	uint8_t  compression_scheme;
	uint8_t  encryption_scheme;
	uint8_t  checksum_scheme;
	uint8_t  major;
	uint8_t  minor;
} __attribute__((__packed__));


static
bool stream_classes_all_have_default_clock_class(bt_trace_class *tc,
		bt_logging_level log_level,
		bt_self_component *self_comp)
{
	uint64_t i, sc_count;
	const bt_clock_class *cc = NULL;
	const bt_stream_class *sc;
	bool ret = true;

	sc_count = bt_trace_class_get_stream_class_count(tc);
	for (i = 0; i < sc_count; i++) {
		sc = bt_trace_class_borrow_stream_class_by_index_const(tc, i);

		BT_ASSERT(sc);

		cc = bt_stream_class_borrow_default_clock_class_const(sc);
		if (!cc) {
			ret = false;
			BT_COMP_LOGE("Stream class doesn't have a default clock class: "
				"sc-id=%" PRIu64 ", sc-name=\"%s\"",
				bt_stream_class_get_id(sc),
				bt_stream_class_get_name(sc));
			goto end;
		}
	}

end:
	return ret;
}
/*
 * Iterate over the stream classes and returns the first clock class
 * encountered. This is useful to create message iterator inactivity message as
 * we don't need a particular clock class.
 */
static
const bt_clock_class *borrow_any_clock_class(bt_trace_class *tc)
{
	uint64_t i, sc_count;
	const bt_clock_class *cc = NULL;
	const bt_stream_class *sc;

	sc_count = bt_trace_class_get_stream_class_count(tc);
	for (i = 0; i < sc_count; i++) {
		sc = bt_trace_class_borrow_stream_class_by_index_const(tc, i);
		BT_ASSERT(sc);

		cc = bt_stream_class_borrow_default_clock_class_const(sc);
		if (cc) {
			goto end;
		}
	}
end:
	BT_ASSERT(cc);
	return cc;
}

BT_HIDDEN
enum lttng_live_iterator_status lttng_live_metadata_update(
		struct lttng_live_trace *trace)
{
	struct lttng_live_session *session = trace->session;
	struct lttng_live_metadata *metadata = trace->metadata;
	ssize_t ret = 0;
	size_t size, len_read = 0;
	char *metadata_buf = NULL;
	FILE *fp = NULL;
	enum ctf_metadata_decoder_status decoder_status;
	enum lttng_live_iterator_status status =
		LTTNG_LIVE_ITERATOR_STATUS_OK;
	bt_logging_level log_level = trace->log_level;
	bt_self_component *self_comp = trace->self_comp;

	/* No metadata stream yet. */
	if (!metadata) {
		if (session->new_streams_needed) {
			status = LTTNG_LIVE_ITERATOR_STATUS_AGAIN;
		} else {
			session->new_streams_needed = true;
			status = LTTNG_LIVE_ITERATOR_STATUS_CONTINUE;
		}
		goto end;
	}

	if (!metadata->trace) {
		trace->new_metadata_needed = false;
	}

	if (!trace->new_metadata_needed) {
		goto end;
	}

	/* Open for writing */
	fp = bt_open_memstream(&metadata_buf, &size);
	if (!fp) {
		BT_COMP_LOGE("Metadata open_memstream: %s", strerror(errno));
		goto error;
	}

	/* Grab all available metadata. */
	do {
		/*
		 * get_one_metadata_packet returns the number of bytes
		 * received, 0 when we have received everything, a
		 * negative value on error.
		 */
		ret = lttng_live_get_one_metadata_packet(trace, fp);
		if (ret > 0) {
			len_read += ret;
		}
	} while (ret > 0);

	/*
	 * Consider metadata closed as soon as we get an error reading
	 * it (e.g. cannot be found).
	 */
	if (ret < 0) {
		if (!metadata->closed) {
			metadata->closed = true;
			/*
			 * Release our reference on the trace as soon as
			 * we know the metadata stream is not available
			 * anymore. This won't necessarily teardown the
			 * metadata objects immediately, but only when
			 * the data streams are done.
			 */
			metadata->trace = NULL;
		}
		if (errno == EINTR) {
			if (lttng_live_graph_is_canceled(
					session->lttng_live_msg_iter)) {
				status = LTTNG_LIVE_ITERATOR_STATUS_AGAIN;
				goto end;
			}
		}
	}

	if (bt_close_memstream(&metadata_buf, &size, fp)) {
		BT_COMP_LOGE("bt_close_memstream: %s", strerror(errno));
	}
	ret = 0;
	fp = NULL;

	if (len_read == 0) {
		if (!trace->trace) {
			status = LTTNG_LIVE_ITERATOR_STATUS_AGAIN;
			goto end;
		}
		trace->new_metadata_needed = false;
		goto end;
	}

	fp = bt_fmemopen(metadata_buf, len_read, "rb");
	if (!fp) {
		BT_COMP_LOGE("Cannot memory-open metadata buffer: %s",
			strerror(errno));
		goto error;
	}

	/*
	 * The call to ctf_metadata_decoder_append_content() will append
	 * new metadata to our current trace class.
	 */
	decoder_status = ctf_metadata_decoder_append_content(
		metadata->decoder, fp);
	switch (decoder_status) {
	case CTF_METADATA_DECODER_STATUS_OK:
		if (!trace->trace_class) {
			struct ctf_trace_class *tc =
				ctf_metadata_decoder_borrow_ctf_trace_class(
					metadata->decoder);

			trace->trace_class =
				ctf_metadata_decoder_get_ir_trace_class(
						metadata->decoder);
			trace->trace = bt_trace_create(trace->trace_class);
			if (!trace->trace) {
				goto error;
			}
			if (ctf_trace_class_configure_ir_trace(tc,
					trace->trace)) {
				goto error;
			}
			if (!stream_classes_all_have_default_clock_class(
					trace->trace_class, log_level,
					self_comp)) {
				/* Error logged in function. */
				goto error;
			}
			trace->clock_class =
				borrow_any_clock_class(trace->trace_class);
		}
		trace->new_metadata_needed = false;

		break;
	case CTF_METADATA_DECODER_STATUS_INCOMPLETE:
		status = LTTNG_LIVE_ITERATOR_STATUS_AGAIN;
		break;
	case CTF_METADATA_DECODER_STATUS_ERROR:
	case CTF_METADATA_DECODER_STATUS_INVAL_VERSION:
	case CTF_METADATA_DECODER_STATUS_IR_VISITOR_ERROR:
		goto error;
	}

	goto end;
error:
	status = LTTNG_LIVE_ITERATOR_STATUS_ERROR;
end:
	if (fp) {
		int closeret;

		closeret = fclose(fp);
		if (closeret) {
			BT_COMP_LOGE("Error on fclose");
		}
	}
	free(metadata_buf);
	return status;
}

BT_HIDDEN
int lttng_live_metadata_create_stream(struct lttng_live_session *session,
		uint64_t ctf_trace_id, uint64_t stream_id,
		const char *trace_name)
{
	struct lttng_live_metadata *metadata = NULL;
	struct lttng_live_trace *trace;
	struct ctf_metadata_decoder_config cfg = {
		.log_level = session->log_level,
		.self_comp = session->self_comp,
		.clock_class_offset_s = 0,
		.clock_class_offset_ns = 0,
		.create_trace_class = true,
	};

	metadata = g_new0(struct lttng_live_metadata, 1);
	if (!metadata) {
		return -1;
	}
	metadata->log_level = session->log_level;
	metadata->self_comp = session->self_comp;
	metadata->stream_id = stream_id;

	metadata->decoder = ctf_metadata_decoder_create(&cfg);
	if (!metadata->decoder) {
		goto error;
	}
	trace = lttng_live_borrow_trace(session, ctf_trace_id);
	if (!trace) {
		goto error;
	}
	metadata->trace = trace;
	trace->metadata = metadata;
	return 0;

error:
	ctf_metadata_decoder_destroy(metadata->decoder);
	g_free(metadata);
	return -1;
}

BT_HIDDEN
void lttng_live_metadata_fini(struct lttng_live_trace *trace)
{
	struct lttng_live_metadata *metadata = trace->metadata;

	if (!metadata) {
		return;
	}
	ctf_metadata_decoder_destroy(metadata->decoder);
	trace->metadata = NULL;
	g_free(metadata);
}
