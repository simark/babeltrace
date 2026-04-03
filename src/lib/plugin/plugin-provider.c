/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2024 Brice Videau <bvideau@anl.gov>
 */

#define BT_LOG_TAG "LIB/PLUGIN-PROVIDER"
#include "lib/logging.h"

#include <babeltrace2/types.h>
#include <glib.h>
#include <stdint.h>
#include <inttypes.h>

#include "common/assert.h"
#include "compat/compiler.h"
#include "common/func-status.h"
#include "common/object.h"
#include "lib/assert-cond.h"
#include "plugin-provider.h"

static
void destroy_plugin_provider_set(struct bt_object *obj)
{
	struct bt_plugin_provider_set *plugin_provider_set;

	if (!obj) {
		goto end;
	}

	plugin_provider_set =
		container_of(obj, struct bt_plugin_provider_set, base);

	BT_LOGD("Destroying plugin provider set: addr=%p", plugin_provider_set);

	if (plugin_provider_set->plugin_providers) {
		BT_LOGD_STR("Putting plugin providers.");
		g_ptr_array_free(plugin_provider_set->plugin_providers, TRUE);
	}

	g_free(plugin_provider_set);

end:

}

struct bt_plugin_provider_set *bt_plugin_provider_set_create(void)
{
	struct bt_plugin_provider_set *plugin_provider_set = NULL;

	BT_LOGD_STR("Creating empty plugin provider set.");
	plugin_provider_set = g_new0(struct bt_plugin_provider_set, 1);

	if (!plugin_provider_set) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to allocate one plugin provider set.");
		goto error;
	}

	bt_object_init_shared(&plugin_provider_set->base, destroy_plugin_provider_set);

	plugin_provider_set->plugin_providers = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_object_put_ref);
	if (!plugin_provider_set->plugin_providers) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to allocate plugin provider set's plugin provider array.");
		goto error;
	}

	BT_LOGD("Created empty plugin provider set: addr=%p", plugin_provider_set);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(plugin_provider_set);

end:
	return plugin_provider_set;
}

int bt_plugin_provider_set_add_plugin_provider(
		struct bt_plugin_provider_set *plugin_provider_set,
		struct bt_plugin_provider *plugin_provider)
{
	BT_ASSERT(plugin_provider_set);
	BT_ASSERT(plugin_provider);

	bt_object_get_ref(plugin_provider);
	g_ptr_array_add(plugin_provider_set->plugin_providers, plugin_provider);
	BT_LIB_LOGD("Added plugin provider to plugin provider set: "
		"plugin-provider-set-addr=%p, %![plugin-provider-]+U",
		plugin_provider_set, plugin_provider);

	return BT_FUNC_STATUS_OK;
}

static inline
bool provider_set_contains(
		struct bt_plugin_provider_set *plugin_provider_set,
		const char *name)
{
	uint64_t i;
	bool contains = false;

	BT_ASSERT(plugin_provider_set);
	BT_ASSERT(name);

	for (i = 0; i < plugin_provider_set->plugin_providers->len; i++) {
		const struct bt_plugin_provider *plugin_provider = plugin_provider_set->plugin_providers->pdata[i];

		if (strcmp(plugin_provider->info->name->str, name) == 0) {
			contains = true;
			goto end;
		}
	}

end:
	return contains;
}

int bt_plugin_provider_set_add_plugin_provider_if_not_exist(
		struct bt_plugin_provider_set *plugin_provider_set,
		struct bt_plugin_provider *plugin_provider)
{
	/* Check plugin_provider with similar name does not already exist */
	if (provider_set_contains(plugin_provider_set,plugin_provider->info->name->str)) {
		BT_LIB_LOGI(
			"Plugin provider with same name already exists in plugin provider set, skipping: "
			"plugin-provider-set-addr=%p, %![plugin-provider-]+U",
			plugin_provider_set, plugin_provider);
		return BT_FUNC_STATUS_OK;
	} else {
		/* Add to plugin_provider set */
		return bt_plugin_provider_set_add_plugin_provider(plugin_provider_set, plugin_provider);
	}
}

BT_EXPORT
bt_bool bt_plugin_provider_create_all_from_file_options_get_fail_on_load_error(
		const struct bt_plugin_provider_create_all_from_file_options *options)
{
	BT_ASSERT_PRE_PLUGIN_PROVIDER_OPTIONS_NON_NULL(options);
	return options->base.fail_on_load_error;
}

BT_EXPORT
bt_bool bt_plugin_provider_create_all_from_static_options_get_fail_on_load_error(
		const struct bt_plugin_provider_create_all_from_static_options *options)
{
	BT_ASSERT_PRE_PLUGIN_PROVIDER_OPTIONS_NON_NULL(options);
	return options->base.fail_on_load_error;
}

static
void destroy_plugin_provider(struct bt_object *obj)
{
	struct bt_plugin_provider *plugin_provider;

	BT_ASSERT(obj);
	plugin_provider = container_of(obj, struct bt_plugin_provider, base);
	BT_LIB_LOGI("Destroying plugin provider object: %!+U", plugin_provider);

	if (plugin_provider->init_called && plugin_provider->exit) {
		BT_LOGD_STR("Calling exit function.");
		plugin_provider->log_level = bt_lib_log_level;
		plugin_provider->exit((void*)plugin_provider);
		BT_LOGD_STR("Exit function returned.");
	}

	BT_OBJECT_PUT_REF_AND_RESET(plugin_provider->info);
	BT_OBJECT_PUT_REF_AND_RESET(plugin_provider->so_handle);
	g_free(plugin_provider);
}

struct bt_plugin_provider *bt_plugin_provider_create(const char *name)
{
	struct bt_plugin_provider *plugin_provider = NULL;

	BT_ASSERT(name);
	BT_LOGD("Creating empty plugin provider object: name=\"%s\"", name);

	plugin_provider = g_new0(struct bt_plugin_provider, 1);
	if (!plugin_provider) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate one plugin provider.");
		goto error;
	}

	bt_object_init_shared(&plugin_provider->base, destroy_plugin_provider);

	/* FIXME: the call below could fail*/
	plugin_provider->info = bt_plugin_provider_info_create(name);

	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(plugin_provider);

end:
	return plugin_provider;
}

int bt_plugin_provider_set_path(
		struct bt_plugin_provider *plugin_provider,
		const char *path)
{
	BT_ASSERT(plugin_provider);
	BT_ASSERT(path);
	g_string_assign(plugin_provider->info->path, path);
	plugin_provider->info->path_set = true;
	BT_LIB_LOGD("Set plugin-provider's path: %![plugin-provider-]+U, path=\"%s\"",
		plugin_provider, path);
	return BT_FUNC_STATUS_OK;
}

int bt_plugin_provider_set_description(
		struct bt_plugin_provider *plugin_provider,
		const char *description)
{
	BT_ASSERT(plugin_provider);
	BT_ASSERT(description);
	g_string_assign(plugin_provider->info->description, description);
	plugin_provider->info->description_set = true;
	BT_LIB_LOGD("Set plugin-provider's description: %![plugin-provider-]+U",
		plugin_provider);
	return BT_FUNC_STATUS_OK;
}

int bt_plugin_provider_set_author(
		struct bt_plugin_provider *plugin_provider,
		const char *author)
{
	BT_ASSERT(plugin_provider);
	BT_ASSERT(author);
	g_string_assign(plugin_provider->info->author, author);
	plugin_provider->info->author_set = true;
	BT_LIB_LOGD("Set plugin-provider's author: %![plugin-provider-]+U, author=\"%s\"",
		plugin_provider, author);
	return BT_FUNC_STATUS_OK;
}

int bt_plugin_provider_set_license(
		struct bt_plugin_provider *plugin_provider,
		const char *license)
{
	BT_ASSERT(plugin_provider);
	BT_ASSERT(license);
	g_string_assign(plugin_provider->info->license, license);
	plugin_provider->info->license_set = true;
	BT_LIB_LOGD("Set plugin provider's license: %![plugin-provider-]+U, license=\"%s\"",
		plugin_provider, license);
	return BT_FUNC_STATUS_OK;
}

int bt_plugin_provider_set_version(
		struct bt_plugin_provider *plugin_provider,
		unsigned int major, unsigned int minor,
		unsigned int patch, const char *extra)
{
	BT_ASSERT(plugin_provider);
	plugin_provider->info->version.major = major;
	plugin_provider->info->version.minor = minor;
	plugin_provider->info->version.patch = patch;

	if (extra) {
		g_string_assign(plugin_provider->info->version.extra, extra);
	}

	plugin_provider->info->version_set = true;
	BT_LIB_LOGD("Set plugin provider's version: %![plugin-provider-]+U, "
		"major=%u, minor=%u, patch=%u, extra=\"%s\"",
		plugin_provider, major, minor, patch, extra);
	return BT_FUNC_STATUS_OK;
}

BT_EXPORT
void *bt_self_plugin_provider_get_data(
		const struct bt_self_plugin_provider *self_plugin_provider)
{
	struct bt_plugin_provider *plugin_provider =
		(void *) self_plugin_provider;

	BT_ASSERT_PRE_PLUGIN_PROVIDER_NON_NULL(plugin_provider);
	return plugin_provider->user_data;
}

BT_EXPORT
void bt_self_plugin_provider_set_data(
		struct bt_self_plugin_provider *self_plugin_provider,
		void *data)
{
	struct bt_plugin_provider *plugin_provider =
		(void *) self_plugin_provider;

	BT_ASSERT_PRE_PLUGIN_PROVIDER_NON_NULL(plugin_provider);
	plugin_provider->user_data = data;
	BT_LIB_LOGD("Set plugin provider's user data: %!+U",
		plugin_provider);
}

BT_EXPORT
int bt_self_plugin_provider_get_logging_level(
		const bt_self_plugin_provider *self_plugin_provider)
{
	struct bt_plugin_provider *plugin_provider =
		(void *) self_plugin_provider;

	return plugin_provider->log_level;
}

static
void destroy_plugin_provider_info(struct bt_object *obj)
{
	struct bt_plugin_provider_info *info;

	BT_ASSERT(obj);
	info = container_of(obj, struct bt_plugin_provider_info, base);
	BT_LIB_LOGI("Destroying plugin provider info object: %!+N", info);

	if (info->name) {
		g_string_free(info->name, TRUE);
		info->name = NULL;
	}

	if (info->path) {
		g_string_free(info->path, TRUE);
		info->path = NULL;
	}

	if (info->description) {
		g_string_free(info->description, TRUE);
		info->description = NULL;
	}

	if (info->author) {
		g_string_free(info->author, TRUE);
		info->author = NULL;
	}

	if (info->license) {
		g_string_free(info->license, TRUE);
		info->license = NULL;
	}

	if (info->version.extra) {
		g_string_free(info->version.extra, TRUE);
		info->version.extra = NULL;
	}

	g_free(info);
}

struct bt_plugin_provider_info *bt_plugin_provider_info_create(const char *name)
{
	struct bt_plugin_provider_info *info = NULL;

	BT_ASSERT(name);
	BT_LOGD("Creating empty plugin provider info object.");

	info = g_new0(struct bt_plugin_provider_info, 1);
	if (!info) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate one plugin provider info.");
		goto error;
	}

	bt_object_init_shared(&info->base, destroy_plugin_provider_info);

	info->name = g_string_new(name);
	if (!info->name) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate a GString.");
		goto error;
	}

	/* Create empty info */
	info->path = g_string_new(NULL);
	if (!info->path) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate a GString.");
		goto error;
	}

	info->description = g_string_new(NULL);
	if (!info->description) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate a GString.");
		goto error;
	}

	info->author = g_string_new(NULL);
	if (!info->author) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate a GString.");
		goto error;
	}

	info->license = g_string_new(NULL);
	if (!info->license) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate a GString.");
		goto error;
	}

	info->version.extra = g_string_new(NULL);
	if (!info->version.extra) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate a GString.");
		goto error;
	}

	BT_LIB_LOGD("Created empty plugin provider info object: %!+N", info);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(info);

end:
	return info;
}

BT_EXPORT
const char *bt_plugin_provider_info_get_name(
		const struct bt_plugin_provider_info *info)
{
	BT_ASSERT_PRE_PLUGIN_PROVIDER_INFO_NON_NULL(info);
	return info->name->str;
}

BT_EXPORT
const char *bt_plugin_provider_info_get_description(
		const struct bt_plugin_provider_info *info)
{
	BT_ASSERT_PRE_PLUGIN_PROVIDER_INFO_NON_NULL(info);
	return info->description_set ? info->description->str : NULL;
}

BT_EXPORT
const char *bt_plugin_provider_info_get_author(
		const struct bt_plugin_provider_info *info)
{
	BT_ASSERT_PRE_PLUGIN_PROVIDER_INFO_NON_NULL(info);
	return info->author_set ? info->author->str : NULL;
}

BT_EXPORT
const char *bt_plugin_provider_info_get_license(
		const struct bt_plugin_provider_info *info)
{
	BT_ASSERT_PRE_PLUGIN_PROVIDER_INFO_NON_NULL(info);
	return info->license_set ? info->license->str : NULL;
}

BT_EXPORT
const char *bt_plugin_provider_info_get_path(
		const struct bt_plugin_provider_info *info)
{
	BT_ASSERT_PRE_PLUGIN_PROVIDER_INFO_NON_NULL(info);
	return info->path_set ? info->path->str : NULL;
}

BT_EXPORT
enum bt_property_availability
bt_plugin_provider_info_get_version(
		const struct bt_plugin_provider_info *info,
		unsigned int *major, unsigned int *minor,
		unsigned int *patch, const char **extra)
{
	enum bt_property_availability avail =
		BT_PROPERTY_AVAILABILITY_AVAILABLE;

	BT_ASSERT_PRE_PLUGIN_PROVIDER_INFO_NON_NULL(info);

	if (!info->version_set) {
		BT_LIB_LOGD("Plugin provider info's version is not set: %!+N", info);
		avail = BT_PROPERTY_AVAILABILITY_NOT_AVAILABLE;
		goto end;
	}

	if (major) {
		*major = info->version.major;
	}

	if (minor) {
		*minor = info->version.minor;
	}

	if (patch) {
		*patch = info->version.patch;
	}

	if (extra) {
		*extra = info->version.extra->str;
	}

end:
	return avail;
}

const bt_plugin_provider_info *bt_plugin_provider_borrow_info_const(
		const struct bt_plugin_provider *plugin_provider)
{
	BT_ASSERT(plugin_provider);
	return plugin_provider->info;
}

uint64_t bt_plugin_provider_set_get_plugin_provider_count(
		const struct bt_plugin_provider_set *plugin_provider_set)
{
	BT_ASSERT(plugin_provider_set);
	return (uint64_t) plugin_provider_set->plugin_providers->len;
}

const struct bt_plugin_provider *
bt_plugin_provider_set_borrow_plugin_provider_by_index_const(
		const struct bt_plugin_provider_set *plugin_provider_set,
		uint64_t index)
{
	BT_ASSERT(plugin_provider_set);
	BT_ASSERT(index < plugin_provider_set->plugin_providers->len);
	return g_ptr_array_index(plugin_provider_set->plugin_providers, index);
}
