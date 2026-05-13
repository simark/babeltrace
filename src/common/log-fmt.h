/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2026 Efficios Inc.
 */

#ifndef BABELTRACE_COMMON_LOG_FMT_H
#define BABELTRACE_COMMON_LOG_FMT_H

#include <babeltrace2/plugin/plugin-loading.h>

#define BT_PLUGIN_FMT				\
	"plugin-name=\"%s\", plugin-path=\"%s\""
#define BT_PLUGIN_ARGS(plugin)			\
	bt_plugin_get_name(plugin),		\
	bt_plugin_get_path(plugin) ? bt_plugin_get_path(plugin) : "(none)"

#endif /* BABELTRACE_COMMON_LOG_FMT_H */
