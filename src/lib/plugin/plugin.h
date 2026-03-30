/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#ifndef BABELTRACE_LIB_PLUGIN_PLUGIN_H
#define BABELTRACE_LIB_PLUGIN_PLUGIN_H

#include <glib.h>
#include <stdbool.h>

#include "common/object-struct.h"

struct bt_plugin {
	struct bt_object base;

	/* Arrays of `struct bt_component_class *` (owned by this) */
	GPtrArray *src_comp_classes;
	GPtrArray *flt_comp_classes;
	GPtrArray *sink_comp_classes;

	/* Info (owned by this) */
	struct {
		GString *path;
		GString *name;
		GString *author;
		GString *license;
		GString *description;
		struct {
			unsigned int major;
			unsigned int minor;
			unsigned int patch;
			GString *extra;
			bool extra_set;
		} version;
		bool path_set;
		bool author_set;
		bool license_set;
		bool description_set;
		bool version_set;
	} info;

	GArray *destruction_listeners;
	bool frozen;
};

struct bt_plugin_set {
	struct bt_object base;

	/* Array of struct bt_plugin * */
	GPtrArray *plugins;
};

#endif /* BABELTRACE_LIB_PLUGIN_PLUGIN_H */
