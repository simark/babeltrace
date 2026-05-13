/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2024 Brice Videau <bvideau@anl.gov>
 */

#define BT_LOG_TAG "LIB/PLUGIN-PROVIDER"
#include "lib/logging.h"

#include <babeltrace2/types.h>
#include <ftw.h>
#include <glib.h>
#include <pthread.h>
#include <stdint.h>

#include "common/assert.h"
#include "common/common.h"
#include "compat/compiler.h"
#include "common/object.h"
#include "lib/assert-cond.h"
#include "plugin-provider.h"
#include "so-handle/so-handle.h"

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
	return;
}

struct bt_plugin_provider_set *create_plugin_provider_set(void)
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
		(GDestroyNotify) bt_plugin_provider_put_ref);
	if (!plugin_provider_set->plugin_providers) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to allocate plugin provider set's plugin provider array.");
		goto error;
	}

	BT_LOGD("Created empty plugin provider set: addr=%p", plugin_provider_set);
	goto end;

error:
	BT_PLUGIN_PROVIDER_SET_PUT_REF_AND_RESET(plugin_provider_set);

end:
	return plugin_provider_set;
}

static
void add_plugin_provider_to_set(
		struct bt_plugin_provider_set *plugin_provider_set,
		struct bt_plugin_provider *plugin_provider)
{
	BT_ASSERT(plugin_provider_set);
	BT_ASSERT(plugin_provider);

	bt_plugin_provider_get_ref(plugin_provider);
	g_ptr_array_add(plugin_provider_set->plugin_providers, plugin_provider);
	BT_LIB_LOGD("Added plugin provider to plugin provider set: "
		"plugin-provider-set-addr=%p, %![plugin-provider-]+U",
		plugin_provider_set, plugin_provider);
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

static
void add_plugin_provider_to_set_if_not_exist(
		struct bt_plugin_provider_set *plugin_provider_set,
		struct bt_plugin_provider *plugin_provider)
{
	/* Check plugin_provider with similar name does not already exist */
	if (provider_set_contains(plugin_provider_set,
			plugin_provider->info->name->str)) {
		BT_LIB_LOGI(
			"Plugin provider with same name already exists in "
			"plugin provider set, skipping: "
			"plugin-provider-set-addr=%p, %![plugin-provider-]+U",
			plugin_provider_set, plugin_provider);
	} else {
		add_plugin_provider_to_set(plugin_provider_set, plugin_provider);
	}
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

static
struct bt_plugin_provider_info *create_plugin_provider_info(const char *name)
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
	BT_PLUGIN_PROVIDER_INFO_PUT_REF_AND_RESET(info);

end:
	return info;
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

	BT_PLUGIN_PROVIDER_INFO_PUT_REF_AND_RESET(plugin_provider->info);
	SO_HANDLE_PUT_REF_AND_RESET(plugin_provider->so_handle);
	g_free(plugin_provider);
}

static
struct bt_plugin_provider *create_plugin_provider(const char *name)
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

	plugin_provider->info = create_plugin_provider_info(name);
	if (!plugin_provider->info) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to create plugin provider info.");
		goto error;
	}

	goto end;

error:
	BT_PLUGIN_PROVIDER_PUT_REF_AND_RESET(plugin_provider);

end:
	return plugin_provider;
}

static
void set_plugin_provider_path(
		struct bt_plugin_provider *plugin_provider,
		const char *path)
{
	BT_ASSERT(plugin_provider);
	BT_ASSERT(path);
	g_string_assign(plugin_provider->info->path, path);
	plugin_provider->info->path_set = true;
	BT_LIB_LOGD("Set plugin-provider's path: %![plugin-provider-]+U, path=\"%s\"",
		plugin_provider, path);
}

static
void set_plugin_provider_description(
		struct bt_plugin_provider *plugin_provider,
		const char *description)
{
	BT_ASSERT(plugin_provider);
	BT_ASSERT(description);
	g_string_assign(plugin_provider->info->description, description);
	plugin_provider->info->description_set = true;
	BT_LIB_LOGD("Set plugin-provider's description: %![plugin-provider-]+U",
		plugin_provider);
}

static
void set_plugin_provider_author(
		struct bt_plugin_provider *plugin_provider,
		const char *author)
{
	BT_ASSERT(plugin_provider);
	BT_ASSERT(author);
	g_string_assign(plugin_provider->info->author, author);
	plugin_provider->info->author_set = true;
	BT_LIB_LOGD("Set plugin-provider's author: %![plugin-provider-]+U, author=\"%s\"",
		plugin_provider, author);
}

static
void set_plugin_provider_license(
		struct bt_plugin_provider *plugin_provider,
		const char *license)
{
	BT_ASSERT(plugin_provider);
	BT_ASSERT(license);
	g_string_assign(plugin_provider->info->license, license);
	plugin_provider->info->license_set = true;
	BT_LIB_LOGD("Set plugin provider's license: %![plugin-provider-]+U, license=\"%s\"",
		plugin_provider, license);
}

static
void set_plugin_provider_version(
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
		plugin_provider->info->version.extra_set = true;
	}

	plugin_provider->info->version_set = true;
	BT_LIB_LOGD("Set plugin provider's version: %![plugin-provider-]+U, "
		"major=%u, minor=%u, patch=%u, extra=\"%s\"",
		plugin_provider, major, minor, patch, extra);
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
		*extra = info->version.extra_set ? info->version.extra->str : NULL;
	}

end:
	return avail;
}

#define NATIVE_PLUGIN_PROVIDER_SUFFIX		"." G_MODULE_SUFFIX
#define NATIVE_PLUGIN_PROVIDER_SUFFIX_LEN	sizeof(NATIVE_PLUGIN_PROVIDER_SUFFIX)
#define LIBTOOL_PLUGIN_PROVIDER_SUFFIX		".la"
#define LIBTOOL_PLUGIN_PROVIDER_SUFFIX_LEN	sizeof(LIBTOOL_PLUGIN_PROVIDER_SUFFIX)

#define PLUGIN_PROVIDER_SUFFIX_LEN \
	bt_max_t(size_t, sizeof(NATIVE_PLUGIN_PROVIDER_SUFFIX), \
		sizeof(LIBTOOL_PLUGIN_PROVIDER_SUFFIX))

#define APPEND_ALL_FROM_DIR_NFDOPEN_MAX	8

BT_PLUGIN_PROVIDER_MODULE();

/*
 * This function does the following:
 *
 * 1. Iterate on the plugin provider descriptor attributes section and
 *    set the plugin provider's attributes depending on the attribute
 *    types. This includes the name of the plugin provider, its
 *    description, and its initialization function, for example.
 *
 * 2. Call the user's plugin provider initialization function, if any is
 *    defined.
 */
static
void initialize_so_plugin_provider(struct bt_plugin_provider *plugin_provider,
		const struct __bt_plugin_provider_descriptor *descriptor,
		struct __bt_plugin_provider_descriptor_attribute const * const *attrs_begin,
		struct __bt_plugin_provider_descriptor_attribute const * const *attrs_end)
{
	struct __bt_plugin_provider_descriptor_attribute const * const *cur_attr_ptr;

	BT_LOGI("Initializing plugin provider object from descriptors found in sections: "
		"plugin-provider-addr=%p, plugin-provider-path=\"%s\", "
		"attrs-begin-addr=%p, attrs-end-addr=%p",
		plugin_provider,
		plugin_provider->so_handle->path ?
			plugin_provider->so_handle->path->str : NULL,
		attrs_begin, attrs_end);

	/*
	 * Find and set optional attributes attached to this plugin
	 * descriptor.
	 */
	for (cur_attr_ptr = attrs_begin; cur_attr_ptr != attrs_end; cur_attr_ptr++) {
		const struct __bt_plugin_provider_descriptor_attribute *cur_attr =
			*cur_attr_ptr;

		if (!cur_attr) {
			continue;
		}

		if (cur_attr->plugin_provider_descriptor != descriptor) {
			continue;
		}

		switch (cur_attr->type) {
		case BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE_TYPE_INIT:
			plugin_provider->init = cur_attr->value.init;
			break;
		case BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE_TYPE_EXIT:
			plugin_provider->exit = cur_attr->value.exit;
			break;
		case BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE_TYPE_CREATE_ALL_FROM_FILE:
			plugin_provider->create_all_from_file = cur_attr->value.create_all_from_file;
			break;
		case BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE_TYPE_CREATE_ALL_FROM_STATIC:
			plugin_provider->create_all_from_static = cur_attr->value.create_all_from_static;
			break;
		case BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE_TYPE_AUTHOR:
			set_plugin_provider_author(plugin_provider, cur_attr->value.author);
			break;
		case BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE_TYPE_LICENSE:
			set_plugin_provider_license(plugin_provider, cur_attr->value.license);
			break;
		case BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE_TYPE_DESCRIPTION:
			set_plugin_provider_description(plugin_provider, cur_attr->value.description);
			break;
		case BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE_TYPE_VERSION:
			set_plugin_provider_version(plugin_provider,
				(unsigned int) cur_attr->value.version.major,
				(unsigned int) cur_attr->value.version.minor,
				(unsigned int) cur_attr->value.version.patch,
				cur_attr->value.version.extra);
			break;
		default:
			BT_LIB_LOGW(
				"Ignoring unknown plugin provider descriptor attribute: "
				"plugin-path=\"%s\", plugin-name=\"%s\", "
				"attr-type-name=\"%s\", attr-type-id=%d",
				plugin_provider->so_handle->path ?
					plugin_provider->so_handle->path->str :
					NULL,
				descriptor->name, cur_attr->type_name,
				cur_attr->type);

			break;
		}
	}

	/* Initialize plugin provider */
	if (plugin_provider->init) {
		enum bt_plugin_provider_initialize_func_status init_status;

		BT_LOGD_STR("Calling user's plugin provider initialization function.");
		plugin_provider->log_level = bt_lib_log_level;
		init_status = plugin_provider->init((void *) plugin_provider);
		BT_LOGD("User function returned: status=%s",
			bt_common_func_status_string(init_status));

		if (init_status < 0) {
			/*
			 * Since we don't return an error, there's no
			 * way to communicate this error to the caller.
			 */
			bt_current_thread_clear_error();
			BT_LIB_LOGW(
				"User's plugin provider initialization function failed: "
				"status=%s",
				bt_common_func_status_string(init_status));

			goto end;
		}
	}

	plugin_provider->init_called = BT_TRUE;

end:
	return;
}

static
size_t count_non_null_items_in_section(const void *begin, const void *end)
{
	size_t count = 0;
	const int * const *begin_int = (const int * const *) begin;
	const int * const *end_int = (const int * const *) end;
	const int * const *iter;

	for (iter = begin_int; iter != end_int; iter++) {
		if (*iter) {
			count++;
		}
	}

	return count;
}

static
int create_all_so_plugin_providers_from_sections(
		struct so_handle *so_handle,
		struct __bt_plugin_provider_descriptor const * const *descriptors_begin,
		struct __bt_plugin_provider_descriptor const * const *descriptors_end,
		struct __bt_plugin_provider_descriptor_attribute const * const *attrs_begin,
		struct __bt_plugin_provider_descriptor_attribute const * const *attrs_end,
		struct bt_plugin_provider_set *plugin_provider_set)
{
	int status;
	size_t descriptor_count;
	size_t attrs_count;
	size_t i;

	BT_ASSERT(so_handle);
	BT_ASSERT(plugin_provider_set);
	descriptor_count = count_non_null_items_in_section(descriptors_begin, descriptors_end);
	attrs_count = count_non_null_items_in_section(attrs_begin, attrs_end);
	BT_LOGI("Creating all SO plugin providers from sections: "
		"plugin-provider-path=\"%s\", "
		"descr-begin-addr=%p, descr-end-addr=%p, "
		"attrs-begin-addr=%p, attrs-end-addr=%p, "
		"descr-count=%zu, attrs-count=%zu",
		so_handle->path ? so_handle->path->str : NULL,
		descriptors_begin, descriptors_end,
		attrs_begin, attrs_end,
		descriptor_count, attrs_count);

	for (i = 0; i < descriptors_end - descriptors_begin; i++) {
		const struct __bt_plugin_provider_descriptor *descriptor =
			descriptors_begin[i];
		struct bt_plugin_provider *plugin_provider;

		if (!descriptor) {
			continue;
		}

		BT_LOGI("Creating plugin provider object for plugin provider: name=\"%s\"",
			descriptor->name);
		plugin_provider = create_plugin_provider(descriptor->name);
		if (!plugin_provider) {
			BT_LIB_LOGE_APPEND_CAUSE(
				"Cannot create plugin provider object: "
				"name=\"%s\", so-path=\"%s\"",
				descriptor->name,
				so_handle->path ? so_handle->path->str : NULL);
			status = BT_FUNC_STATUS_MEMORY_ERROR;
			goto end;
		}

		plugin_provider->so_handle = so_handle;
		so_handle_get_ref(plugin_provider->so_handle);

		if (so_handle->path) {
			set_plugin_provider_path(plugin_provider,
				so_handle->path->str);
		}

		initialize_so_plugin_provider(plugin_provider, descriptor,
			attrs_begin, attrs_end);

		/* Add to plugin provider set */
		add_plugin_provider_to_set_if_not_exist(
			plugin_provider_set, plugin_provider);

		BT_PLUGIN_PROVIDER_PUT_REF_AND_RESET(plugin_provider);
	}

	status = BT_FUNC_STATUS_OK;

end:
	return status;
}

static
int create_all_so_plugin_providers_from_static(
		struct bt_plugin_provider_set *plugin_provider_set)
{
	int status;
	struct so_handle *so_handle = NULL;

	BT_ASSERT(plugin_provider_set);

	status = create_so_handle(NULL, bt_lib_log_level, &so_handle);
	if (status != BT_FUNC_STATUS_OK) {
		goto end;
	}

	BT_ASSERT(so_handle);
	BT_LOGD_STR("Creating all SO plugin providers from built-in plugin providers.");
	status = create_all_so_plugin_providers_from_sections(so_handle,
		__bt_get_begin_section_plugin_provider_descriptors(),
		__bt_get_end_section_plugin_provider_descriptors(),
		__bt_get_begin_section_plugin_provider_descriptor_attributes(),
		__bt_get_end_section_plugin_provider_descriptor_attributes(),
		plugin_provider_set);

end:
	SO_HANDLE_PUT_REF_AND_RESET(so_handle);
	return status;
}

static
int create_all_so_plugin_providers_from_file(const char *path,
		struct bt_plugin_provider_set *plugin_provider_set)
{
	size_t path_len;
	int status;
	struct __bt_plugin_provider_descriptor const * const *descriptors_begin = NULL;
	struct __bt_plugin_provider_descriptor const * const *descriptors_end = NULL;
	struct __bt_plugin_provider_descriptor_attribute const * const *attrs_begin = NULL;
	struct __bt_plugin_provider_descriptor_attribute const * const *attrs_end = NULL;
	struct __bt_plugin_provider_descriptor const * const *(*get_begin_section_plugin_provider_descriptors)(void);
	struct __bt_plugin_provider_descriptor const * const *(*get_end_section_plugin_provider_descriptors)(void);
	struct __bt_plugin_provider_descriptor_attribute const * const *(*get_begin_section_plugin_provider_descriptor_attributes)(void);
	struct __bt_plugin_provider_descriptor_attribute const * const *(*get_end_section_plugin_provider_descriptor_attributes)(void);
	bt_bool is_libtool_wrapper = BT_FALSE, is_shared_object = BT_FALSE;
	struct so_handle *so_handle = NULL;

	BT_ASSERT(path);
	BT_ASSERT(plugin_provider_set);
	path_len = strlen(path);

	/*
	 * An SO plugin provider file must have a known plugin provider
	 * file suffix. So the file path must be longer than the suffix length.
	 */
	if (path_len <= PLUGIN_PROVIDER_SUFFIX_LEN) {
		BT_LOGI("Path is too short to be an `.so` or `.la` plugin provider file: "
			"path=%s, path-length=%zu, min-length=%zu",
			path, path_len, PLUGIN_PROVIDER_SUFFIX_LEN);
		status = BT_FUNC_STATUS_OK;
		goto end;
	}

	BT_LOGI("Creating all SO plugin providers from file: path=\"%s\"", path);
	path_len++;

	/*
	 * Check if the file ends with a known plugin provider file type suffix
	 * (i.e. .so or .la on Linux).
	 */
	is_libtool_wrapper = !strncmp(LIBTOOL_PLUGIN_PROVIDER_SUFFIX,
		path + path_len - LIBTOOL_PLUGIN_PROVIDER_SUFFIX_LEN,
		LIBTOOL_PLUGIN_PROVIDER_SUFFIX_LEN);
	is_shared_object = !strncmp(NATIVE_PLUGIN_PROVIDER_SUFFIX,
		path + path_len - NATIVE_PLUGIN_PROVIDER_SUFFIX_LEN,
		NATIVE_PLUGIN_PROVIDER_SUFFIX_LEN);
	if (!is_shared_object && !is_libtool_wrapper) {
		/* Name indicates this is not a plugin provider file; not an error */
		BT_LOGI("File is not an SO plugin provider file: path=\"%s\"", path);
		status = BT_FUNC_STATUS_OK;
		goto end;
	}

	status = create_so_handle(path, bt_lib_log_level, &so_handle);
	if (status != BT_FUNC_STATUS_OK) {
		goto end;
	}

	if (g_module_symbol(so_handle->module,
			"__bt_get_begin_section_plugin_provider_descriptors",
			(gpointer *) &get_begin_section_plugin_provider_descriptors)) {
		descriptors_begin = get_begin_section_plugin_provider_descriptors();
	} else {
		/*
		 * Use this first symbol to know whether or not this
		 * shared object _looks like_ a Babeltrace plugin
		 * provider. Since g_module_symbol() failed, assume that
		 * this is not a Babeltrace plugin_provider, so it's not
		 * an error.
		 */
		BT_LOGI("Cannot resolve plugin provider symbol: path=\"%s\", "
			"symbol=\"%s\"", path,
			"__bt_get_begin_section_plugin_provider_descriptors");
		status = BT_FUNC_STATUS_OK;
		goto end;
	}

	/*
	 * If g_module_symbol() fails for any of the other symbols ignore
	 * since we are not failing on load errors.
	 */
	if (g_module_symbol(so_handle->module,
			"__bt_get_end_section_plugin_provider_descriptors",
			(gpointer *) &get_end_section_plugin_provider_descriptors)) {
		descriptors_end = get_end_section_plugin_provider_descriptors();
	} else {
		BT_LIB_LOGW(
			"Cannot resolve plugin provider symbol: path=\"%s\", "
			"symbol=\"%s\"", path,
			"__bt_get_end_section_plugin_provider_descriptors");
		status = BT_FUNC_STATUS_OK;

		goto end;
	}

	if (g_module_symbol(so_handle->module,
			"__bt_get_begin_section_plugin_provider_descriptor_attributes",
			(gpointer *) &get_begin_section_plugin_provider_descriptor_attributes)) {
		 attrs_begin = get_begin_section_plugin_provider_descriptor_attributes();
	} else {
		BT_LOGI("Cannot resolve plugin provider symbol: path=\"%s\", "
			"symbol=\"%s\"", path,
			"__bt_get_begin_section_plugin_provider_descriptor_attributes");
	}

	if (g_module_symbol(so_handle->module,
			"__bt_get_end_section_plugin_provider_descriptor_attributes",
			(gpointer *) &get_end_section_plugin_provider_descriptor_attributes)) {
		attrs_end = get_end_section_plugin_provider_descriptor_attributes();
	} else {
		BT_LOGI("Cannot resolve plugin provider symbol: path=\"%s\", "
			"symbol=\"%s\"", path,
			"__bt_get_end_section_plugin_provider_descriptor_attributes");
	}

	if ((!!attrs_begin - !!attrs_end) != 0) {
		BT_LIB_LOGW(
			"Found section start or end symbol, but not both: "
			"path=\"%s\", symbol-start=\"%s\", "
			"symbol-end=\"%s\", symbol-start-addr=%p, "
			"symbol-end-addr=%p",
			path, "__bt_get_begin_section_plugin_provider_descriptor_attributes",
			"__bt_get_end_section_plugin_provider_descriptor_attributes",
			attrs_begin, attrs_end);
		status = BT_FUNC_STATUS_OK;

		goto end;
	}

	status = create_all_so_plugin_providers_from_sections(so_handle,
		descriptors_begin, descriptors_end, attrs_begin, attrs_end,
		plugin_provider_set);

end:
	SO_HANDLE_PUT_REF_AND_RESET(so_handle);
	return status;
}

static struct {
	pthread_mutex_t lock;
	struct bt_plugin_provider_set *plugin_provider_set;
	int status;
} g_append_all_from_dir_info = {
	.lock = PTHREAD_MUTEX_INITIALIZER
};

static
int nftw_append_all_from_dir(const char *file,
		const struct stat *sb __attribute__((unused)),
		int flag, struct FTW *s)
{
	int ret;
	const char *name = file + s->base;

	/* We don't recurse */
	if (s->level > 1) {
		ret = 0;
		goto end;
	}

	switch (flag) {
	case FTW_F:
	{
		if (name[0] == '.') {
			/* Skip hidden files */
			BT_LOGI("Skipping hidden file: path=\"%s\"", file);
			ret = 0;
			goto end;
		}

		g_append_all_from_dir_info.status =
			create_all_so_plugin_providers_from_file(file,
				g_append_all_from_dir_info.plugin_provider_set);
		if (g_append_all_from_dir_info.status < 0) {
			ret = -1;
			goto end;
		}

		break;
	}
	case FTW_DNR:
		/* Continue to next file / directory. */
		BT_LOGI("Cannot enter directory: continuing: path=\"%s\"", file);
		break;
	case FTW_NS:
		/* Continue to next file / directory. */
		BT_LOGI("Cannot get file information: continuing: path=\"%s\"", file);
		break;
	}

	ret = 0;

end:
	return ret;
}

static
int create_all_so_plugin_providers_from_dir_append(
		struct bt_plugin_provider_set *plugin_provider_set,
		const char *path)
{
	int nftw_flags = FTW_PHYS;
	int ret;
	int status;
	struct stat sb;

	BT_ASSERT(plugin_provider_set);
	BT_ASSERT(path);
	BT_ASSERT(strlen(path) < PATH_MAX);

	/*
	 * Make sure that path exists and is accessible.
	 * This is necessary since Cygwin implementation of nftw() is not POSIX
	 * compliant. Cygwin nftw() implementation does not fail on non-existent
	 * path with ENOENT. Instead, it flags the directory as FTW_NS. FTW_NS during
	 * nftw_append_all_from_dir is not treated as an error since we are
	 * traversing the tree for plugin_provider discovery.
	 */
	if (stat(path, &sb)) {
		BT_LOGW_ERRNO("Cannot open directory", ": path=\"%s\"",
			path);
		(void) BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_UNKNOWN(
			BT_LIB_LOG_LIBBABELTRACE2_NAME,
			"Cannot open directory: path=\"%s\"",
			path);
		status = BT_FUNC_STATUS_ERROR;
		goto end;
	}

	pthread_mutex_lock(&g_append_all_from_dir_info.lock);
	g_append_all_from_dir_info.plugin_provider_set = plugin_provider_set;
	g_append_all_from_dir_info.status = BT_FUNC_STATUS_OK;
	ret = nftw(path, nftw_append_all_from_dir,
		APPEND_ALL_FROM_DIR_NFDOPEN_MAX, nftw_flags);
	g_append_all_from_dir_info.plugin_provider_set = NULL;
	status = g_append_all_from_dir_info.status;
	pthread_mutex_unlock(&g_append_all_from_dir_info.lock);
	if (ret) {
		BT_LIB_LOGW_APPEND_CAUSE("Failed to walk directory",
			": path=\"%s\"", path);
		status = BT_FUNC_STATUS_ERROR;
		goto end;
	}

end:
	return status;
}

static
int create_all_so_plugin_providers_from_dir(const char *path,
		struct bt_plugin_provider_set *plugin_provider_set)
{
	int status;

	BT_LOGI("Creating all plugin providers in directory: path=\"%s\"",
		path);

	/*
	 * Append found plugin providers to array (never returns
	 * `BT_FUNC_STATUS_NOT_FOUND`)
	 */
	status = create_all_so_plugin_providers_from_dir_append(
		plugin_provider_set, path);
	if (status < 0) {
		/*
		 * create_all_so_plugin_providers_from_dir_append() does not
		 * fail on load errors, so this is a "real" error.
		 */
		BT_LIB_LOGE_APPEND_CAUSE(
			"Cannot append plugin_providers found in directory: "
			"path=\"%s\", status=%s",
			path, bt_common_func_status_string(status));
		goto end;
	}

	BT_LOGI("Created %u plugin providers from directory: path=\"%s\"",
		plugin_provider_set->plugin_providers->len, path);

end:
	return status;
}

static
void destroy_gstring(void *data)
{
	g_string_free(data, TRUE);
}

int find_all_plugin_providers(
		struct bt_plugin_provider_set *plugin_provider_set)
{
	char *home_plugin_provider_dir = NULL;
	const char *envvar = NULL;
	const char *disable_std_dirs_envvar = NULL;
	const char *system_plugin_provider_dir = NULL;
	GPtrArray *dirs = NULL;
	int ret;
	int status;
	uint64_t dir_i;

	BT_ASSERT(plugin_provider_set);
	BT_LOGI_STR("Finding all plugin providers in standard directories "
		"and built-in plugin providers.");

	dirs = g_ptr_array_new_with_free_func((GDestroyNotify) destroy_gstring);
	if (!dirs) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate a GPtrArray.");
		status = BT_FUNC_STATUS_MEMORY_ERROR;
		goto end;
	}

	/*
	 * Search order is:
	 *
	 * 1. `BABELTRACE_PLUGIN_PROVIDER_PATH` environment variable
	 *    (colon-separated list of directories)
	 * 2. `~/.local/lib/babeltrace2/plugin-providers`
	 * 3. Default system directory for Babeltrace plugin providers, usually
	 *    `/usr/lib/babeltrace2/plugin-providers` or
	 *    `/usr/local/lib/babeltrace2/plugin-providers` if installed locally
	 * 4. Built-in plugin providers (static)
	 *
	 * Directories are searched non-recursively.
	 *
	 * The standard directories (2. and 3.) are skipped if the
	 * `LIBBABELTRACE2_DISABLE_STD_PLUGIN_PROVIDER_DIRS` environment
	 * variable is set to `1`. This is used by the test suite to isolate
	 * the plugin provider mechanism from any plugin provider external to
	 * the build.
	 */

	/* Add directories in environment variable */
	envvar = getenv("BABELTRACE_PLUGIN_PROVIDER_PATH");

	if (envvar) {
		ret = bt_common_append_plugin_path_dirs(envvar, dirs);
		if (ret) {
			BT_LIB_LOGE_APPEND_CAUSE(
				"Failed to append plugin provider path to array of directories.");
			status = BT_FUNC_STATUS_MEMORY_ERROR;
			goto end;
		}
	}

	disable_std_dirs_envvar =
		getenv("LIBBABELTRACE2_DISABLE_STD_PLUGIN_PROVIDER_DIRS");
	if (disable_std_dirs_envvar &&
			strcmp(disable_std_dirs_envvar, "1") == 0) {
		BT_LOGI_STR("Not searching the standard plugin provider "
			"directories because the "
			"`LIBBABELTRACE2_DISABLE_STD_PLUGIN_PROVIDER_DIRS` "
			"environment variable is set to `1`.");
	} else {
		/* Add user home directory */
		home_plugin_provider_dir = bt_common_get_home_plugin_provider_path(
			BT_LOG_OUTPUT_LEVEL);
		if (home_plugin_provider_dir) {
			GString *home_plugin_provider_dir_str = g_string_new(
				home_plugin_provider_dir);

			if (!home_plugin_provider_dir_str) {
				BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate a GString.");
				status = BT_FUNC_STATUS_MEMORY_ERROR;
				goto end;
			}

			g_ptr_array_add(dirs, home_plugin_provider_dir_str);
		}

		/* Add system directory */
		system_plugin_provider_dir =
			bt_common_get_system_plugin_provider_path();

		if (system_plugin_provider_dir) {
			GString *system_plugin_provider_dir_str =
				g_string_new(system_plugin_provider_dir);

			if (!system_plugin_provider_dir_str) {
				BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate a GString.");
				status = BT_FUNC_STATUS_MEMORY_ERROR;
				goto end;
			}

			g_ptr_array_add(dirs, system_plugin_provider_dir_str);
		}
	}

	/* Load plugin providers in collected directories */
	for (dir_i = 0; dir_i < dirs->len; dir_i++) {
		GString *dir = dirs->pdata[dir_i];

		/*
		 * Skip this if the directory does not exist because
		 * find_all_plugin_providers_from_dir() would log a warning.
		 */
		if (!g_file_test(dir->str, G_FILE_TEST_IS_DIR)) {
			BT_LOGI("Skipping nonexistent directory path: "
				"path=\"%s\"", dir->str);
			continue;
		}

		/* create_all_so_plugin_providers_from_dir() logs details/errors */
		status = create_all_so_plugin_providers_from_dir(dir->str,
			plugin_provider_set);
		if (status < 0) {
			goto end;
		}
	}

	/* Search plugin providers in static */
	status = create_all_so_plugin_providers_from_static(
		plugin_provider_set);
	if (status < 0) {
		goto end;
	}

	BT_LOGI("Found %u plugin providers in standard directories and "
		"built-in plugin providers.",
		plugin_provider_set->plugin_providers->len);
	status = BT_FUNC_STATUS_OK;

end:
	free(home_plugin_provider_dir);

	if (dirs) {
		g_ptr_array_free(dirs, TRUE);
	}

	return status;
}

void bt_plugin_provider_get_ref (const struct bt_plugin_provider *provider)
{
	bt_object_get_ref(provider);
}

void bt_plugin_provider_put_ref (const struct bt_plugin_provider *provider)
{
	bt_object_put_ref(provider);
}

BT_EXPORT
void bt_plugin_provider_info_get_ref (const struct bt_plugin_provider_info *info)
{
	bt_object_get_ref(info);
}

BT_EXPORT
void bt_plugin_provider_info_put_ref (const struct bt_plugin_provider_info *info)
{
	bt_object_put_ref(info);
}

void bt_plugin_provider_set_get_ref (const struct bt_plugin_provider_set *set)
{
	bt_object_get_ref(set);
}

void bt_plugin_provider_set_put_ref (const struct bt_plugin_provider_set *set)
{
	bt_object_put_ref(set);
}
