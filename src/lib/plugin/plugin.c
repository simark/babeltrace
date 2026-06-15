/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#define BT_LOG_TAG "LIB/PLUGIN"
#include "lib/logging.h"

#include "common/assert.h"
#include "lib/assert-cond.h"
#include "common/macros.h"
#include "compat/compiler.h"
#include "compat/limits.h"
#include "common/common.h"
#include "common/object.h"
#include "lib/graph/component-class.h"
#include <glib.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/stat.h>
#include <ftw.h>
#include <pthread.h>
#include <gmodule.h>

#include "plugin.h"
#include "plugin-provider.h"
#include "common/func-status.h"

struct bt_plugin_destruction_listener_elem {
	bt_plugin_destruction_listener_func func;
	void *data;
};

#define BT_ASSERT_PRE_DEV_PLUGIN_HOT(_p)				\
	BT_ASSERT_PRE_DEV_HOT("plugin",					\
		((const struct bt_plugin *) (_p)),			\
		"Plugin", ": %!+l", (_p))

#ifdef BT_DEV_MODE
# define bt_plugin_freeze _bt_plugin_freeze

static
void _bt_plugin_freeze(struct bt_plugin *plugin)
{
	BT_ASSERT(plugin);
	BT_LIB_LOGD("Freezing plugin: %!+l", plugin);
	plugin->frozen = true;
}

#else
# define bt_plugin_freeze(_p)
#endif

#define DESTRUCTION_LISTENER_FUNC_NAME  "bt_plugin_destruction_listener_func"

#define APPEND_ALL_FROM_DIR_NFDOPEN_MAX	8

/*
 * Make the libbabeltrace2 library able to contain shared object
 * plugins.
 */
BT_PLUGIN_MODULE();

static struct {
	/* Value passed to g_once_init_enter() */
	gsize init;

	/*
	 * Cached return value of try_init_plugin_providers() returned on
	 * subsequent call.
	 */
	int init_status;

	struct bt_plugin_provider_set *set;
} g_plugin_providers = { 0 };

/*
 * Loads the plugin providers the first time this is called and caches the
 * result (including any error status) for subsequent calls.
 */
static
int try_init_plugin_providers(void) {
	if (g_once_init_enter(&g_plugin_providers.init)) {
		struct bt_plugin_provider_set *plugin_provider_set;
		int status;

		plugin_provider_set = create_plugin_provider_set();
		if (!plugin_provider_set) {
			BT_LIB_LOGE_APPEND_CAUSE(
				"Cannot create empty plugin provider set.");
			status = BT_FUNC_STATUS_MEMORY_ERROR;
			goto end;
		}

		BT_LOGI_STR("Loading plugin providers modules.");
		status = find_all_plugin_providers(plugin_provider_set);

		if (status != BT_FUNC_STATUS_OK) {
			BT_LIB_LOGE_APPEND_CAUSE(
				"Failed to load plugin provider modules.");
			goto end;
		}

		/* On success, move the reference to the global. */
		g_plugin_providers.set = plugin_provider_set;
		plugin_provider_set = NULL;

end:
		BT_PLUGIN_PROVIDER_SET_PUT_REF_AND_RESET(plugin_provider_set);
		g_plugin_providers.init_status = status;
		g_once_init_leave(&g_plugin_providers.init, 1);
	}

	return g_plugin_providers.init_status;
}

BT_EXPORT
uint64_t bt_get_plugin_provider_count(void)
{
	int status = try_init_plugin_providers();
	int count;

	if (status < 0) {
		count = 0;
		goto end;
	}

	count = g_plugin_providers.set->plugin_providers->len;

end:
	return count;
}

BT_EXPORT
const bt_plugin_provider_info *
bt_borrow_plugin_provider_info_by_index(uint64_t index)
{
	int status = try_init_plugin_providers();
	const struct bt_plugin_provider_info *info;
	const struct bt_plugin_provider *provider;

	if (status < 0) {
		info = NULL;
		goto end;
	}

	BT_ASSERT_PRE_DEV_VALID_INDEX(index,
		g_plugin_providers.set->plugin_providers->len);

	provider = g_ptr_array_index(g_plugin_providers.set->plugin_providers,
			index);
	info = provider->info;

end:
	return info;
}

__attribute__((destructor)) static
void fini_plugin_provider_set(void) {
	BT_LOGI_STR("Unloading plugin provider modules.");
	BT_PLUGIN_PROVIDER_SET_PUT_REF_AND_RESET(g_plugin_providers.set);
	g_plugin_providers.init = 0;
	g_plugin_providers.init_status = 0;
}

static
void destroy_plugin_set(struct bt_object *obj)
{
	struct bt_plugin_set *plugin_set;

	BT_ASSERT(obj);
	plugin_set = container_of(obj, struct bt_plugin_set, base);
	BT_LOGD("Destroying plugin set: addr=%p", plugin_set);

	if (plugin_set->plugins) {
		BT_LOGD_STR("Putting plugins.");
		g_ptr_array_free(plugin_set->plugins, TRUE);
	}

	g_free(plugin_set);
}

static
struct bt_plugin_set *create_plugin_set(void)
{
	struct bt_plugin_set *plugin_set = NULL;

	BT_LOGD_STR("Creating empty plugin set.");
	plugin_set = g_new0(struct bt_plugin_set, 1);

	if (!plugin_set) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to allocate one plugin set.");
		goto error;
	}

	bt_object_init_shared(&plugin_set->base, destroy_plugin_set);

	plugin_set->plugins = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_object_put_ref);
	if (!plugin_set->plugins) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to allocate plugin set's plugin array.");
		goto error;
	}

	BT_LOGD("Created empty plugin set: addr=%p", plugin_set);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(plugin_set);

end:
	return plugin_set;
}

static
void clear_plugin_set(struct bt_plugin_set *plugin_set)
{
	g_ptr_array_set_size(plugin_set->plugins, 0);
}

BT_EXPORT
enum bt_plugin_set_add_plugin_status
bt_plugin_set_add_plugin(struct bt_plugin_set *plugin_set,
		struct bt_plugin *plugin)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_PLUGIN_SET_NON_NULL(plugin_set);
	BT_ASSERT_PRE_PLUGIN_NON_NULL(plugin);
	BT_ASSERT_PRE("no-plugin-with-same-name-in-plugin-set",
		!bt_plugin_set_borrow_plugin_by_name_const(
			plugin_set, plugin->info.name->str),
		"Plugin set already contains a plugin with this name: "
		"plugin-set-addr=%p, %![plugin-]+l",
		plugin_set, plugin);

	bt_object_get_ref(plugin);
	g_ptr_array_add(plugin_set->plugins, plugin);
	bt_plugin_freeze(plugin);
	BT_LIB_LOGD("Added plugin to plugin set: "
		"plugin-set-addr=%p, %![plugin-]+l",
		plugin_set, plugin);

	return BT_FUNC_STATUS_OK;
}

BT_EXPORT
uint64_t bt_plugin_set_get_plugin_count(const struct bt_plugin_set *plugin_set)
{
	BT_ASSERT_PRE_DEV_PLUGIN_SET_NON_NULL(plugin_set);
	return (uint64_t) plugin_set->plugins->len;
}

BT_EXPORT
const struct bt_plugin *bt_plugin_set_borrow_plugin_by_index_const(
		const struct bt_plugin_set *plugin_set, uint64_t index)
{
	BT_ASSERT_PRE_DEV_PLUGIN_SET_NON_NULL(plugin_set);
	BT_ASSERT_PRE_DEV_VALID_INDEX(index, plugin_set->plugins->len);
	return g_ptr_array_index(plugin_set->plugins, index);
}

static
void set_provider_info_of_plugins(
	const struct bt_plugin_set *plugin_set,
	const struct bt_plugin_provider *plugin_provider)
{
	BT_ASSERT(plugin_provider);
	BT_ASSERT(plugin_set);

	uint64_t plugin_count = plugin_set->plugins->len;

	for (uint64_t i = 0; i < plugin_count; i++) {
		struct bt_plugin *plugin =
			plugin_set->plugins->pdata[i];

		if (!plugin->info.provider_info) {
			plugin->info.provider_info = plugin_provider->info;
			bt_plugin_provider_info_get_ref(
				plugin->info.provider_info);
		} else {
			BT_ASSERT(plugin->info.provider_info
				== plugin_provider->info);
		}
	}
}

#define FIND_ALL_FROM_STATIC_FUNC_NAME	"bt_plugin_find_all_from_static_func"

BT_EXPORT
const struct bt_plugin *bt_plugin_set_borrow_plugin_by_name_const(
		const struct bt_plugin_set *plugin_set, const char *name)
{
	const struct bt_plugin *plugin = NULL;
	size_t i;

	BT_ASSERT_PRE_DEV_PLUGIN_SET_NON_NULL(plugin_set);
	BT_ASSERT_PRE_DEV_NAME_NON_NULL(name);

	for (i = 0; i < plugin_set->plugins->len; i++) {
		const struct bt_plugin *plugin_candidate =
			plugin_set->plugins->pdata[i];
		if (strcmp(plugin_candidate->info.name->str, name) == 0) {
			plugin = plugin_candidate;
			goto end;
		}
	}

end:
	return plugin;
}

/*
 * Merge `src_plugin_set` into `dst_plugin_set`.
 *
 * If there are name conflicts, the plugins from `src_plugin_set` are dropped.
 */

static
enum bt_plugin_set_add_plugin_status merge_plugins_sets(
		struct bt_plugin_set *dst_plugin_set,
		const struct bt_plugin_set *src_plugin_set)
{
	enum bt_plugin_set_add_plugin_status status;

	for (size_t plugin_i = 0; plugin_i < src_plugin_set->plugins->len;
			plugin_i++) {
		struct bt_plugin *plugin =
			src_plugin_set->plugins->pdata[plugin_i];

		if (bt_plugin_set_borrow_plugin_by_name_const(dst_plugin_set,
				plugin->info.name->str)) {
			BT_LIB_LOGI(
				"Plugin with same name already exists in plugin"
				 " set, skipping: plugin-set-addr=%p, %![plugin-]+l",
				dst_plugin_set, plugin);
			continue;
		}

		status = bt_plugin_set_add_plugin(dst_plugin_set, plugin);
		if (status != BT_FUNC_STATUS_OK) {
			BT_LIB_LOGE_APPEND_CAUSE(
				"Cannot add plugin to plugin set: %![plugin-]+l",
				plugin);
			goto end;
		}
	}

	status = BT_PLUGIN_SET_ADD_PLUGIN_STATUS_OK;

end:
	return status;
}

BT_EXPORT
enum bt_plugin_find_all_from_static_status bt_plugin_find_all_from_static(
		bt_bool fail_on_load_error,
		const struct bt_plugin_set **plugin_set_out)
{
	enum bt_plugin_find_all_from_static_status status;
	struct bt_plugin_set *scratch_plugin_set = NULL;
	struct bt_plugin_set *plugin_set = NULL;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_PLUGIN_SET_OUT_NON_NULL(plugin_set_out);
	BT_LOGI_STR("Creating plugins from static");

	status = try_init_plugin_providers();
	if (status < 0) {
		/* try_init_plugin_providers() logs errors */
		goto end;
	}

	plugin_set = create_plugin_set();
	if (!plugin_set) {
		BT_LIB_LOGE_APPEND_CAUSE("Cannot create empty plugin set.");
		status = BT_FUNC_STATUS_MEMORY_ERROR;
		goto end;
	}

	if (g_plugin_providers.set) {
		scratch_plugin_set = create_plugin_set();
		if (!scratch_plugin_set) {
			BT_LIB_LOGE_APPEND_CAUSE("Cannot create scratch plugin set.");
			status = BT_FUNC_STATUS_MEMORY_ERROR;
			goto end;
		}

		for (size_t i = 0; i < g_plugin_providers.set->plugin_providers->len; i++) {
			struct bt_plugin_provider *plugin_provider =
				g_ptr_array_index(g_plugin_providers.set->plugin_providers, i);

			if (!plugin_provider->create_all_from_static) {
				continue;
			}

			struct bt_plugin_provider_create_all_from_static_options options =
				{ .base = { .fail_on_load_error = fail_on_load_error } };

			BT_ASSERT(scratch_plugin_set->plugins->len == 0);
			status = (int) plugin_provider->create_all_from_static(
				(void *) plugin_provider, &options, scratch_plugin_set);
			BT_ASSERT_POST_NO_ERROR_IF_NO_ERROR_STATUS(
					FIND_ALL_FROM_STATIC_FUNC_NAME,
					status);

			if (status == BT_FUNC_STATUS_OK) {
				BT_ASSERT_POST(
					FIND_ALL_FROM_STATIC_FUNC_NAME,
					"plugin-set-is-not-empty",
					scratch_plugin_set->plugins->len > 0,
					"Plugin set is empty with the "
					"`BT_PLUGIN_PROVIDER_CREATE_ALL_FROM_STATIC_FUNC_STATUS_OK` status: "
					"%![plugin-provider-]+N",
					plugin_provider->info);
				set_provider_info_of_plugins(scratch_plugin_set,
					plugin_provider);

				/* Move found plugins to the returned plugin set */
				status = (int) merge_plugins_sets(
					plugin_set, scratch_plugin_set);
				if (status != BT_FUNC_STATUS_OK) {
					goto end;
				}
			} else if (status < 0) {
				/*
				 * create_all_from_static handles
				 * `fail_on_load_error` itself, so this is a
				 * "real" error.
				 */
				goto end;
			} else {
				BT_ASSERT(status == BT_FUNC_STATUS_NOT_FOUND);
			}

			clear_plugin_set(scratch_plugin_set);
		}
	}

	/*
	 * If the output set is not empty we have successfully loaded
	 * plugins.
	 */
	if (plugin_set->plugins->len > 0) {
		status = BT_FUNC_STATUS_OK;
		*plugin_set_out = plugin_set;
		plugin_set = NULL;
	} else {
		status = BT_FUNC_STATUS_NOT_FOUND;
	}

end:
	BT_PLUGIN_SET_PUT_REF_AND_RESET(scratch_plugin_set);
	BT_PLUGIN_SET_PUT_REF_AND_RESET(plugin_set);


	if (status == BT_FUNC_STATUS_OK) {
		BT_LOGI("Created plugins from static: "
			"count=%u, plugin-set-addr=%p",
			(*plugin_set_out)->plugins->len,
			*plugin_set_out);
	} else if (status == BT_FUNC_STATUS_NOT_FOUND) {
		BT_LOGI_STR("Found no plugins in static");
	}

	return status;
}

#define FIND_ALL_FROM_FILE_FUNC_NAME	"bt_plugin_find_all_from_file_func"

BT_EXPORT
enum bt_plugin_find_all_from_file_status bt_plugin_find_all_from_file(
		const char *path, bt_bool fail_on_load_error,
		const struct bt_plugin_set **plugin_set_out)
{
	enum bt_plugin_find_all_from_file_status status;
	struct bt_plugin_set *scratch_plugin_set = NULL;
	struct bt_plugin_set *plugin_set = NULL;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_NON_NULL("path", path, "Path");
	BT_ASSERT_PRE_PLUGIN_SET_OUT_NON_NULL(plugin_set_out);
	BT_LOGI("Creating plugins from file: path=\"%s\"", path);

	status = try_init_plugin_providers();
	if (status < 0) {
		/* try_init_plugin_providers() logs errors */
		goto end;
	}

	plugin_set = create_plugin_set();
	if (!plugin_set) {
		BT_LIB_LOGE_APPEND_CAUSE("Cannot create empty plugin set.");
		status = BT_FUNC_STATUS_MEMORY_ERROR;
		goto end;
	}

	if (g_plugin_providers.set) {

		scratch_plugin_set = create_plugin_set();
		if (!scratch_plugin_set) {
			BT_LIB_LOGE_APPEND_CAUSE("Cannot create scratch plugin set.");
			status = BT_FUNC_STATUS_MEMORY_ERROR;
			goto end;
		}

		for (size_t i = 0; i < g_plugin_providers.set->plugin_providers->len; i++) {
			struct bt_plugin_provider *plugin_provider =
				g_ptr_array_index(g_plugin_providers.set->plugin_providers, i);

			if (!plugin_provider->create_all_from_file) {
				continue;
			}

			struct bt_plugin_provider_create_all_from_file_options options =
				{ .base = { .fail_on_load_error = fail_on_load_error } };

			BT_ASSERT(scratch_plugin_set->plugins->len == 0);
			status = (int) plugin_provider->create_all_from_file(
				(void *) plugin_provider, path, &options, scratch_plugin_set);
			BT_ASSERT_POST_NO_ERROR_IF_NO_ERROR_STATUS(
					FIND_ALL_FROM_FILE_FUNC_NAME,
					status);

			if (status == BT_FUNC_STATUS_OK) {
				BT_ASSERT_POST(
					FIND_ALL_FROM_FILE_FUNC_NAME,
					"plugin-set-is-not-empty",
					scratch_plugin_set->plugins->len > 0,
					"Plugin set is empty with the "
					"`BT_PLUGIN_PROVIDER_CREATE_ALL_FROM_FILE_FUNC_STATUS_OK` status: "
					"%![plugin-provider-]+N",
					plugin_provider->info);
				set_provider_info_of_plugins(scratch_plugin_set,
					plugin_provider);

				/* Move found plugins to the returned plugin set */
				status = (int) merge_plugins_sets(
					plugin_set, scratch_plugin_set);
				if (status != BT_FUNC_STATUS_OK) {
					goto end;
				}
			} else if (status < 0) {
				/*
				 * create_all_from_file handles
				 * `fail_on_load_error` itself, so this is a
				 * "real" error.
				 */
				goto end;
			} else {
				BT_ASSERT(status == BT_FUNC_STATUS_NOT_FOUND);
			}

			clear_plugin_set(scratch_plugin_set);
		}
	}

	/*
	 * If the output set is not empty we have successfully loaded
	 * plugins.
	 */
	if (plugin_set->plugins->len > 0) {
		status = BT_FUNC_STATUS_OK;
		*plugin_set_out = plugin_set;
		plugin_set = NULL;
	} else {
		status = BT_FUNC_STATUS_NOT_FOUND;
	}

end:
	BT_PLUGIN_SET_PUT_REF_AND_RESET(scratch_plugin_set);
	BT_PLUGIN_SET_PUT_REF_AND_RESET(plugin_set);

	if (status == BT_FUNC_STATUS_OK) {
		BT_LOGI("Created plugins from file: "
			"count=%u, path=\"%s\", plugin-set-addr=%p",
			(*plugin_set_out)->plugins->len, path,
			*plugin_set_out);
	} else if (status == BT_FUNC_STATUS_NOT_FOUND) {
		BT_LOGI("Found no plugins in file: path=\"%s\"", path);
	}

	BT_OBJECT_PUT_REF_AND_RESET(plugin_set);
	return status;
}

static
void destroy_gstring(void *data)
{
	g_string_free(data, TRUE);
}

BT_EXPORT
enum bt_plugin_find_all_status bt_plugin_find_all(bt_bool find_in_std_env_var,
		bt_bool find_in_user_dir, bt_bool find_in_sys_dir,
		bt_bool find_in_static, bt_bool fail_on_load_error,
		const struct bt_plugin_set **plugin_set_out)
{
	char *home_plugin_dir = NULL;
	const struct bt_plugin_set *plugin_set = NULL;
	GPtrArray *dirs = NULL;
	int ret;
	int status = BT_FUNC_STATUS_OK;
	uint64_t dir_i;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_PLUGIN_SET_OUT_NON_NULL(plugin_set_out);

	if (bt_common_user_and_system_plugin_paths_disabled() &&
			(find_in_user_dir || find_in_sys_dir)) {
		BT_LOGI_STR("Disabling user and system plugin directory "
			"lookup because the "
			"`LIBBABELTRACE2_DISABLE_USER_AND_SYSTEM_PLUGIN_PATHS` "
			"environment variable is set to `1`.");
		find_in_user_dir = BT_FALSE;
		find_in_sys_dir = BT_FALSE;
	}

	BT_LOGI("Finding all plugins in standard directories and built-in plugins: "
		"find-in-std-env-var=%d, find-in-user-dir=%d, "
		"find-in-sys-dir=%d, find-in-static=%d",
		find_in_std_env_var, find_in_user_dir, find_in_sys_dir,
		find_in_static);
	dirs = g_ptr_array_new_with_free_func((GDestroyNotify) destroy_gstring);
	if (!dirs) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate a GPtrArray.");
		status = BT_FUNC_STATUS_MEMORY_ERROR;
		goto end;
	}

	*plugin_set_out = create_plugin_set();
	if (!*plugin_set_out) {
		BT_LIB_LOGE_APPEND_CAUSE("Cannot create empty plugin set.");
		status = BT_FUNC_STATUS_MEMORY_ERROR;
		goto end;
	}

	/*
	 * Search order is:
	 *
	 * 1. `BABELTRACE_PLUGIN_PATH` environment variable
	 *    (colon-separated list of directories)
	 * 2. `~/.local/lib/babeltrace2/plugins`
	 * 3. Default system directory for Babeltrace plugins, usually
	 *    `/usr/lib/babeltrace2/plugins` or
	 *    `/usr/local/lib/babeltrace2/plugins` if installed locally
	 * 4. Built-in plugins (static)
	 *
	 * Directories are searched non-recursively.
	 */
	if (find_in_std_env_var) {
		const char *envvar = getenv("BABELTRACE_PLUGIN_PATH");

		if (envvar) {
			ret = bt_common_append_plugin_path_dirs(envvar, dirs);
			if (ret) {
				BT_LIB_LOGE_APPEND_CAUSE(
					"Failed to append plugin path to array of directories.");
				status = BT_FUNC_STATUS_MEMORY_ERROR;
				goto end;
			}
		}
	}

	if (find_in_user_dir) {
		home_plugin_dir = bt_common_get_home_plugin_path(
			BT_LOG_OUTPUT_LEVEL);
		if (home_plugin_dir) {
			GString *home_plugin_dir_str = g_string_new(
				home_plugin_dir);

			if (!home_plugin_dir_str) {
				BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate a GString.");
				status = BT_FUNC_STATUS_MEMORY_ERROR;
				goto end;
			}

			g_ptr_array_add(dirs, home_plugin_dir_str);
		}
	}

	if (find_in_sys_dir) {
		const char *system_plugin_dir =
			bt_common_get_system_plugin_path();

		if (system_plugin_dir) {
			GString *system_plugin_dir_str =
				g_string_new(system_plugin_dir);

			if (!system_plugin_dir_str) {
				BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate a GString.");
				status = BT_FUNC_STATUS_MEMORY_ERROR;
				goto end;
			}

			g_ptr_array_add(dirs, system_plugin_dir_str);
		}
	}

	for (dir_i = 0; dir_i < dirs->len; dir_i++) {
		GString *dir = dirs->pdata[dir_i];

		BT_OBJECT_PUT_REF_AND_RESET(plugin_set);

		/*
		 * Skip this if the directory does not exist because
		 * bt_plugin_find_all_from_dir() would log a warning.
		 */
		if (!g_file_test(dir->str, G_FILE_TEST_IS_DIR)) {
			BT_LOGI("Skipping nonexistent directory path: "
				"path=\"%s\"", dir->str);
			continue;
		}

		/* bt_plugin_find_all_from_dir() logs details/errors */
		status = bt_plugin_find_all_from_dir(dir->str, BT_FALSE,
			fail_on_load_error, &plugin_set);
		if (status < 0) {
			BT_ASSERT(!plugin_set);
			goto end;
		} else if (status == BT_FUNC_STATUS_NOT_FOUND) {
			BT_ASSERT(!plugin_set);
			BT_LOGI("No plugins found in directory: path=\"%s\"",
				dir->str);
			continue;
		}

		BT_ASSERT(status == BT_FUNC_STATUS_OK);
		BT_ASSERT(plugin_set);
		BT_LOGI("Found plugins in directory: path=\"%s\", count=%u",
			dir->str, plugin_set->plugins->len);

		status = merge_plugins_sets(
			(void *) *plugin_set_out, plugin_set);
		if (status != BT_FUNC_STATUS_OK) {
			goto end;
		}
	}

	if (find_in_static) {
		BT_OBJECT_PUT_REF_AND_RESET(plugin_set);
		status = bt_plugin_find_all_from_static(fail_on_load_error,
			&plugin_set);
		if (status < 0) {
			BT_ASSERT(!plugin_set);
			goto end;
		} else if (status == BT_FUNC_STATUS_NOT_FOUND) {
			BT_ASSERT(!plugin_set);
			BT_LOGI_STR("No plugins found in built-in plugins.");
			goto end;
		}

		BT_ASSERT(status == BT_FUNC_STATUS_OK);
		BT_ASSERT(plugin_set);
		BT_LOGI("Found built-in plugins: count=%u",
			plugin_set->plugins->len);

		status = merge_plugins_sets(
			(void *) *plugin_set_out, plugin_set);
		if (status != BT_FUNC_STATUS_OK) {
			goto end;
		}
	}

end:
	free(home_plugin_dir);
	bt_object_put_ref(plugin_set);

	if (dirs) {
		g_ptr_array_free(dirs, TRUE);
	}

	if (status < 0) {
		BT_OBJECT_PUT_REF_AND_RESET(*plugin_set_out);
	} else {
		BT_ASSERT(*plugin_set_out);

		if ((*plugin_set_out)->plugins->len > 0) {
			BT_LOGI("Found plugins in standard directories and built-in plugins: "
				"count=%u", (*plugin_set_out)->plugins->len);
			status = BT_FUNC_STATUS_OK;
		} else {
			BT_LOGI_STR("No plugins found in standard directories and built-in plugins.");
			status = BT_FUNC_STATUS_NOT_FOUND;
			BT_OBJECT_PUT_REF_AND_RESET(*plugin_set_out);
		}
	}

	return status;
}

BT_EXPORT
enum bt_plugin_find_status bt_plugin_find(const char *plugin_name,
		bt_bool find_in_std_env_var, bt_bool find_in_user_dir,
		bt_bool find_in_sys_dir, bt_bool find_in_static,
		bt_bool fail_on_load_error, const struct bt_plugin **plugin_out)
{
	enum bt_plugin_find_status status;
	const struct bt_plugin_set *plugin_set = NULL;
	uint64_t i;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_NAME_NON_NULL(plugin_name);
	BT_ASSERT_PRE_PLUGIN_OUT_NON_NULL(plugin_out);
	BT_LOGI("Finding named plugin in standard directories and built-in plugins: "
		"name=\"%s\", find-in-std-env-var=%d, find-in-user-dir=%d, "
		"find-in-sys-dir=%d, find-in-static=%d",
		plugin_name, find_in_std_env_var, find_in_user_dir,
		find_in_sys_dir, find_in_static);
	status = (enum bt_plugin_find_status) bt_plugin_find_all(find_in_std_env_var, find_in_user_dir,
		find_in_sys_dir, find_in_static, fail_on_load_error,
		&plugin_set);
	if (status != BT_FUNC_STATUS_OK) {
		BT_ASSERT(!plugin_set);
		goto end;
	}

	BT_ASSERT(plugin_set);

	for (i = 0; i < plugin_set->plugins->len; i++) {
		const struct bt_plugin *plugin = plugin_set->plugins->pdata[i];

		if (strcmp(plugin->info.name->str, plugin_name) == 0) {
			*plugin_out = plugin;
			bt_object_get_ref_no_null_check(*plugin_out);
			goto end;
		}
	}

	status = BT_FUNC_STATUS_NOT_FOUND;

end:
	if (status == BT_FUNC_STATUS_OK) {
		BT_ASSERT(*plugin_out);
		BT_LIB_LOGI("Found plugin in standard directories and built-in plugins: "
			"%!+l", *plugin_out);
        } else if (status == BT_FUNC_STATUS_NOT_FOUND) {
		BT_LOGI("No plugin found in standard directories and built-in plugins: "
			"name=\"%s\"", plugin_name);
	}

	bt_plugin_set_put_ref(plugin_set);

	return status;
}

static struct {
	pthread_mutex_t lock;
	struct bt_plugin_set *plugin_set;
	bool recurse;
	bool fail_on_load_error;
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

	/* Check for recursion */
	if (!append_all_from_dir_info.recurse && s->level > 1) {
		goto end;
	}

	switch (flag) {
	case FTW_F:
	{
		const struct bt_plugin_set *plugins_from_file = NULL;

		if (name[0] == '.') {
			/* Skip hidden files */
			BT_LOGI("Skipping hidden file: path=\"%s\"", file);
			goto end;
		}

		append_all_from_dir_info.status =
			bt_plugin_find_all_from_file(file,
				append_all_from_dir_info.fail_on_load_error,
				&plugins_from_file);
		if (append_all_from_dir_info.status == BT_FUNC_STATUS_OK) {
			BT_ASSERT(plugins_from_file);

			append_all_from_dir_info.status =
				merge_plugins_sets(
					append_all_from_dir_info.plugin_set,
					plugins_from_file);
			if (append_all_from_dir_info.status != BT_FUNC_STATUS_OK) {
				bt_object_put_ref(plugins_from_file);
				ret = -1;
				goto end;
			}

			bt_object_put_ref(plugins_from_file);
			goto end;
		} else if (append_all_from_dir_info.status < 0) {
			/* bt_plugin_find_all_from_file() logs errors */
			BT_ASSERT(!plugins_from_file);
			ret = -1;
			goto end;
		}

		/*
		 * Not found in this file: this is no an error; continue
		 * walking the directories.
		 */
		BT_ASSERT(!plugins_from_file);
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
int bt_plugin_create_append_all_from_dir(struct bt_plugin_set *plugin_set,
		const char *path, bt_bool recurse, bt_bool fail_on_load_error)
{
	int nftw_flags = FTW_PHYS;
	int ret;
	int status;
	struct stat sb;

	BT_ASSERT(plugin_set);
	BT_ASSERT(path);
	BT_ASSERT(strlen(path) < PATH_MAX);

	/*
	 * Make sure that path exists and is accessible.
	 * This is necessary since Cygwin implementation of nftw() is not POSIX
	 * compliant. Cygwin nftw() implementation does not fail on non-existent
	 * path with ENOENT. Instead, it flags the directory as FTW_NS. FTW_NS during
	 * nftw_append_all_from_dir is not treated as an error since we are
	 * traversing the tree for plugin discovery.
	 */
	if (stat(path, &sb)) {
		BT_LOGW_ERRNO("Cannot open directory",
			": path=\"%s\", recurse=%d",
			path, recurse);
		(void) BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_UNKNOWN(
			BT_LIB_LOG_LIBBABELTRACE2_NAME,
			"Cannot open directory: path=\"%s\", recurse=%d",
			path, recurse);
		status = BT_FUNC_STATUS_ERROR;
		goto end;
	}

	pthread_mutex_lock(&append_all_from_dir_info.lock);
	append_all_from_dir_info.plugin_set = plugin_set;
	append_all_from_dir_info.recurse = recurse;
	append_all_from_dir_info.status = BT_FUNC_STATUS_OK;
	append_all_from_dir_info.fail_on_load_error = fail_on_load_error;
	ret = nftw(path, nftw_append_all_from_dir,
		APPEND_ALL_FROM_DIR_NFDOPEN_MAX, nftw_flags);
	append_all_from_dir_info.plugin_set = NULL;
	status = append_all_from_dir_info.status;
	pthread_mutex_unlock(&append_all_from_dir_info.lock);
	if (ret) {
		BT_LIB_LOGW_APPEND_CAUSE("Failed to walk directory",
			": path=\"%s\", recurse=%d",
			path, recurse);
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

BT_EXPORT
enum bt_plugin_find_all_from_dir_status bt_plugin_find_all_from_dir(
		const char *path, bt_bool recurse, bt_bool fail_on_load_error,
		const struct bt_plugin_set **plugin_set_out)
{
	enum bt_plugin_find_all_from_dir_status status =
		BT_FUNC_STATUS_OK;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_PLUGIN_SET_OUT_NON_NULL(plugin_set_out);
	BT_LOGI("Creating all plugins in directory: path=\"%s\", recurse=%d",
		path, recurse);
	*plugin_set_out = create_plugin_set();
	if (!*plugin_set_out) {
		BT_LIB_LOGE_APPEND_CAUSE("Cannot create empty plugin set.");
		status = BT_FUNC_STATUS_MEMORY_ERROR;
		goto error;
	}

	/*
	 * Append found plugins to array (never returns
	 * `BT_FUNC_STATUS_NOT_FOUND`)
	 */
	status = bt_plugin_create_append_all_from_dir((void *) *plugin_set_out,
		path, recurse, fail_on_load_error);
	if (status < 0) {
		/*
		 * bt_plugin_create_append_all_from_dir() handles
		 * `fail_on_load_error`, so this is a "real" error.
		 */
		BT_LIB_LOGE_APPEND_CAUSE(
			"Cannot append plugins found in directory: "
			"path=\"%s\", status=%s",
			path, bt_common_func_status_string(status));
		goto error;
	}

	BT_ASSERT(status == BT_FUNC_STATUS_OK);

	if ((*plugin_set_out)->plugins->len == 0) {
		/* Nothing was appended: not found */
		BT_LOGI("No plugins found in directory: path=\"%s\"", path);
		status = BT_FUNC_STATUS_NOT_FOUND;
		goto error;
	}

	BT_LOGI("Created %u plugins from directory: count=%u, path=\"%s\"",
		(*plugin_set_out)->plugins->len,
		(*plugin_set_out)->plugins->len, path);
	goto end;

error:
	BT_ASSERT(status != BT_FUNC_STATUS_OK);
	BT_OBJECT_PUT_REF_AND_RESET(*plugin_set_out);

end:
	return status;
}

static
void destroy_plugin(struct bt_object *obj)
{
	struct bt_plugin *plugin;

	BT_ASSERT(obj);
	plugin = container_of(obj, struct bt_plugin, base);
	BT_LIB_LOGI("Destroying plugin object: %!+l", plugin);

	if (plugin->destruction_listeners) {
		int64_t i;
		const struct bt_error *saved_error;

		BT_LIB_LOGD("Calling plugin destruction listener(s): %!+l", plugin);

		/*
		 * The plugin's reference count is 0 if we're here.
		 * Increment it to avoid a double-destroy (possibly infinitely
		 * recursive). This could happen for example if a destruction
		 * listener did bt_object_get_ref() (or anything that causes
		 * bt_object_get_ref() to be called) on the plugin (ref.
		 * count goes from 0 to 1), and then bt_object_put_ref(): the
		 * reference count would go from 1 to 0 again and this function
		 * would be called again.
		 */
		plugin->base.ref_count++;

		saved_error = bt_current_thread_take_error();

		/* Call destruction listeners in reverse registration order */
		for (i = (int64_t)plugin->destruction_listeners->len - 1;
				i >= 0; i--) {
			struct bt_plugin_destruction_listener_elem elem =
				bt_g_array_index(plugin->destruction_listeners,
					struct bt_plugin_destruction_listener_elem, i);

			if (elem.func) {
				elem.func(plugin, elem.data);
				BT_ASSERT_POST_NO_ERROR(
					DESTRUCTION_LISTENER_FUNC_NAME);
			}

			/*
			 * The destruction listener should not have kept a
			 * reference to the plugin.
			 */
			BT_ASSERT_POST(DESTRUCTION_LISTENER_FUNC_NAME,
				"plugin-reference-count-not-changed",
				plugin->base.ref_count == 1,
				"Destruction listener kept a reference to the "
				"plugin being destroyed: %![plugin-]+l",
				plugin);
		}
		g_array_free(plugin->destruction_listeners, TRUE);
		plugin->destruction_listeners = NULL;

		if (saved_error) {
			BT_CURRENT_THREAD_MOVE_ERROR_AND_RESET(saved_error);
		}
	}

	if (plugin->src_comp_classes) {
		BT_LOGD_STR("Putting source component classes.");
		g_ptr_array_free(plugin->src_comp_classes, TRUE);
		plugin->src_comp_classes = NULL;
	}

	if (plugin->flt_comp_classes) {
		BT_LOGD_STR("Putting filter component classes.");
		g_ptr_array_free(plugin->flt_comp_classes, TRUE);
		plugin->flt_comp_classes = NULL;
	}

	if (plugin->sink_comp_classes) {
		BT_LOGD_STR("Putting sink component classes.");
		g_ptr_array_free(plugin->sink_comp_classes, TRUE);
		plugin->sink_comp_classes = NULL;
	}

	if (plugin->info.name) {
		g_string_free(plugin->info.name, TRUE);
		plugin->info.name = NULL;
	}

	if (plugin->info.path) {
		g_string_free(plugin->info.path, TRUE);
		plugin->info.path = NULL;
	}

	if (plugin->info.description) {
		g_string_free(plugin->info.description, TRUE);
		plugin->info.description = NULL;
	}

	if (plugin->info.author) {
		g_string_free(plugin->info.author, TRUE);
		plugin->info.author = NULL;
	}

	if (plugin->info.license) {
		g_string_free(plugin->info.license, TRUE);
		plugin->info.license = NULL;
	}

	if (plugin->info.version.extra) {
		g_string_free(plugin->info.version.extra, TRUE);
		plugin->info.version.extra = NULL;
	}

	BT_PLUGIN_PROVIDER_INFO_PUT_REF_AND_RESET(plugin->info.provider_info);
	g_free(plugin);
}

BT_EXPORT
struct bt_plugin *bt_plugin_create(const char *name)
{
	struct bt_plugin *plugin = NULL;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_NAME_NON_NULL(name);

	BT_LOGD("Creating empty plugin object: name=\"%s\"", name);

	plugin = g_new0(struct bt_plugin, 1);
	if (!plugin) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate one plugin.");
		goto error;
	}

	bt_object_init_shared(&plugin->base, destroy_plugin);

	/* Create empty arrays of component classes */
	plugin->src_comp_classes =
		g_ptr_array_new_with_free_func(
			(GDestroyNotify) bt_object_put_ref);
	if (!plugin->src_comp_classes) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate a GPtrArray.");
		goto error;
	}

	plugin->flt_comp_classes =
		g_ptr_array_new_with_free_func(
			(GDestroyNotify) bt_object_put_ref);
	if (!plugin->flt_comp_classes) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate a GPtrArray.");
		goto error;
	}

	plugin->sink_comp_classes =
		g_ptr_array_new_with_free_func(
			(GDestroyNotify) bt_object_put_ref);
	if (!plugin->sink_comp_classes) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate a GPtrArray.");
		goto error;
	}

	plugin->info.name = g_string_new(name);
	if (!plugin->info.name) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate a GString.");
		goto error;
	}

	/* Create empty info */
	plugin->info.path = g_string_new(NULL);
	if (!plugin->info.path) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate a GString.");
		goto error;
	}

	plugin->info.description = g_string_new(NULL);
	if (!plugin->info.description) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate a GString.");
		goto error;
	}

	plugin->info.author = g_string_new(NULL);
	if (!plugin->info.author) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate a GString.");
		goto error;
	}

	plugin->info.license = g_string_new(NULL);
	if (!plugin->info.license) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate a GString.");
		goto error;
	}

	plugin->info.version.extra = g_string_new(NULL);
	if (!plugin->info.version.extra) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate a GString.");
		goto error;
	}

	plugin->destruction_listeners = g_array_new(FALSE, TRUE,
		sizeof(struct bt_plugin_destruction_listener_elem));
	if (!plugin->destruction_listeners) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate a GArray.");
		goto error;
	}

	BT_LIB_LOGD("Created empty plugin object: %!+l", plugin);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(plugin);

end:
	return plugin;
}

BT_EXPORT
enum bt_plugin_set_path_status
bt_plugin_set_path(struct bt_plugin *plugin, const char *path)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_PLUGIN_NON_NULL(plugin);
	BT_ASSERT_PRE_DEV_PLUGIN_HOT(plugin);
	BT_ASSERT_PRE_NON_NULL("path", path, "Path");
	g_string_assign(plugin->info.path, path);
	plugin->info.path_set = true;
	BT_LIB_LOGD("Set plugin's path: %![plugin-]+l, path=\"%s\"",
		plugin, path);
	return BT_FUNC_STATUS_OK;
}

BT_EXPORT
enum bt_plugin_set_description_status
bt_plugin_set_description(struct bt_plugin *plugin,
		const char *description)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_PLUGIN_NON_NULL(plugin);
	BT_ASSERT_PRE_DEV_PLUGIN_HOT(plugin);
	BT_ASSERT_PRE_DESCR_NON_NULL(description);
	g_string_assign(plugin->info.description, description);
	plugin->info.description_set = true;
	BT_LIB_LOGD("Set plugin's description: %![plugin-]+l", plugin);
	return BT_FUNC_STATUS_OK;
}

BT_EXPORT
enum bt_plugin_set_author_status
bt_plugin_set_author(struct bt_plugin *plugin, const char *author)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_PLUGIN_NON_NULL(plugin);
	BT_ASSERT_PRE_DEV_PLUGIN_HOT(plugin);
	BT_ASSERT_PRE_NON_NULL("author", author, "Author");
	g_string_assign(plugin->info.author, author);
	plugin->info.author_set = true;
	BT_LIB_LOGD("Set plugin's author: %![plugin-]+l, author=\"%s\"",
		plugin, author);
	return BT_FUNC_STATUS_OK;
}

BT_EXPORT
enum bt_plugin_set_license_status
bt_plugin_set_license(struct bt_plugin *plugin, const char *license)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_PLUGIN_NON_NULL(plugin);
	BT_ASSERT_PRE_DEV_PLUGIN_HOT(plugin);
	BT_ASSERT_PRE_NON_NULL("license", license, "License");
	g_string_assign(plugin->info.license, license);
	plugin->info.license_set = true;
	BT_LIB_LOGD("Set plugin's license: %![plugin-]+l, license=\"%s\"",
		plugin, license);
	return BT_FUNC_STATUS_OK;
}

BT_EXPORT
enum bt_plugin_set_version_status
bt_plugin_set_version(struct bt_plugin *plugin, unsigned int major,
		unsigned int minor, unsigned int patch, const char *extra)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_PLUGIN_NON_NULL(plugin);
	BT_ASSERT_PRE_DEV_PLUGIN_HOT(plugin);
	plugin->info.version.major = major;
	plugin->info.version.minor = minor;
	plugin->info.version.patch = patch;

	if (extra) {
		g_string_assign(plugin->info.version.extra, extra);
		plugin->info.version.extra_set = true;
	}

	plugin->info.version_set = true;
	BT_LIB_LOGD("Set plugin's version: %![plugin-]+l, "
		"major=%u, minor=%u, patch=%u, extra=\"%s\"",
		plugin, major, minor, patch, extra);
	return BT_FUNC_STATUS_OK;
}

static
bool component_classes_contain_component_class(
		GPtrArray *comp_classes, const char *name)
{
	bool result = false;
	size_t i;

	BT_ASSERT(comp_classes);
	BT_ASSERT(name);

	for (i = 0; i < comp_classes->len; i++) {
		struct bt_component_class *comp_class_candidate =
			g_ptr_array_index(comp_classes, i);
		const char *comp_class_cand_name =
			bt_component_class_get_name(comp_class_candidate);

		BT_ASSERT_DBG(comp_class_cand_name);

		if (strcmp(name, comp_class_cand_name) == 0) {
			result = true;
			break;
		}
	}

	return result;
}

BT_EXPORT
enum bt_plugin_add_component_class_status
bt_plugin_add_component_class(
	struct bt_plugin *plugin, struct bt_component_class *comp_class)
{
	GPtrArray *comp_classes;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_PLUGIN_NON_NULL(plugin);
	BT_ASSERT_PRE_DEV_PLUGIN_HOT(plugin);
	BT_ASSERT_PRE_COMP_CLS_NON_NULL(comp_class);
	/* Check that component class is not already part of a plugin */
	BT_ASSERT_PRE("comp-class-is-not-part-of-plugin",
		!comp_class->part_of_plugin,
		"Component class is already part of a plugin: %![cc-]+C",
		comp_class);

	switch (comp_class->type) {
	case BT_COMPONENT_CLASS_TYPE_SOURCE:
		comp_classes = plugin->src_comp_classes;
		break;
	case BT_COMPONENT_CLASS_TYPE_FILTER:
		comp_classes = plugin->flt_comp_classes;
		break;
	case BT_COMPONENT_CLASS_TYPE_SINK:
		comp_classes = plugin->sink_comp_classes;
		break;
	default:
		bt_common_abort();
	}

	BT_ASSERT_PRE("comp-class-with-same-name-and-type-is-not-part-of-plugin",
		!component_classes_contain_component_class(
			comp_classes, comp_class->name->str),
		"Component class with same name and type is already part of the plugin: "
		"%![plugin-]+l, %![cc-]+C", plugin, comp_class);

	comp_class->part_of_plugin = true;

	/* Set component class's original plugin name */
	BT_ASSERT(comp_class->plugin_name);
	BT_ASSERT(plugin->info.name);
	g_string_assign(comp_class->plugin_name, plugin->info.name->str);

	/* Add new component class */
	bt_object_get_ref(comp_class);
	g_ptr_array_add(comp_classes, comp_class);

	bt_component_class_freeze(comp_class);
	BT_LIB_LOGD("Added component class to plugin: "
		"%![plugin-]+l, %![cc-]+C", plugin, comp_class);
	return BT_FUNC_STATUS_OK;
}

BT_EXPORT
const char *bt_plugin_get_name(const struct bt_plugin *plugin)
{
	BT_ASSERT_PRE_DEV_PLUGIN_NON_NULL(plugin);
	return plugin->info.name->str;
}

BT_EXPORT
const char *bt_plugin_get_author(const struct bt_plugin *plugin)
{
	BT_ASSERT_PRE_DEV_PLUGIN_NON_NULL(plugin);
	return plugin->info.author_set ? plugin->info.author->str : NULL;
}

BT_EXPORT
const char *bt_plugin_get_license(const struct bt_plugin *plugin)
{
	BT_ASSERT_PRE_DEV_PLUGIN_NON_NULL(plugin);
	return plugin->info.license_set ? plugin->info.license->str : NULL;
}

BT_EXPORT
const char *bt_plugin_get_path(const struct bt_plugin *plugin)
{
	BT_ASSERT_PRE_DEV_PLUGIN_NON_NULL(plugin);
	return plugin->info.path_set ? plugin->info.path->str : NULL;
}

BT_EXPORT
const char *bt_plugin_get_description(const struct bt_plugin *plugin)
{
	BT_ASSERT_PRE_DEV_PLUGIN_NON_NULL(plugin);
	return plugin->info.description_set ?
		plugin->info.description->str : NULL;
}

BT_EXPORT
enum bt_property_availability bt_plugin_get_version(const struct bt_plugin *plugin,
		unsigned int *major, unsigned int *minor, unsigned int *patch,
		const char **extra)
{
	enum bt_property_availability avail =
		BT_PROPERTY_AVAILABILITY_AVAILABLE;

	BT_ASSERT_PRE_DEV_PLUGIN_NON_NULL(plugin);

	if (!plugin->info.version_set) {
		BT_LIB_LOGD("Plugin's version is not set: %!+l", plugin);
		avail = BT_PROPERTY_AVAILABILITY_NOT_AVAILABLE;
		goto end;
	}

	if (major) {
		*major = plugin->info.version.major;
	}

	if (minor) {
		*minor = plugin->info.version.minor;
	}

	if (patch) {
		*patch = plugin->info.version.patch;
	}

	if (extra) {
		*extra = plugin->info.version.extra_set ? plugin->info.version.extra->str : NULL;
	}

end:
	return avail;
}

BT_EXPORT
const struct bt_plugin_provider_info *
bt_plugin_borrow_provider_info(const struct bt_plugin *plugin)
{
	BT_ASSERT_PRE_DEV_PLUGIN_NON_NULL(plugin);
	return plugin->info.provider_info;
}

BT_EXPORT
uint64_t bt_plugin_get_source_component_class_count(const struct bt_plugin *plugin)
{
	BT_ASSERT_PRE_DEV_PLUGIN_NON_NULL(plugin);
	return (uint64_t) plugin->src_comp_classes->len;
}

BT_EXPORT
uint64_t bt_plugin_get_filter_component_class_count(const struct bt_plugin *plugin)
{
	BT_ASSERT_PRE_DEV_PLUGIN_NON_NULL(plugin);
	return (uint64_t) plugin->flt_comp_classes->len;
}

BT_EXPORT
uint64_t bt_plugin_get_sink_component_class_count(const struct bt_plugin *plugin)
{
	BT_ASSERT_PRE_DEV_PLUGIN_NON_NULL(plugin);
	return (uint64_t) plugin->sink_comp_classes->len;
}

static inline
struct bt_component_class *borrow_component_class_by_index(
		const struct bt_plugin *plugin, GPtrArray *comp_classes,
		uint64_t index)
{
	BT_ASSERT_PRE_DEV_PLUGIN_NON_NULL(plugin);
	BT_ASSERT_PRE_DEV_VALID_INDEX(index, comp_classes->len);
	return g_ptr_array_index(comp_classes, index);
}

BT_EXPORT
const struct bt_component_class_source *
bt_plugin_borrow_source_component_class_by_index_const(
		const struct bt_plugin *plugin, uint64_t index)
{
	return (const void *) borrow_component_class_by_index(plugin,
		plugin->src_comp_classes, index);
}

BT_EXPORT
const struct bt_component_class_filter *
bt_plugin_borrow_filter_component_class_by_index_const(
		const struct bt_plugin *plugin, uint64_t index)
{
	return (const void *) borrow_component_class_by_index(plugin,
		plugin->flt_comp_classes, index);
}

BT_EXPORT
const struct bt_component_class_sink *
bt_plugin_borrow_sink_component_class_by_index_const(
		const struct bt_plugin *plugin, uint64_t index)
{
	return (const void *) borrow_component_class_by_index(plugin,
		plugin->sink_comp_classes, index);
}

static inline
struct bt_component_class *borrow_component_class_by_name(
		const struct bt_plugin *plugin, GPtrArray *comp_classes,
		const char *name)
{
	struct bt_component_class *comp_class = NULL;
	size_t i;

	BT_ASSERT_PRE_DEV_PLUGIN_NON_NULL(plugin);
	BT_ASSERT_PRE_DEV_NAME_NON_NULL(name);

	for (i = 0; i < comp_classes->len; i++) {
		struct bt_component_class *comp_class_candidate =
			g_ptr_array_index(comp_classes, i);
		const char *comp_class_cand_name =
			bt_component_class_get_name(comp_class_candidate);

		BT_ASSERT_DBG(comp_class_cand_name);

		if (strcmp(name, comp_class_cand_name) == 0) {
			comp_class = comp_class_candidate;
			break;
		}
	}

	return comp_class;
}

BT_EXPORT
const struct bt_component_class_source *
bt_plugin_borrow_source_component_class_by_name_const(
		const struct bt_plugin *plugin, const char *name)
{
	return (const void *) borrow_component_class_by_name(plugin,
		plugin->src_comp_classes, name);
}

BT_EXPORT
const struct bt_component_class_filter *
bt_plugin_borrow_filter_component_class_by_name_const(
		const struct bt_plugin *plugin, const char *name)
{
	return (const void *) borrow_component_class_by_name(plugin,
		plugin->flt_comp_classes, name);
}

BT_EXPORT
const struct bt_component_class_sink *
bt_plugin_borrow_sink_component_class_by_name_const(
		const struct bt_plugin *plugin, const char *name)
{
	return (const void *) borrow_component_class_by_name(plugin,
		plugin->sink_comp_classes, name);
}

BT_EXPORT
enum bt_plugin_add_listener_status bt_plugin_add_destruction_listener(
		const struct bt_plugin *_plugin,
		bt_plugin_destruction_listener_func listener,
		void *data, bt_listener_id *listener_id)
{
	struct bt_plugin *plugin = (void *) _plugin;
	uint64_t index;
	struct bt_plugin_destruction_listener_elem new_elem = {
		.func = listener,
		.data = data,
	};

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_PLUGIN_NON_NULL(plugin);
	BT_ASSERT_PRE_LISTENER_FUNC_NON_NULL(listener);

	index = plugin->destruction_listeners->len;
	/* Add at the end to be able to execute in reverse order */
	g_array_append_val(plugin->destruction_listeners, new_elem);

	if (listener_id) {
		*listener_id = index;
	}

	BT_LIB_LOGD("Added plugin destruction listener: %![plugin-]+l, "
			"listener-id=%" PRIu64, plugin, index);
	return BT_FUNC_STATUS_OK;
}

static
bool has_listener_id(const struct bt_plugin *plugin, uint64_t listener_id)
{
	return listener_id < plugin->destruction_listeners->len &&
		(&bt_g_array_index(plugin->destruction_listeners,
			struct bt_plugin_destruction_listener_elem,
			listener_id))->func;
}

BT_EXPORT
enum bt_plugin_remove_listener_status bt_plugin_remove_destruction_listener(
		const struct bt_plugin *_plugin, bt_listener_id listener_id)
{
	struct bt_plugin *plugin = (void *) _plugin;
	struct bt_plugin_destruction_listener_elem *elem;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_PLUGIN_NON_NULL(plugin);
	BT_ASSERT_PRE("listener-id-exists",
		has_listener_id(plugin, listener_id),
		"Plugin has no such plugin destruction listener ID: "
		"%![plugin-]+l, %" PRIu64, plugin, listener_id);
	elem = &bt_g_array_index(plugin->destruction_listeners,
			struct bt_plugin_destruction_listener_elem,
			listener_id);
	BT_ASSERT(elem->func);

	elem->func = NULL;
	elem->data = NULL;
	BT_LIB_LOGD("Removed plugin destruction listener: "
		"%![plugin-]+l, listener-id=%" PRIu64,
		plugin, listener_id);
	return BT_FUNC_STATUS_OK;
}

BT_EXPORT
void bt_plugin_get_ref(const struct bt_plugin *plugin)
{
	bt_object_get_ref(plugin);
}

BT_EXPORT
void bt_plugin_put_ref(const struct bt_plugin *plugin)
{
	bt_object_put_ref(plugin);
}

BT_EXPORT
void bt_plugin_set_get_ref(const struct bt_plugin_set *plugin_set)
{
	bt_object_get_ref(plugin_set);
}

BT_EXPORT
void bt_plugin_set_put_ref(const struct bt_plugin_set *plugin_set)
{
	bt_object_put_ref(plugin_set);
}
