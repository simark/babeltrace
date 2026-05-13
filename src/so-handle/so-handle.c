/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2024 Brice Videau <bvideau@anl.gov>
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#define BT_LOG_TAG "SO-HANDLE"
#define BT_LOG_OUTPUT_LEVEL so_handle_log_level
static int so_handle_log_level;

#include "logging/log.h"

#include "common/func-status.h"
#include "compat/compiler.h"
#include "common/log-and-append.h"
#include "so-handle/so-handle.h"

#define BT_SOH_LOG_AND_APPEND(_lvl, _fmt, ...)				\
	BT_LOG_AND_APPEND(_lvl, BT_LOG_TAG, _fmt, ##__VA_ARGS__)
#define BT_SOH_LOGE_APPEND_CAUSE(_fmt, ...)				\
	BT_SOH_LOG_AND_APPEND(BT_LOG_ERROR, _fmt, ##__VA_ARGS__)

static
void destroy_so_handle(struct bt_object *obj)
{
	struct so_handle *so_handle;

	BT_ASSERT(obj);
	so_handle = container_of(obj, struct so_handle, base);
	so_handle_log_level = so_handle->log_level;

	const char *path = so_handle->path ? so_handle->path->str : NULL;

	BT_LOGI("Destroying shared library handle: addr=%p, path=\"%s\"",
		so_handle, path);

	if (so_handle->finalize_funcs) {
		gint i;

		for (i = 0; i < so_handle->finalize_funcs->len; ++i) {
			const bt_plugin_finalize_func finalize_func =
				g_ptr_array_index(so_handle->finalize_funcs, i);

			BT_ASSERT(finalize_func);

			BT_LOGD_STR("Calling user's plugin finalize function.");
			finalize_func();
			BT_LOGD_STR("User function returned.");
		}

		g_ptr_array_free(so_handle->finalize_funcs, TRUE);
		so_handle->finalize_funcs = NULL;
	}

	if (so_handle->module) {
#ifdef BT_DEBUG_MODE
		/*
		 * Valgrind shows incomplete stack traces when
		 * dynamically loaded libraries are closed before it
		 * finishes. Use the LIBBABELTRACE2_NO_DLCLOSE in a debug
		 * build to avoid this.
		 */
		const char *var = getenv("LIBBABELTRACE2_NO_DLCLOSE");

		if (!var || strcmp(var, "1") != 0) {
#endif
			BT_LOGI("Closing GModule: path=\"%s\"", path);

			if (!g_module_close(so_handle->module)) {
				/*
				 * Just log here: we're in a destructor,
				 * so we cannot append an error cause
				 * (there's no returned status).
				 */
				BT_LOGE("Cannot close GModule: %s: path=\"%s\"",
					g_module_error(), path);
			}

			so_handle->module = NULL;
#ifdef BT_DEBUG_MODE
		} else {
			BT_LOGI("Not closing GModule because `LIBBABELTRACE2_NO_DLCLOSE=1`: "
				"path=\"%s\"", path);
		}
#endif
	}

	if (so_handle->path) {
		g_string_free(so_handle->path, TRUE);
		so_handle->path = NULL;
	}

	g_free(so_handle);
}

int create_so_handle(const char *path, int log_level,
		struct so_handle **so_handle_out)
{
	int status;
	struct so_handle *so_handle;

	BT_ASSERT(so_handle_out);

	*so_handle_out = NULL;
	so_handle_log_level = log_level;

	BT_LOGI("Creating shared library handle: path=\"%s\"", path ? path : "(null)");
	so_handle = g_new0(struct so_handle, 1);
	if (!so_handle) {
		BT_SOH_LOGE_APPEND_CAUSE("Failed to allocate one shared library handle.");
		status = BT_FUNC_STATUS_MEMORY_ERROR;
		goto end;
	}

	bt_object_init_shared(&so_handle->base, destroy_so_handle);
	so_handle->log_level = log_level;

	so_handle->finalize_funcs = g_ptr_array_new();
	if (!so_handle->finalize_funcs) {
		BT_SOH_LOGE_APPEND_CAUSE("Failed to allocate a GPtrArray.");
		status = BT_FUNC_STATUS_MEMORY_ERROR;
		goto end;
	}

	if (!path) {
		*so_handle_out = so_handle;
		so_handle = NULL;
		status = BT_FUNC_STATUS_OK;
		goto end;
	}

	so_handle->path = g_string_new(path);
	if (!so_handle->path) {
		BT_SOH_LOGE_APPEND_CAUSE("Failed to allocate a GString.");
		status = BT_FUNC_STATUS_MEMORY_ERROR;
		goto end;
	}

	so_handle->module = g_module_open(path, G_MODULE_BIND_LOCAL);
	if (!so_handle->module) {
		/*
		 * INFO-level logging because we're only _trying_ to
		 * open this file, it might not exist.
		 */
		BT_LOGI("Cannot open GModule: %s: path=\"%s\"",
			g_module_error(), path);
		status = BT_FUNC_STATUS_NOT_FOUND;
		goto end;
	}

	*so_handle_out = so_handle;
	so_handle = NULL;
	status = BT_FUNC_STATUS_OK;

end:
	so_handle_put_ref(so_handle);
	BT_ASSERT(*so_handle_out || status != BT_FUNC_STATUS_OK);
	if (*so_handle_out) {
		BT_LOGI("Created shared library handle: path=\"%s\", addr=%p",
			path ? path : "(null)", *so_handle_out);
	}

	return status;
}
