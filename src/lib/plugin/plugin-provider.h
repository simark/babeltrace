/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2024 Brice Videau <bvideau@anl.gov>
 */

#ifndef BABELTRACE_PLUGIN_PLUGIN_PROVIDER_INTERNAL_H
#define BABELTRACE_PLUGIN_PLUGIN_PROVIDER_INTERNAL_H

#include <babeltrace2/babeltrace.h>
#include <glib.h>
#include <stdbool.h>

#include "common/object-struct.h"


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

struct bt_plugin_provider_set *bt_plugin_provider_set_create(void);

int bt_plugin_provider_set_add_plugin_provider(
		struct bt_plugin_provider_set *plugin_provider_set,
		struct bt_plugin_provider *plugin_provider);

int bt_plugin_provider_set_add_plugin_provider_if_not_exist(
		struct bt_plugin_provider_set *plugin_provider_set,
		struct bt_plugin_provider *plugin_provider);

struct bt_plugin_provider_info *bt_plugin_provider_info_create(const char *name);

struct bt_plugin_provider *bt_plugin_provider_create(const char *name);

int bt_plugin_provider_set_path(
		struct bt_plugin_provider *plugin_provider,
		const char *path);

int bt_plugin_provider_set_description(
		struct bt_plugin_provider *plugin_provider,
		const char *description);

int bt_plugin_provider_set_author(
		struct bt_plugin_provider *plugin_provider,
		const char *author);

int bt_plugin_provider_set_license(
		struct bt_plugin_provider *plugin_provider,
		const char *license);

int bt_plugin_provider_set_version(
		struct bt_plugin_provider *plugin_provider,
		unsigned int major, unsigned int minor,
		unsigned int patch, const char *extra);

const struct bt_plugin_provider_info *bt_plugin_provider_borrow_info_const(
		const struct bt_plugin_provider *plugin_provider);

uint64_t bt_plugin_provider_set_get_plugin_provider_count(
		const struct bt_plugin_provider_set *plugin_provider_set);

const struct bt_plugin_provider *
bt_plugin_provider_set_borrow_plugin_provider_by_index_const(
		const struct bt_plugin_provider_set *plugin_provider_set,
		uint64_t index);

int bt_plugin_provider_find_all(
		const struct bt_plugin_provider_set **plugin_provider_set_out);

#endif /* BABELTRACE_PLUGIN_PLUGIN_PROVIDER_INTERNAL_H */
