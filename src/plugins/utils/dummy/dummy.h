#ifndef BABELTRACE_PLUGINS_UTILS_DUMMY_H
#define BABELTRACE_PLUGINS_UTILS_DUMMY_H

/*
 * Copyright 2017 Philippe Proulx <pproulx@efficios.com>
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

#include <glib.h>
#include <babeltrace2/babeltrace.h>
#include "common/macros.h"

struct dummy {
	bt_self_component_port_input_message_iterator *msg_iter;
};

BT_HIDDEN
bt_component_class_initialize_method_status dummy_init(
		bt_self_component_sink *component,
		bt_self_component_sink_configuration *config,
		const bt_value *params, void *init_method_data);

BT_HIDDEN
void dummy_finalize(bt_self_component_sink *component);

BT_HIDDEN
bt_component_class_sink_graph_is_configured_method_status dummy_graph_is_configured(
		bt_self_component_sink *comp);

BT_HIDDEN
bt_component_class_sink_consume_method_status dummy_consume(
		bt_self_component_sink *component);

#endif /* BABELTRACE_PLUGINS_UTILS_DUMMY_H */
