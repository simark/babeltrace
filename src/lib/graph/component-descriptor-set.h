/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2017 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#ifndef BABELTRACE_LIB_GRAPH_COMPONENT_DESCRIPTOR_SET_H
#define BABELTRACE_LIB_GRAPH_COMPONENT_DESCRIPTOR_SET_H

#include <glib.h>
#include "lib/object-struct.h"

/*
 * This structure describes an eventual component instance.
 */
struct bt_component_descriptor_set_entry {
	/* Owned by this */
	struct bt_component_class *comp_cls;

	/* Owned by this */
	struct bt_value *params;

	void *init_method_data;
};

struct bt_component_descriptor_set {
	struct bt_object base;

	/* Array of `struct bt_component_descriptor_set_entry *` */
	GPtrArray *sources;

	/* Array of `struct bt_component_descriptor_set_entry *` */
	GPtrArray *filters;

	/* Array of `struct bt_component_descriptor_set_entry *` */
	GPtrArray *sinks;
};

#endif /* BABELTRACE_LIB_GRAPH_COMPONENT_DESCRIPTOR_SET_H */
