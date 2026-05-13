/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2024 Brice Videau <bvideau@anl.gov>
 */

#ifndef BABELTRACE_LIB_PLUGIN_PLUGIN_PROVIDER_H
#define BABELTRACE_LIB_PLUGIN_PLUGIN_PROVIDER_H

#include <babeltrace2/babeltrace.h>
#include <glib.h>
#include <stdbool.h>

#include "common/object-struct.h"

#define BT_PLUGIN_PROVIDER_PUT_REF_AND_RESET(_provider)	\
	do {						\
		bt_plugin_provider_put_ref(_provider);	\
		(_provider) = NULL;			\
	} while (0)

#define BT_PLUGIN_PROVIDER_SET_PUT_REF_AND_RESET(_set)	\
	do {						\
		bt_plugin_provider_set_put_ref(_set);	\
		(_set) = NULL;				\
	} while (0)

struct bt_plugin_provider_create_all_from_base_options {
	bool fail_on_load_error;
};

struct bt_plugin_provider_create_all_from_static_options {
	struct bt_plugin_provider_create_all_from_base_options base;
};

struct bt_plugin_provider_create_all_from_file_options {
	struct bt_plugin_provider_create_all_from_base_options base;
};

struct bt_plugin_provider_info {
	struct bt_object base;

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
};

struct bt_plugin_provider {
	struct bt_object base;

	struct bt_plugin_provider_info *info;
	bt_plugin_provider_initialize_func init;
	bt_plugin_provider_finalize_func exit;
	bt_plugin_provider_create_all_from_file_func create_all_from_file;
	bt_plugin_provider_create_all_from_static_func create_all_from_static;

	/* User-defined data */
	void *user_data;
	bt_bool init_called;

	int log_level;

	/* Shared lib. handle: owned by this */
	struct so_handle *so_handle;
};

struct bt_plugin_provider_set {
	struct bt_object base;

	/* Array of struct bt_plugin_provider * */
	GPtrArray *plugin_providers;
};

struct bt_plugin_provider_set *create_plugin_provider_set(void);

void bt_plugin_provider_get_ref (const struct bt_plugin_provider *provider);
void bt_plugin_provider_put_ref (const struct bt_plugin_provider *provider);

void bt_plugin_provider_set_get_ref (const struct bt_plugin_provider_set *set);
void bt_plugin_provider_set_put_ref (const struct bt_plugin_provider_set *set);

int find_all_plugin_providers(
		struct bt_plugin_provider_set *plugin_provider_set);

#endif /* BABELTRACE_LIB_PLUGIN_PLUGIN_PROVIDER_H */
