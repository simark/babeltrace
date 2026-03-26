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
#include <babeltrace2/babeltrace.h>

#include "common/object.h"
#include "compat/compiler.h"

/* Protection: this file uses BT_LIB_LOG*() macros directly */
#ifndef BT_LIB_LOG_SUPPORTED
# error Please include "lib/logging.h" before including this file.
#endif

enum bt_plugin_type {
	BT_PLUGIN_TYPE_SO = 0,
	BT_PLUGIN_TYPE_PYTHON = 1,
	BT_PLUGIN_TYPE_EXTERNAL = 2,
};

struct bt_plugin {
	struct bt_object base;
	enum bt_plugin_type type;

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
	/* Value depends on the specific plugin type */
	void *spec_data;
	void (*destroy_spec_data)(struct bt_plugin *);
};

struct bt_plugin_set {
	struct bt_object base;

	/* Array of struct bt_plugin * */
	GPtrArray *plugins;
};

static inline
const char *bt_plugin_type_string(enum bt_plugin_type type)
{
	switch (type) {
	case BT_PLUGIN_TYPE_SO:
		return "SO";
	case BT_PLUGIN_TYPE_PYTHON:
		return "PYTHON";
	case BT_PLUGIN_TYPE_EXTERNAL:
		return "EXTERNAL";
	default:
		return "(unknown)";
	}
}

static inline
enum bt_plugin_set_add_plugin_status
add_plugin_to_set_if_not_exists(
		struct bt_plugin_set *plugin_set,
		struct bt_plugin *plugin)
{
	enum bt_plugin_set_add_plugin_status status;

	if (bt_plugin_set_borrow_plugin_by_name_const(
			plugin_set, plugin->info.name->str)) {
		BT_LIB_LOGI(
			"Plugin with same name already exists in plugin set, skipping: "
			"plugin-set-addr=%p, %![plugin-]+l",
			plugin_set, plugin);
		status = BT_PLUGIN_SET_ADD_PLUGIN_STATUS_OK;
		goto end;
	}

	status = bt_plugin_set_add_plugin(plugin_set, plugin);

end:
	return status;
}

#endif /* BABELTRACE_LIB_PLUGIN_PLUGIN_H */
