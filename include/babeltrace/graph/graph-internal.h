#ifndef BABELTRACE_GRAPH_GRAPH_INTERNAL_H
#define BABELTRACE_GRAPH_GRAPH_INTERNAL_H

/*
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2017 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace/graph/graph.h>
#include <babeltrace/graph/connection-internal.h>
#include <babeltrace/graph/message-const.h>
#include <babeltrace/graph/component-internal.h>
#include <babeltrace/graph/component-sink-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/object-pool-internal.h>
#include <babeltrace/assert-internal.h>
#include <stdlib.h>
#include <glib.h>

struct bt_component;
struct bt_port;

enum bt_graph_configuration_state {
	BT_GRAPH_CONFIGURATION_STATE_CONFIGURING,
	BT_GRAPH_CONFIGURATION_STATE_PARTIALLY_CONFIGURED,
	BT_GRAPH_CONFIGURATION_STATE_CONFIGURED,
	BT_GRAPH_CONFIGURATION_STATE_FAULTY,
};

struct bt_graph {
	/**
	 * A component graph contains components and point-to-point connection
	 * between these components.
	 *
	 * In terms of ownership:
	 * 1) The graph is the components' parent,
	 * 2) The graph is the connnections' parent,
	 * 3) Components share the ownership of their connections,
	 * 4) A connection holds weak references to its two component endpoints.
	 */
	struct bt_object base;

	/* Array of pointers to bt_connection. */
	GPtrArray *connections;
	/* Array of pointers to bt_component. */
	GPtrArray *components;
	/* Queue of pointers (weak references) to sink bt_components. */
	GQueue *sinks_to_consume;

	bool canceled;
	bool in_remove_listener;
	bool has_sink;

	/*
	 * If this is false, then the public API's consuming
	 * functions (bt_graph_consume() and bt_graph_run()) return
	 * BT_GRAPH_STATUS_CANNOT_CONSUME. The internal "no check"
	 * functions always work.
	 *
	 * In bt_port_output_message_iterator_create(), on success,
	 * this flag is cleared so that the iterator remains the only
	 * consumer for the graph's lifetime.
	 */
	bool can_consume;

	enum bt_graph_configuration_state config_state;

	struct {
		GArray *source_output_port_added;
		GArray *filter_output_port_added;
		GArray *filter_input_port_added;
		GArray *sink_input_port_added;
		GArray *source_filter_ports_connected;
		GArray *source_sink_ports_connected;
		GArray *filter_filter_ports_connected;
		GArray *filter_sink_ports_connected;
	} listeners;

	/* Pool of `struct bt_message_event *` */
	struct bt_object_pool event_msg_pool;

	/* Pool of `struct bt_message_packet_beginning *` */
	struct bt_object_pool packet_begin_msg_pool;

	/* Pool of `struct bt_message_packet_end *` */
	struct bt_object_pool packet_end_msg_pool;

	/*
	 * Array of `struct bt_message *` (weak).
	 *
	 * This is an array of all the messages ever created from
	 * this graph. Some of them can be in one of the pools above,
	 * some of them can be at large. Because each message has a
	 * weak pointer to the graph containing its pool, we need to
	 * notify each message that the graph is gone on graph
	 * destruction.
	 *
	 * TODO: When we support a maximum size for object pools,
	 * add a way for a message to remove itself from this
	 * array (on destruction).
	 */
	GPtrArray *messages;
};

static inline
void _bt_graph_set_can_consume(struct bt_graph *graph, bool can_consume)
{
	BT_ASSERT(graph);
	graph->can_consume = can_consume;
}

#ifdef BT_DEV_MODE
# define bt_graph_set_can_consume	_bt_graph_set_can_consume
#else
# define bt_graph_set_can_consume(_graph, _can_consume)
#endif

BT_HIDDEN
enum bt_graph_status bt_graph_consume_sink_no_check(struct bt_graph *graph,
		struct bt_component_sink *sink);

BT_HIDDEN
void bt_graph_notify_port_added(struct bt_graph *graph, struct bt_port *port);

BT_HIDDEN
void bt_graph_notify_ports_connected(struct bt_graph *graph,
		struct bt_port *upstream_port, struct bt_port *downstream_port);

BT_HIDDEN
void bt_graph_remove_connection(struct bt_graph *graph,
		struct bt_connection *connection);

/*
 * This only works with a component which is not connected at this
 * point.
 *
 * Also the reference count of `component` should be 0 when you call
 * this function, which means only `graph` owns the component, so it
 * is safe to destroy.
 */
BT_HIDDEN
int bt_graph_remove_unconnected_component(struct bt_graph *graph,
		struct bt_component *component);

BT_HIDDEN
void bt_graph_add_message(struct bt_graph *graph,
		struct bt_message *msg);

static inline
const char *bt_graph_status_string(enum bt_graph_status status)
{
	switch (status) {
	case BT_GRAPH_STATUS_CANCELED:
		return "BT_GRAPH_STATUS_CANCELED";
	case BT_GRAPH_STATUS_AGAIN:
		return "BT_GRAPH_STATUS_AGAIN";
	case BT_GRAPH_STATUS_END:
		return "BT_GRAPH_STATUS_END";
	case BT_GRAPH_STATUS_OK:
		return "BT_GRAPH_STATUS_OK";
	case BT_GRAPH_STATUS_ERROR:
		return "BT_GRAPH_STATUS_ERROR";
	case BT_GRAPH_STATUS_COMPONENT_REFUSES_PORT_CONNECTION:
		return "BT_GRAPH_STATUS_COMPONENT_REFUSES_PORT_CONNECTION";
	case BT_GRAPH_STATUS_NOMEM:
		return "BT_GRAPH_STATUS_NOMEM";
	default:
		return "(unknown)";
	}
}

static inline
const char *bt_graph_configuration_state_string(
		enum bt_graph_configuration_state state)
{
	switch (state) {
	case BT_GRAPH_CONFIGURATION_STATE_CONFIGURING:
		return "BT_GRAPH_CONFIGURATION_STATE_CONFIGURING";
	case BT_GRAPH_CONFIGURATION_STATE_PARTIALLY_CONFIGURED:
		return "BT_GRAPH_CONFIGURATION_STATE_PARTIALLY_CONFIGURED";
	case BT_GRAPH_CONFIGURATION_STATE_CONFIGURED:
		return "BT_GRAPH_CONFIGURATION_STATE_CONFIGURED";
	default:
		return "(unknown)";
	}
}

static inline
enum bt_graph_status bt_graph_configure(struct bt_graph *graph)
{
	enum bt_graph_status status = BT_GRAPH_STATUS_OK;
	uint64_t i;

	BT_ASSERT(graph->config_state != BT_GRAPH_CONFIGURATION_STATE_FAULTY);

	if (likely(graph->config_state ==
			BT_GRAPH_CONFIGURATION_STATE_CONFIGURED)) {
		goto end;
	}

#ifdef BT_ASSERT_PRE
	BT_ASSERT_PRE(graph->has_sink, "Graph has no sink component: %!+g", graph);
#endif

	graph->config_state = BT_GRAPH_CONFIGURATION_STATE_PARTIALLY_CONFIGURED;

	for (i = 0; i < graph->components->len; i++) {
		struct bt_component *comp = graph->components->pdata[i];
		struct bt_component_sink *comp_sink = (void *) comp;
		struct bt_component_class_sink *comp_cls_sink =
			(void *) comp->class;

		if (comp->class->type != BT_COMPONENT_CLASS_TYPE_SINK) {
			continue;
		}

		if (comp_sink->graph_is_configured_method_called) {
			continue;
		}

		if (comp_cls_sink->methods.graph_is_configured) {
			enum bt_self_component_status comp_status;

#ifdef BT_LIB_LOGD
			BT_LIB_LOGD("Calling user's \"graph is configured\" method: "
				"%![graph-]+g, %![comp-]+c",
				graph, comp);
#endif

			comp_status = comp_cls_sink->methods.graph_is_configured(
				(void *) comp_sink);

#ifdef BT_LIB_LOGD
			BT_LIB_LOGD("User method returned: status=%s",
				bt_self_component_status_string(comp_status));
#endif

#ifdef BT_ASSERT_PRE
			BT_ASSERT_PRE(comp_status == BT_SELF_COMPONENT_STATUS_OK ||
				comp_status == BT_SELF_COMPONENT_STATUS_ERROR ||
				comp_status == BT_SELF_COMPONENT_STATUS_NOMEM,
				"Unexpected returned status: status=%s",
				bt_self_component_status_string(comp_status));
#endif

			if (comp_status != BT_SELF_COMPONENT_STATUS_OK) {
				status = BT_GRAPH_STATUS_ERROR;
#ifdef BT_LIB_LOGW
				BT_LIB_LOGW("User's \"graph is configured\" method failed: "
					"%![comp-]+c, status=%s",
					comp,
					bt_self_component_status_string(
						comp_status));
#endif

				goto end;
			}
		}

		comp_sink->graph_is_configured_method_called = true;
	}

	graph->config_state = BT_GRAPH_CONFIGURATION_STATE_CONFIGURED;

end:
	return status;
}

#endif /* BABELTRACE_GRAPH_GRAPH_INTERNAL_H */
