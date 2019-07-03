#ifndef BABELTRACE2_GRAPH_MESSAGE_STREAM_CONST_H
#define BABELTRACE2_GRAPH_MESSAGE_STREAM_CONST_H

/*
 * Copyright 2019 Simon Marchi <simon.marchi@efficios.com>
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

#ifdef __cplusplus
extern "C" {
#endif

enum bt_message_stream_clock_snapshot_state {
	BT_MESSAGE_STREAM_CLOCK_SNAPSHOT_STATE_UNKNOWN = 0,
	BT_MESSAGE_STREAM_CLOCK_SNAPSHOT_STATE_KNOWN,
};

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_GRAPH_MESSAGE_STREAM_CONST_H */
