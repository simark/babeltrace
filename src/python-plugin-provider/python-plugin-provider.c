/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017 Philippe Proulx <pproulx@efficios.com>
 *
 * Babeltrace Python plugin provider
 */

/* `Python.h` needs to be included before any system header. */
#include <Python.h>
#include "py-common/py-common.h"

#define BT_LOG_TAG "PLUGIN-PY"
#define BT_LOG_OUTPUT_LEVEL log_level
#include "logging/log.h"

#include "common/common.h"
#include "common/func-status.h"
#include "common/log-and-append.h"
#include "common/log-fmt.h"
#ifdef __ELF__
#include <dlfcn.h>
#endif
#include <stdbool.h>
#include <stdlib.h>
#include <signal.h>
#include <glib.h>

#define PYTHON_PLUGIN_FILE_PREFIX	"bt_plugin_"
#define PYTHON_PLUGIN_FILE_PREFIX_LEN	(sizeof(PYTHON_PLUGIN_FILE_PREFIX) - 1)
#define PYTHON_PLUGIN_FILE_EXT		".py"
#define PYTHON_PLUGIN_FILE_EXT_LEN	(sizeof(PYTHON_PLUGIN_FILE_EXT) - 1)

#define PYTHON_PLUGIN_PROVIDER_NAME "Python plugin provider"

#define BT_PPP_LOG_AND_APPEND(_lvl, _fmt, ...)				\
	BT_LOG_AND_APPEND(_lvl, PYTHON_PLUGIN_PROVIDER_NAME, _fmt, ##__VA_ARGS__)

#define BT_PPP_LOGW_APPEND_CAUSE(_fmt, ...)				\
	BT_PPP_LOG_AND_APPEND(BT_LOG_WARNING, _fmt, ##__VA_ARGS__)
#define BT_PPP_LOGE_APPEND_CAUSE(_fmt, ...)				\
	BT_PPP_LOG_AND_APPEND(BT_LOG_ERROR, _fmt, ##__VA_ARGS__)

enum python_state {
	/* init_python() not called yet */
	PYTHON_STATE_NOT_INITED = 0,

	/* init_python() called once with success */
	PYTHON_STATE_FULLY_INITIALIZED,

	/* init_python() called once without success */
	PYTHON_STATE_CANNOT_INITIALIZE,

	/*
	 * init_python() called, but environment variable asks the
	 * Python interpreter not to be loaded.
	 */
	PYTHON_STATE_WONT_INITIALIZE,
};

struct python_plugin_provider_data {
	enum python_state python_state;
	PyObject *py_try_load_plugin_module_func;
	bool python_was_initialized_by_us;
};

static
void append_python_traceback_error_cause(int log_level)
{
	GString *exc = NULL;

	if (Py_IsInitialized() && PyErr_Occurred()) {
		exc = bt_py_common_format_current_exception(log_level);
		if (!exc) {
			BT_LOGE_STR("Failed to format Python exception.");
			goto end;
		}

		(void) BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_UNKNOWN(
			PYTHON_PLUGIN_PROVIDER_NAME, "%s", exc->str);
	}

end:
	if (exc) {
		g_string_free(exc, TRUE);
	}
}

static
void log_python_traceback(int log_level)
{
	GString *exc = NULL;

	if (Py_IsInitialized() && PyErr_Occurred()) {
		exc = bt_py_common_format_current_exception(BT_LOG_OUTPUT_LEVEL);
		if (!exc) {
			BT_LOGE_STR("Failed to format Python exception.");
			goto end;
		}

		BT_LOG_WRITE_PRINTF(log_level, BT_LOG_TAG,
			"Exception occurred: Python traceback:\n%s", exc->str);
	}

end:
	if (exc) {
		g_string_free(exc, TRUE);
	}
}

static
void pyerr_clear(void)
{
	if (Py_IsInitialized()) {
		PyErr_Clear();
	}
}

static bt_plugin_provider_initialize_func_status
initialize_python_plugin_provider(bt_self_plugin_provider *self_plugin_provider)
{
	struct python_plugin_provider_data *data;
	bt_plugin_provider_initialize_func_status status;
	int log_level;

	BT_ASSERT(self_plugin_provider);

	log_level = bt_self_plugin_provider_get_logging_level(
		self_plugin_provider);

	data = g_new0(struct python_plugin_provider_data, 1);

	if (!data) {
		BT_PPP_LOGE_APPEND_CAUSE(
			"Failed to allocate plugin provider data.");
		status = BT_PLUGIN_PROVIDER_INITIALIZE_FUNC_STATUS_MEMORY_ERROR;
		goto end;
	}

	data->python_state = PYTHON_STATE_NOT_INITED;
	data->py_try_load_plugin_module_func = NULL;
	data->python_was_initialized_by_us = false;

	bt_self_plugin_provider_set_data(self_plugin_provider, data);
	status = BT_PLUGIN_PROVIDER_INITIALIZE_FUNC_STATUS_OK;

end:

	return status;
}

static
int init_python(struct python_plugin_provider_data *data, int log_level)
{
	int ret = BT_FUNC_STATUS_OK;
	PyObject *py_bt2_py_plugin_mod = NULL;
	const char *dis_python_env;
#ifndef __MINGW32__
	sig_t old_sigint = signal(SIGINT, SIG_DFL);
#endif

	switch (data->python_state) {
	case PYTHON_STATE_NOT_INITED:
		break;
	case PYTHON_STATE_FULLY_INITIALIZED:
		goto end;
	case PYTHON_STATE_WONT_INITIALIZE:
		ret = BT_FUNC_STATUS_NOT_FOUND;
		goto end;
	case PYTHON_STATE_CANNOT_INITIALIZE:
		ret = BT_FUNC_STATUS_ERROR;
		goto end;
	default:
		bt_common_abort();
	}

	/*
	 * User can disable Python plugin support with the
	 * `LIBBABELTRACE2_DISABLE_PYTHON_PLUGINS` environment variable
	 * set to 1.
	 */
	dis_python_env = getenv("LIBBABELTRACE2_DISABLE_PYTHON_PLUGINS");
	if (dis_python_env && strcmp(dis_python_env, "1") == 0) {
		BT_LOGI_STR("Python plugin support is disabled because the "
			"`LIBBABELTRACE2_DISABLE_PYTHON_PLUGINS` environment "
			"variable is set to `1`.");
		data->python_state = PYTHON_STATE_WONT_INITIALIZE;
		ret = BT_FUNC_STATUS_NOT_FOUND;
		goto end;
	}

	/*
	 * On ELF platforms, this shared object is loaded with RTLD_LOCAL, so
	 * libpython, which it depends on (with DT_NEEDED) is also loaded with
	 * the RTLD_LOCAL semantic.  Trying to load native Python modules would
	 * fail, as they would not find the symbols they need from libpython.
	 *
	 * Promote libpython to the global symbol namespace before initializing
	 * the interpreter so that extension module imports succeed.
	 *
	 * Use dladdr on a known libpython symbol to discover the exact path
	 * of the already-loaded library, then re-dlopen it with RTLD_GLOBAL
	 * and RTLD_NOLOAD to promote it without loading a second copy.
	 */
#ifdef __ELF__
	{
		Dl_info libpython_info;

		if (dladdr(Py_IsInitialized, &libpython_info)
				&& libpython_info.dli_fname) {
			void *libpython_handle;

			libpython_handle = dlopen(libpython_info.dli_fname,
				RTLD_GLOBAL | RTLD_NOLOAD | RTLD_LAZY);
			if (libpython_handle) {
				BT_LOGI("Promoted %s to the global symbol "
					"namespace.",
					libpython_info.dli_fname);
				dlclose(libpython_handle);
			} else {
				BT_LOGW("Failed to promote %s to the global "
					"symbol namespace: %s. Python "
					"extension module imports may fail.",
					libpython_info.dli_fname, dlerror());
			}
		} else {
			BT_LOGW_STR("Failed to resolve libpython library path "
				"with dladdr. Python extension module "
				"imports may fail.");
		}
	}
#endif

	if (!Py_IsInitialized()) {
		BT_LOGI_STR("Python interpreter is not initialized: initializing Python interpreter.");
		Py_InitializeEx(0);
		data->python_was_initialized_by_us = true;
		BT_LOGI("Initialized Python interpreter: version=\"%s\"",
			Py_GetVersion());
	} else {
		BT_LOGI("Python interpreter is already initialized: version=\"%s\"",
			Py_GetVersion());
	}

	py_bt2_py_plugin_mod = PyImport_ImportModule("bt2.py_plugin");
	if (!py_bt2_py_plugin_mod) {
		append_python_traceback_error_cause(log_level);
		BT_PPP_LOGW_APPEND_CAUSE(
			"Cannot import `bt2.py_plugin` Python module: "
			"Python plugin support is disabled.");
		data->python_state = PYTHON_STATE_CANNOT_INITIALIZE;
		ret = BT_FUNC_STATUS_ERROR;
		goto end;
	}

	data->py_try_load_plugin_module_func =
		PyObject_GetAttrString(py_bt2_py_plugin_mod, "_try_load_plugin_module");
	if (!data->py_try_load_plugin_module_func) {
		append_python_traceback_error_cause(log_level);
		BT_PPP_LOGW_APPEND_CAUSE(
			"Cannot get `_try_load_plugin_module` attribute from `bt2.py_plugin` Python module: "
			"Python plugin support is disabled.");
		data->python_state = PYTHON_STATE_CANNOT_INITIALIZE;
		ret = BT_FUNC_STATUS_ERROR;
		goto end;
	}

	data->python_state = PYTHON_STATE_FULLY_INITIALIZED;

end:
#ifndef __MINGW32__
	if (old_sigint != SIG_ERR) {
		(void) signal(SIGINT, old_sigint);
	}
#endif

	log_python_traceback(ret == BT_FUNC_STATUS_ERROR ?
		BT_LOG_WARNING : BT_LOG_INFO);
	pyerr_clear();
	Py_XDECREF(py_bt2_py_plugin_mod);
	return ret;
}

static
void finalize_python_plugin_provider(
		bt_self_plugin_provider *self_plugin_provider)
{
	struct python_plugin_provider_data *data = NULL;

	BT_ASSERT(self_plugin_provider);

	data = bt_self_plugin_provider_get_data(self_plugin_provider);

	BT_ASSERT(data);

	if (Py_IsInitialized()) {
		if (data->py_try_load_plugin_module_func) {
			Py_DECREF(data->py_try_load_plugin_module_func);
			data->py_try_load_plugin_module_func = NULL;
		}

		if (data->python_was_initialized_by_us) {
			Py_Finalize();
		}
	}

	data->python_state = PYTHON_STATE_NOT_INITED;
	g_free(data);
}

static
enum bt_plugin_provider_create_all_from_file_func_status
create_all_python_plugins_from_file(
		bt_self_plugin_provider *self_plugin_provider, const char *path,
		const bt_plugin_provider_create_all_from_file_options *options,
		bt_plugin_set *plugin_set)
{
	bt_plugin *plugin = NULL;
	PyObject *py_plugin_addr = NULL;
	gchar *basename = NULL;
	size_t path_len;
	int status = BT_FUNC_STATUS_OK;
	struct python_plugin_provider_data *data;
	bool fail_on_load_error;
	int log_level;

	BT_ASSERT(self_plugin_provider);
	BT_ASSERT(path);
	BT_ASSERT(options);
	BT_ASSERT(plugin_set);
	fail_on_load_error =
		bt_plugin_provider_create_all_from_file_options_get_fail_on_load_error(
			options);
	log_level =
		bt_self_plugin_provider_get_logging_level(self_plugin_provider);
	data = bt_self_plugin_provider_get_data(self_plugin_provider);
	BT_ASSERT(data);

	if (data->python_state == PYTHON_STATE_CANNOT_INITIALIZE) {
		/*
		 * We do not even care about the rest of the function
		 * here because we already know Python cannot be fully
		 * initialized.
		 */
		BT_PPP_LOGE_APPEND_CAUSE(
			"Python interpreter could not be initialized previously.");
		status = BT_FUNC_STATUS_ERROR;
		goto error;
	} else if (data->python_state == PYTHON_STATE_WONT_INITIALIZE) {
		/*
		 * This is not an error: the environment requires that
		 * Python plugins are disabled, so it's simply not
		 * found.
		 */
		BT_LOGI_STR("Python plugin support was disabled previously "
			"because the `LIBBABELTRACE2_DISABLE_PYTHON_PLUGINS` "
			"environment variable is set to `1`.");
		status = BT_FUNC_STATUS_NOT_FOUND;
		goto error;
	}

	BT_LOGI("Trying to create all Python plugins from file: path=\"%s\"",
		path);
	path_len = strlen(path);

	/* File name ends with `.py` */
	if (strncmp(path + path_len - PYTHON_PLUGIN_FILE_EXT_LEN,
			PYTHON_PLUGIN_FILE_EXT,
			PYTHON_PLUGIN_FILE_EXT_LEN) != 0) {
		BT_LOGI("Skipping non-Python file: path=\"%s\"", path);
		status = BT_FUNC_STATUS_NOT_FOUND;
		goto error;
	}

	/* File name starts with `bt_plugin_` */
	basename = g_path_get_basename(path);
	if (!basename) {
		BT_PPP_LOGE_APPEND_CAUSE(
			"Cannot get path's basename: path=\"%s\"", path);
		status = BT_FUNC_STATUS_ERROR;
		goto error;
	}

	if (strncmp(basename, PYTHON_PLUGIN_FILE_PREFIX,
			PYTHON_PLUGIN_FILE_PREFIX_LEN) != 0) {
		BT_LOGI("Skipping Python file not starting with `%s`: "
			"path=\"%s\"", PYTHON_PLUGIN_FILE_PREFIX, path);
		status = BT_FUNC_STATUS_NOT_FOUND;
		goto error;
	}

	/*
	 * Initialize Python now.
	 *
	 * This is not done in the library constructor because the
	 * interpreter is somewhat slow to initialize. If you don't
	 * have any potential Python plugins, you don't need to endure
	 * this waiting time everytime you load the library.
	 */
	status = init_python(data, log_level);
	if (status != BT_FUNC_STATUS_OK) {
		/* init_python() logs and append errors */
		goto error;
	}

	/*
	 * Call bt2.py_plugin._try_load_plugin_module() with this path
	 * to get a plugin handle if the plugin is loadable and
	 * complete. This function raises on error, in which case
	 * PyObject_CallFunction returns NULL with the Python error
	 * state set.
	 */
	BT_LOGD_STR("Getting Python plugin set from Python module.");
	py_plugin_addr = PyObject_CallFunction(data->py_try_load_plugin_module_func,
		"(s)", path);
	if (!py_plugin_addr) {
		if (fail_on_load_error) {
			append_python_traceback_error_cause(log_level);
			BT_PPP_LOGW_APPEND_CAUSE(
				"Cannot load Python plugin: path=\"%s\"", path);
			status = BT_FUNC_STATUS_ERROR;
		} else {
			BT_LOGW("Cannot load Python plugin: path=\"%s\"", path);
			status = BT_FUNC_STATUS_NOT_FOUND;
		}

		goto error;
	}

	BT_ASSERT(PyLong_Check(py_plugin_addr));
	plugin = PyLong_AsVoidPtr(py_plugin_addr);
	BT_ASSERT(plugin);

	BT_LOGD("Created Python plugin from file: "
		"plugin-name=\"%s\", path=\"%s\"",
		bt_plugin_get_name(plugin), path);

	status = bt_plugin_set_add_plugin(plugin_set, plugin);
	if (status != BT_FUNC_STATUS_OK) {
		BT_PPP_LOGE_APPEND_CAUSE(
			"Cannot add plugin to plugin set: "
			"plugin-set-addr=%p, " BT_PLUGIN_FMT,
			plugin_set, BT_PLUGIN_ARGS(plugin));
		goto error;
	}

	status = BT_FUNC_STATUS_OK;
	goto end;

error:
	BT_ASSERT(status != BT_FUNC_STATUS_OK);
	log_python_traceback(fail_on_load_error ? BT_LOG_WARNING : BT_LOG_INFO);
	pyerr_clear();

end:
	g_free(basename);
	return status;
}

#ifndef BT_BUILT_IN_PYTHON_PLUGIN_SUPPORT
BT_PLUGIN_PROVIDER_MODULE();
#endif

/* Initialize plug-in provider description. */
BT_PLUGIN_PROVIDER(python);
BT_PLUGIN_PROVIDER_DESCRIPTION("Python plugin support");
BT_PLUGIN_PROVIDER_AUTHOR("EfficiOS <https://www.efficios.com/>");
BT_PLUGIN_PROVIDER_LICENSE("MIT");

/* Set plug-in provider functions. */
BT_PLUGIN_PROVIDER_INITIALIZE_FUNC(initialize_python_plugin_provider);
BT_PLUGIN_PROVIDER_FINALIZE_FUNC(finalize_python_plugin_provider);
BT_PLUGIN_PROVIDER_CREATE_ALL_FROM_FILE_FUNC(create_all_python_plugins_from_file);
