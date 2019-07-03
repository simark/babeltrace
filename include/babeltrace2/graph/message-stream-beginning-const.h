#ifndef BABELTRACE_GRAPH_MESSAGE_STREAM_BEGINNING_CONST_H
#define BABELTRACE_GRAPH_MESSAGE_STREAM_BEGINNING_CONST_H

/*
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
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

/* For bt_message, bt_clock_snapshot, bt_stream */
#include <babeltrace2/types.h>

/* For bt_message_stream_clock_snapshot_state */
#include <babeltrace2/graph/message-stream-const.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const bt_stream *bt_message_stream_beginning_borrow_stream_const(
		const bt_message *message);

extern enum bt_message_stream_clock_snapshot_state
bt_message_stream_beginning_borrow_default_clock_snapshot_const(
		const bt_message *message, const bt_clock_snapshot **snapshot);

extern const bt_clock_class *
bt_message_stream_beginning_borrow_stream_class_default_clock_class_const(
		const bt_message *msg);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_GRAPH_MESSAGE_STREAM_BEGINNING_CONST_H */
