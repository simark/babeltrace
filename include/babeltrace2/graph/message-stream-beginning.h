#ifndef BABELTRACE_GRAPH_MESSAGE_STREAM_BEGINNING_H
#define BABELTRACE_GRAPH_MESSAGE_STREAM_BEGINNING_H

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

/* For bt_message, bt_self_message_iterator, bt_stream */
#include <babeltrace2/types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern
bt_message *bt_message_stream_beginning_create(
		bt_self_message_iterator *message_iterator,
		const bt_stream *stream);

extern bt_stream *bt_message_stream_beginning_borrow_stream(
		bt_message *message);

extern
void bt_message_stream_beginning_set_default_clock_snapshot(
		bt_message *message, uint64_t raw_value);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_GRAPH_MESSAGE_STREAM_BEGINNING_H */
