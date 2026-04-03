/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2024 Brice Videau <bvideau@anl.gov>
 */

#define BT_LOG_TAG "LIB/PLUGIN-PROVIDER-SO"
#include "lib/logging.h"

#include "common/assert.h"
#include "lib/assert-cond.h"
#include "compat/compiler.h"
#include <babeltrace2/plugin/plugin-provider-dev.h>
#include <babeltrace2/types.h>
#include "common/list.h"
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <glib.h>
#include <gmodule.h>
#include <sys/stat.h>
#include <ftw.h>
#include <pthread.h>

#include "plugin-provider.h"
#include "plugin.h"
#include "common/func-status.h"
#include "common/common.h"
#include "common/so-handle.h"

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
int bt_plugin_provider_so_init(struct bt_plugin_provider *plugin_provider,
		const struct __bt_plugin_provider_descriptor *descriptor,
		struct __bt_plugin_provider_descriptor_attribute const * const *attrs_begin,
		struct __bt_plugin_provider_descriptor_attribute const * const *attrs_end)
{
	int status = BT_FUNC_STATUS_OK;
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
			status = bt_plugin_provider_set_author(plugin_provider, cur_attr->value.author);
			if (status) {
				BT_LIB_LOGE_APPEND_CAUSE(
					"Cannot set plugin provider author: %!+U", plugin_provider);
				goto end;
			}
			break;
		case BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE_TYPE_LICENSE:
			status = bt_plugin_provider_set_license(plugin_provider, cur_attr->value.license);
			if (status) {
				BT_LIB_LOGE_APPEND_CAUSE(
					"Cannot set plugin provider license: %!+U", plugin_provider);
				goto end;
			}
			break;
		case BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE_TYPE_DESCRIPTION:
			status = bt_plugin_provider_set_description(plugin_provider, cur_attr->value.description);
			if (status) {
				BT_LIB_LOGE_APPEND_CAUSE(
					"Cannot set plugin provider description: %!+U", plugin_provider);
				goto end;
			}
			break;
		case BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE_TYPE_VERSION:
			status = bt_plugin_provider_set_version(plugin_provider,
				(unsigned int) cur_attr->value.version.major,
				(unsigned int) cur_attr->value.version.minor,
				(unsigned int) cur_attr->value.version.patch,
				cur_attr->value.version.extra);
			if (status) {
				BT_LIB_LOGE_APPEND_CAUSE(
					"Cannot set plugin provider version: %!+U", plugin_provider);
				goto end;
			}
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
			status = BT_FUNC_STATUS_NOT_FOUND;

			goto end;
		}
	}

	plugin_provider->init_called = BT_TRUE;

end:
	return status;
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
int bt_plugin_provider_so_create_all_from_sections(
		struct so_handle *so_handle,
		struct __bt_plugin_provider_descriptor const * const *descriptors_begin,
		struct __bt_plugin_provider_descriptor const * const *descriptors_end,
		struct __bt_plugin_provider_descriptor_attribute const * const *attrs_begin,
		struct __bt_plugin_provider_descriptor_attribute const * const *attrs_end,
		struct bt_plugin_provider_set **plugin_provider_set_out)
{
	int status = BT_FUNC_STATUS_OK;
	size_t descriptor_count;
	size_t attrs_count;
	size_t i;

	BT_ASSERT(so_handle);
	BT_ASSERT(plugin_provider_set_out);
	*plugin_provider_set_out = NULL;
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
	*plugin_provider_set_out = bt_plugin_provider_set_create();
	if (!*plugin_provider_set_out) {
		BT_LIB_LOGE_APPEND_CAUSE("Cannot create empty plugin provider set.");
		status = BT_FUNC_STATUS_MEMORY_ERROR;
		goto error;
	}

	for (i = 0; i < descriptors_end - descriptors_begin; i++) {
		const struct __bt_plugin_provider_descriptor *descriptor =
			descriptors_begin[i];
		struct bt_plugin_provider *plugin_provider;

		if (!descriptor) {
			continue;
		}

		BT_LOGI("Creating plugin provider object for plugin provider: name=\"%s\"",
			descriptor->name);
		plugin_provider = bt_plugin_provider_create(descriptor->name);
		if (!plugin_provider) {
			BT_LIB_LOGE_APPEND_CAUSE("Cannot create plugin provider object.");
			status = BT_FUNC_STATUS_MEMORY_ERROR;
			goto error;
		}

		plugin_provider->so_handle = so_handle;
		bt_object_get_ref_no_null_check(plugin_provider->so_handle);

		if (so_handle->path) {
			status = bt_plugin_provider_set_path(plugin_provider,
				so_handle->path->str);
			if (status != BT_FUNC_STATUS_OK) {
				BT_LIB_LOGE_APPEND_CAUSE(
					"Cannot set plugin provider path: %!+U", plugin_provider);
				goto error;
			}
		}

		status = bt_plugin_provider_so_init(plugin_provider, descriptor,
			attrs_begin, attrs_end);
		if (status == BT_FUNC_STATUS_OK) {
			/* Add to plugin provider set */
			status = bt_plugin_provider_set_add_plugin_provider_if_not_exist(*plugin_provider_set_out, plugin_provider);
			BT_OBJECT_PUT_REF_AND_RESET(plugin_provider);
			if (status != BT_FUNC_STATUS_OK) {
				BT_LIB_LOGE_APPEND_CAUSE(
					"Cannot add plugin provider to plugin provider set: "
					"plugin-provider-set-addr=%p, %![plugin-provider-]+U",
					*plugin_provider_set_out, plugin_provider);
				goto error;
			}
		} else if (status == BT_FUNC_STATUS_NOT_FOUND) {
			/*
			 * There was an error initializing the plugin, but
			 * we don't fail in this case.
			 */
			BT_OBJECT_PUT_REF_AND_RESET(plugin_provider);
		} else if (status < 0) {
			/*
			 * bt_plugin_provider_so_init() doesn't fail on
			 * load errors, so this is a "real" error.
			 */
			BT_LIB_LOGW_APPEND_CAUSE(
				"Cannot initialize SO plugin_provider object from sections.");
			BT_OBJECT_PUT_REF_AND_RESET(plugin_provider);
			goto error;
		}

		BT_ASSERT(!plugin_provider);
	}

	BT_ASSERT(*plugin_provider_set_out);

	if ((*plugin_provider_set_out)->plugin_providers->len == 0) {
		BT_OBJECT_PUT_REF_AND_RESET(*plugin_provider_set_out);
		status = BT_FUNC_STATUS_NOT_FOUND;
	}

	goto end;

error:
	BT_ASSERT(status < 0);
	BT_OBJECT_PUT_REF_AND_RESET(*plugin_provider_set_out);

end:
	return status;
}

static
int bt_plugin_provider_so_create_all_from_static(
		struct bt_plugin_provider_set **plugin_provider_set_out)
{
	int status;
	struct so_handle *so_handle = NULL;

	BT_ASSERT(plugin_provider_set_out);
	*plugin_provider_set_out = NULL;
	status = create_so_handle(NULL, bt_lib_log_level, &so_handle);
	if (status != BT_FUNC_STATUS_OK) {
		BT_ASSERT(!so_handle);
		goto end;
	}

	BT_ASSERT(so_handle);
	BT_LOGD_STR("Creating all SO plugin providers from built-in plugin providers.");
	status = bt_plugin_provider_so_create_all_from_sections(so_handle,
		__bt_get_begin_section_plugin_provider_descriptors(),
		__bt_get_end_section_plugin_provider_descriptors(),
		__bt_get_begin_section_plugin_provider_descriptor_attributes(),
		__bt_get_end_section_plugin_provider_descriptor_attributes(),
		plugin_provider_set_out);
	BT_ASSERT((status == BT_FUNC_STATUS_OK && *plugin_provider_set_out &&
		(*plugin_provider_set_out)->plugin_providers->len > 0) || !*plugin_provider_set_out);

end:
	BT_OBJECT_PUT_REF_AND_RESET(so_handle);
	return status;
}

static
int bt_plugin_provider_so_create_all_from_file(const char *path,
		struct bt_plugin_provider_set **plugin_provider_set_out)
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
	BT_ASSERT(plugin_provider_set_out);
	*plugin_provider_set_out = NULL;
	path_len = strlen(path);

	/*
	 * An SO plugin provider file must have a known plugin provider
	 * file suffix. So the file path must be longer than the suffix length.
	 */
	if (path_len <= PLUGIN_PROVIDER_SUFFIX_LEN) {
		BT_LOGI("Path is too short to be an `.so` or `.la` plugin provider file:"
			"path=%s, path-length=%zu, min-length=%zu",
			path, path_len, PLUGIN_PROVIDER_SUFFIX_LEN);
		status = BT_FUNC_STATUS_NOT_FOUND;
		goto end;
	}

	BT_LOGI("Trying to create all SO plugin providers from file: path=\"%s\"", path);
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
		status = BT_FUNC_STATUS_NOT_FOUND;
		goto end;
	}

	status = create_so_handle(path, bt_lib_log_level, &so_handle);
	if (status != BT_FUNC_STATUS_OK) {
		/* create_so_handle() logs more details */
		BT_ASSERT(!so_handle);
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
		status = BT_FUNC_STATUS_NOT_FOUND;
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
		status = BT_FUNC_STATUS_NOT_FOUND;

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
		status = BT_FUNC_STATUS_NOT_FOUND;

		goto end;
	}

	/* Initialize plugin provider */
	BT_LOGD_STR("Initializing plugin provider object.");
	status = bt_plugin_provider_so_create_all_from_sections(so_handle,
		descriptors_begin, descriptors_end, attrs_begin, attrs_end,
		plugin_provider_set_out);

end:
	BT_OBJECT_PUT_REF_AND_RESET(so_handle);
	return status;
}

static
int bt_plugin_provider_find_all_from_static(
		const struct bt_plugin_provider_set **plugin_provider_set_out)
{
	BT_ASSERT_PRE_NO_ERROR();

	/* bt_plugin_provider_so_create_all_from_static() logs errors */
	return bt_plugin_provider_so_create_all_from_static(
		(void *) plugin_provider_set_out);
}

static
int bt_plugin_provider_find_all_from_file(const char *path,
		const struct bt_plugin_provider_set **plugin_provider_set_out)
{
	int status;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT(path);
	BT_ASSERT(plugin_provider_set_out);
	BT_LOGI("Creating plugin providers from file: path=\"%s\"", path);

	/* Try shared object plugin_providers */
	status = bt_plugin_provider_so_create_all_from_file(path,
		(void *) plugin_provider_set_out);
	if (status == BT_FUNC_STATUS_OK) {
		BT_ASSERT(*plugin_provider_set_out);
		BT_ASSERT((*plugin_provider_set_out)->plugin_providers->len > 0);
		goto end;
	} else if (status < 0) {
		BT_ASSERT(!*plugin_provider_set_out);
		goto end;
	}

	BT_ASSERT(status == BT_FUNC_STATUS_NOT_FOUND);
	BT_ASSERT(!*plugin_provider_set_out);

end:
	if (status == BT_FUNC_STATUS_OK) {
		BT_LOGI("Created %u plugin providers from file: "
			"path=\"%s\", count=%u, plugin-provider-set-addr=%p",
			(*plugin_provider_set_out)->plugin_providers->len, path,
			(*plugin_provider_set_out)->plugin_providers->len,
			*plugin_provider_set_out);
	} else if (status == BT_FUNC_STATUS_NOT_FOUND) {
		BT_LOGI("Found no plugin providers in file: path=\"%s\"", path);
	}

	return status;
}

static struct {
	pthread_mutex_t lock;
	struct bt_plugin_provider_set *plugin_provider_set;
	int status;
} append_all_from_dir_info = {
	.lock = PTHREAD_MUTEX_INITIALIZER
};

static
int nftw_append_all_from_dir(const char *file,
		const struct stat *sb __attribute__((unused)),
		int flag, struct FTW *s)
{
	int ret = 0;
	const char *name = file + s->base;

	/* We don't recurse */
	if (s->level > 1) {
		goto end;
	}

	switch (flag) {
	case FTW_F:
	{
		const struct bt_plugin_provider_set *plugin_providers_from_file = NULL;

		if (name[0] == '.') {
			/* Skip hidden files */
			BT_LOGI("Skipping hidden file: path=\"%s\"", file);
			goto end;
		}

		append_all_from_dir_info.status =
			bt_plugin_provider_find_all_from_file(file,
				&plugin_providers_from_file);
		if (append_all_from_dir_info.status == BT_FUNC_STATUS_OK) {
			size_t j;

			BT_ASSERT(plugin_providers_from_file);

			for (j = 0; j < plugin_providers_from_file->plugin_providers->len; j++) {
				struct bt_plugin_provider *plugin_provider =
					g_ptr_array_index(plugin_providers_from_file->plugin_providers, j);

				BT_LIB_LOGI("Adding plugin provider to plugin provider set: "
					"plugin-provider-path=\"%s\", %![plugin-provider-]+U",
					file, plugin_provider);
				append_all_from_dir_info.status =
					bt_plugin_provider_set_add_plugin_provider_if_not_exist(
					append_all_from_dir_info.plugin_provider_set,
					plugin_provider);
				if (append_all_from_dir_info.status != BT_FUNC_STATUS_OK) {
					bt_object_put_ref(plugin_providers_from_file);
					BT_LIB_LOGE_APPEND_CAUSE(
						"Cannot add plugin provider to plugin provider set.");
					ret = -1;
					goto end;
				}
			}

			bt_object_put_ref(plugin_providers_from_file);
			goto end;
		} else if (append_all_from_dir_info.status < 0) {
			/* bt_plugin_provider_find_all_from_file() logs errors */
			BT_ASSERT(!plugin_providers_from_file);
			ret = -1;
			goto end;
		}

		/*
		 * Not found in this file: this is no an error; continue
		 * walking the directories.
		 */
		BT_ASSERT(!plugin_providers_from_file);
		BT_ASSERT(append_all_from_dir_info.status ==
			BT_FUNC_STATUS_NOT_FOUND);
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

end:
	return ret;
}

static
int bt_plugin_provider_create_append_all_from_dir(struct bt_plugin_provider_set *plugin_provider_set,
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
		BT_LOGW_ERRNO("Cannot open directory",
			": path=\"%s\"",
			path);
		(void) BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_UNKNOWN(
			BT_LIB_LOG_LIBBABELTRACE2_NAME,
			"Cannot open directory: path=\"%s\"",
			path);
		status = BT_FUNC_STATUS_ERROR;
		goto end;
	}

	pthread_mutex_lock(&append_all_from_dir_info.lock);
	append_all_from_dir_info.plugin_provider_set = plugin_provider_set;
	append_all_from_dir_info.status = BT_FUNC_STATUS_OK;
	ret = nftw(path, nftw_append_all_from_dir,
		APPEND_ALL_FROM_DIR_NFDOPEN_MAX, nftw_flags);
	append_all_from_dir_info.plugin_provider_set = NULL;
	status = append_all_from_dir_info.status;
	pthread_mutex_unlock(&append_all_from_dir_info.lock);
	if (ret) {
		BT_LIB_LOGW_APPEND_CAUSE("Failed to walk directory",
			": path=\"%s\"",
			path);
		status = BT_FUNC_STATUS_ERROR;
		goto end;
	}

	if (status == BT_FUNC_STATUS_NOT_FOUND) {
		/*
		 * We're just appending in this function; even if
		 * nothing was found, it's still okay from the caller's
		 * perspective.
		 */
		status = BT_FUNC_STATUS_OK;
	}

end:
	return status;
}

static
int bt_plugin_provider_find_all_from_dir(const char *path,
		const struct bt_plugin_provider_set **plugin_provider_set_out)
{
	int status = BT_FUNC_STATUS_OK;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT(plugin_provider_set_out);
	BT_LOGI("Creating all plugin providers in directory: path=\"%s\"",
		path);
	*plugin_provider_set_out = bt_plugin_provider_set_create();
	if (!*plugin_provider_set_out) {
		BT_LIB_LOGE_APPEND_CAUSE("Cannot create empty plugin provider set.");
		status = BT_FUNC_STATUS_MEMORY_ERROR;
		goto error;
	}

	/*
	 * Append found plugin providers to array (never returns
	 * `BT_FUNC_STATUS_NOT_FOUND`)
	 */
	status = bt_plugin_provider_create_append_all_from_dir((void *) *plugin_provider_set_out,
		path);
	if (status < 0) {
		/*
		 * bt_plugin_provider_create_append_all_from_dir() does not
		 * fail on load errors, so this is a "real" error.
		 */
		BT_LIB_LOGE_APPEND_CAUSE(
			"Cannot append plugin_providers found in directory: "
			"path=\"%s\", status=%s",
			path, bt_common_func_status_string(status));
		goto error;
	}

	BT_ASSERT(status == BT_FUNC_STATUS_OK);

	if ((*plugin_provider_set_out)->plugin_providers->len == 0) {
		/* Nothing was appended: not found */
		BT_LOGI("No plugin providers found in directory: path=\"%s\"", path);
		status = BT_FUNC_STATUS_NOT_FOUND;
		goto error;
	}

	BT_LOGI("Created %u plugin providers from directory: count=%u, path=\"%s\"",
		(*plugin_provider_set_out)->plugin_providers->len,
		(*plugin_provider_set_out)->plugin_providers->len, path);
	goto end;

error:
	BT_ASSERT(status != BT_FUNC_STATUS_OK);
	BT_OBJECT_PUT_REF_AND_RESET(*plugin_provider_set_out);

end:
	return status;
}

static
void destroy_gstring(void *data)
{
	g_string_free(data, TRUE);
}

int bt_plugin_provider_find_all(
		const struct bt_plugin_provider_set **plugin_provider_set_out)
{
	char *home_plugin_provider_dir = NULL;
	const char *envvar = NULL;
	const char *system_plugin_provider_dir = NULL;
	const struct bt_plugin_provider_set *plugin_provider_set = NULL;
	GPtrArray *dirs = NULL;
	int ret;
	int status = BT_FUNC_STATUS_OK;
	uint64_t dir_i, plugin_provider_i;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT(plugin_provider_set_out);
	BT_LOGI_STR("Finding all plugin providers in standard directories "
		"and built-in plugin providers.");
	dirs = g_ptr_array_new_with_free_func((GDestroyNotify) destroy_gstring);
	if (!dirs) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate a GPtrArray.");
		status = BT_FUNC_STATUS_MEMORY_ERROR;
		goto end;
	}

	*plugin_provider_set_out = bt_plugin_provider_set_create();
	if (!*plugin_provider_set_out) {
		BT_LIB_LOGE_APPEND_CAUSE("Cannot create empty plugin provider set.");
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

	/* Load plugin providers in collected directories */
	for (dir_i = 0; dir_i < dirs->len; dir_i++) {
		GString *dir = dirs->pdata[dir_i];

		BT_OBJECT_PUT_REF_AND_RESET(plugin_provider_set);

		/*
		 * Skip this if the directory does not exist because
		 * bt_plugin_provider_find_all_from_dir() would log a warning.
		 */
		if (!g_file_test(dir->str, G_FILE_TEST_IS_DIR)) {
			BT_LOGI("Skipping nonexistent directory path: "
				"path=\"%s\"", dir->str);
			continue;
		}

		/* bt_plugin_provider_find_all_from_dir() logs details/errors */
		status = bt_plugin_provider_find_all_from_dir(dir->str,
			&plugin_provider_set);
		if (status < 0) {
			BT_ASSERT(!plugin_provider_set);
			goto end;
		} else if (status == BT_FUNC_STATUS_NOT_FOUND) {
			BT_ASSERT(!plugin_provider_set);
			BT_LOGI("No plugin providers found in directory: path=\"%s\"",
				dir->str);
			continue;
		}

		BT_ASSERT(status == BT_FUNC_STATUS_OK);
		BT_ASSERT(plugin_provider_set);
		BT_LOGI("Found plugin providers in directory: path=\"%s\", count=%u",
			dir->str, plugin_provider_set->plugin_providers->len);

		for (plugin_provider_i = 0; plugin_provider_i < plugin_provider_set->plugin_providers->len;
				plugin_provider_i++) {
			status = bt_plugin_provider_set_add_plugin_provider_if_not_exist(
				(void *) *plugin_provider_set_out,
				plugin_provider_set->plugin_providers->pdata[plugin_provider_i]);
			if (status != BT_FUNC_STATUS_OK) {
				BT_LIB_LOGE_APPEND_CAUSE(
					"Cannot add plugin provider to plugin provider set.");
				goto end;
			}
		}
	}

	/* Search plugin providers in static */
	BT_OBJECT_PUT_REF_AND_RESET(plugin_provider_set);
	status = bt_plugin_provider_find_all_from_static(&plugin_provider_set);
	if (status < 0) {
		BT_ASSERT(!plugin_provider_set);
		goto end;
	} else if (status == BT_FUNC_STATUS_NOT_FOUND) {
		BT_ASSERT(!plugin_provider_set);
		BT_LOGI_STR("No plugin providers found in built-in plugin providers.");
		goto end;
	}

	BT_ASSERT(status == BT_FUNC_STATUS_OK);
	BT_ASSERT(plugin_provider_set);
	BT_LOGI("Found built-in plugin providers: count=%u",
		plugin_provider_set->plugin_providers->len);

	for (plugin_provider_i = 0; plugin_provider_i < plugin_provider_set->plugin_providers->len;
			plugin_provider_i++) {
		status = bt_plugin_provider_set_add_plugin_provider_if_not_exist(
			(void *) *plugin_provider_set_out,
			plugin_provider_set->plugin_providers->pdata[plugin_provider_i]);
		if (status != BT_FUNC_STATUS_OK) {
			BT_LIB_LOGE_APPEND_CAUSE(
				"Cannot add plugin provider to plugin provider set.");
			goto end;
		}
	}

end:
	free(home_plugin_provider_dir);
	bt_object_put_ref(plugin_provider_set);

	if (dirs) {
		g_ptr_array_free(dirs, TRUE);
	}

	if (status < 0) {
		BT_OBJECT_PUT_REF_AND_RESET(*plugin_provider_set_out);
	} else {
		BT_ASSERT(*plugin_provider_set_out);

		if ((*plugin_provider_set_out)->plugin_providers->len > 0) {
			BT_LOGI("Found plugin providers in standard directories and built-in plugin providers: "
				"count=%u", (*plugin_provider_set_out)->plugin_providers->len);
			status = BT_FUNC_STATUS_OK;
		} else {
			BT_LOGI_STR("No plugin providers found in standard directories and built-in plugin providers.");
			status = BT_FUNC_STATUS_NOT_FOUND;
			BT_OBJECT_PUT_REF_AND_RESET(*plugin_provider_set_out);
		}
	}

	return status;
}


