/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#ifndef BABELTRACE_LIB_PLUGIN_PLUGIN_SO_H
#define BABELTRACE_LIB_PLUGIN_PLUGIN_SO_H

#include <stdbool.h>
#include <babeltrace2/babeltrace.h>

int bt_plugin_so_create_all_from_file(const char *path,
		bool fail_on_load_error, struct bt_plugin_set *plugin_set,
		int log_level);

int bt_plugin_so_create_all_from_static(bool fail_on_load_error,
		struct bt_plugin_set *plugin_set, int log_level);

#endif /* BABELTRACE_LIB_PLUGIN_PLUGIN_SO_H */
