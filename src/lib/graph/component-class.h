/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#ifndef BABELTRACE_LIB_GRAPH_COMPONENT_CLASS_H
#define BABELTRACE_LIB_GRAPH_COMPONENT_CLASS_H

#include "common/list.h"
#include "common/object-struct.h"
#include <stdbool.h>
#include <glib.h>
#include <babeltrace2/babeltrace.h>

struct bt_component_class;

struct bt_component_class {
	struct bt_object base;
	enum bt_component_class_type type;
	GString *name;
	GString *description;
	GString *help;
	GString *plugin_name;

	GArray *destruction_listeners;
	bool frozen;
	bool part_of_plugin;
};

struct bt_component_class_with_iterator_class {
	struct bt_component_class parent;
	bt_message_iterator_class *msg_iter_cls;
};

struct bt_component_class_source {
	struct bt_component_class_with_iterator_class parent;
	struct {
		bt_component_class_source_get_supported_mip_versions_method get_supported_mip_versions;
		bt_component_class_source_initialize_method init;
		bt_component_class_source_finalize_method finalize;
		bt_component_class_source_query_method query;
		bt_component_class_source_output_port_connected_method output_port_connected;
	} methods;
};

struct bt_component_class_filter {
	struct bt_component_class_with_iterator_class parent;
	struct {
		bt_component_class_filter_get_supported_mip_versions_method get_supported_mip_versions;
		bt_component_class_filter_initialize_method init;
		bt_component_class_filter_finalize_method finalize;
		bt_component_class_filter_query_method query;
		bt_component_class_filter_input_port_connected_method input_port_connected;
		bt_component_class_filter_output_port_connected_method output_port_connected;
	} methods;
};

struct bt_component_class_sink {
	struct bt_component_class parent;
	struct {
		bt_component_class_sink_get_supported_mip_versions_method get_supported_mip_versions;
		bt_component_class_sink_initialize_method init;
		bt_component_class_sink_finalize_method finalize;
		bt_component_class_sink_query_method query;
		bt_component_class_sink_input_port_connected_method input_port_connected;
		bt_component_class_sink_graph_is_configured_method graph_is_configured;
		bt_component_class_sink_consume_method consume;
	} methods;
};

void _bt_component_class_freeze(
		const struct bt_component_class *component_class);

#ifdef BT_DEV_MODE
# define bt_component_class_freeze	_bt_component_class_freeze
#else
# define bt_component_class_freeze(_cc)
#endif

static inline
bool bt_component_class_has_message_iterator_class(
	struct bt_component_class *component_class)
{
	return component_class->type == BT_COMPONENT_CLASS_TYPE_SOURCE ||
		component_class->type == BT_COMPONENT_CLASS_TYPE_FILTER;
}

#endif /* BABELTRACE_LIB_GRAPH_COMPONENT_CLASS_H */
