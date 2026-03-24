/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2010-2019 EfficiOS Inc. and Linux Foundation
 */

#ifndef BABELTRACE2_PLUGIN_PLUGIN_LOADING_H
#define BABELTRACE2_PLUGIN_PLUGIN_LOADING_H

/* IWYU pragma: private, include <babeltrace2/babeltrace.h> */

#ifndef __BT_IN_BABELTRACE_H
# error "Please include <babeltrace2/babeltrace.h> instead."
#endif

#include <stdint.h>
#include <stddef.h>

#include <babeltrace2/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
@defgroup api-plugin-loading Plugin loading

@brief
    Plugin loading functions.

The bt_plugin_find_all(), bt_plugin_find_all_from_file(),
bt_plugin_find_all_from_dir(), and bt_plugin_find_all_from_static()
functions return a <strong>\bt_plugin_set</strong>, that is, a shared
object containing one or more \bt_p_plugin.

@attention
    The plugin loading API offers functions to <em>find and load</em>
    existing plugins and use the packaged \bt_p_comp_cls. To \em write a
    shared object plugin, see \ref api-plugin-dev.

<h1>Find and load plugins</h1>

\anchor api-plugin-loading-def-dirs The bt_plugin_find() and
bt_plugin_find_all() functions find and load plugins from the default
plugin search directories and from the static plugins.

The plugin search order is:

-# The colon-separated (or semicolon-separated on Windows) list of
   directories in the \c BABELTRACE_PLUGIN_PATH environment variable,
   if it's set.

   The function searches each directory in this list, without recursing.

-# <code>$HOME/.local/lib/babeltrace2/plugins</code>,
   without recursing.

-# The system \bt_name plugin directory, typically
   <code>/usr/lib/babeltrace2/plugins</code> or
   <code>/usr/local/lib/babeltrace2/plugins</code> on Linux,
   without recursing.

-# The static plugins.

Both bt_plugin_find() and bt_plugin_find_all() functions have dedicated
boolean parameters to include or exclude each of the four locations
above.

<h2>Find and load a plugin by name</h2>

Find and load a plugin by name with bt_plugin_find().

bt_plugin_find() tries to find a plugin with a specific name within
the \ref api-plugin-loading-def-dirs "default plugin search directories"
and static plugins.

<h2>Find and load all the plugins from the default directories</h2>

Load all the plugins found in the
\ref api-plugin-loading-def-dirs "default plugin search directories"
and static plugins with bt_plugin_find_all().

<h2>Find and load plugins from a specific file or directory</h2>

Find and load plugins from a specific file (<code>.so</code>,
<code>.dll</code>, or <code>.py</code>) with
bt_plugin_find_all_from_file().

A single shared object file can contain multiple plugins, although it's
not common practice to do so.

Find and load plugins from a specific directory with
bt_plugin_find_all_from_dir(). This function can search for plugins
within the given directory recursively or not.

<h2>Find and load static plugins</h2>

Find and load static plugins with bt_plugin_find_all_from_static().

A static plugin is built directly into the application or library
instead of being a separate shared object file.
*/

/*! @{ */

/*!
@brief
    Status codes for bt_plugin_find().
*/
typedef enum bt_plugin_find_status {
	/*!
	@brief
	    Success.
	*/
	BT_PLUGIN_FIND_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Plugin not found.
	*/
	BT_PLUGIN_FIND_STATUS_NOT_FOUND		= __BT_FUNC_STATUS_NOT_FOUND,

	/*!
	@brief
	    Out of memory.
	*/
	BT_PLUGIN_FIND_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    Error.
	*/
	BT_PLUGIN_FIND_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
} bt_plugin_find_status;

/*!
@brief
    Finds and loads a single plugin which has the name
    \bt_p{plugin_name} from the default plugin search directories and
    static plugins, setting \bt_p{*plugin} to the result.

This function returns the first plugin which has the name
\bt_p{plugin_name} within, in order:

-# <strong>If the \bt_p{find_in_std_env_var} parameter is
   #BT_TRUE</strong>,
   the colon-separated (or semicolon-separated on Windows) list of
   directories in the \c BABELTRACE_PLUGIN_PATH environment variable,
   if it's set.

   The function searches each directory in this list, without recursing.

-# <strong>If the \bt_p{find_in_user_dir} parameter is
   #BT_TRUE</strong>, <code>$HOME/.local/lib/babeltrace2/plugins</code>,
   without recursing.

-# <strong>If the \bt_p{find_in_sys_dir} is #BT_TRUE</strong>, the
   system \bt_name plugin directory, typically
   <code>/usr/lib/babeltrace2/plugins</code> or
   <code>/usr/local/lib/babeltrace2/plugins</code> on Linux, without
   recursing.

-# <strong>If the \bt_p{find_in_static} is #BT_TRUE</strong>,
   the static plugins.

@note
    The name of a plugin isn't related to the name of its file (shared
    object or Python file). For example, a plugin found in the file
    \c patente.so can be named <code>Dan</code>.

If this function finds a file which looks like a plugin (shared object
file or Python file with the \c bt_plugin_ prefix), but it fails to load
it for any reason, then this function:

<dl>
  <dt>If \bt_p{fail_on_load_error} is #BT_TRUE</dt>
  <dd>Returns #BT_PLUGIN_FIND_STATUS_ERROR.</dd>

  <dt>If \bt_p{fail_on_load_error} is #BT_FALSE</dt>
  <dd>Ignores the loading error and continues searching.</dd>
</dl>

If this function doesn't find any plugin, it returns
#BT_PLUGIN_FIND_STATUS_NOT_FOUND and does \em not set \bt_p{*plugin}.

@param[in] plugin_name
    Name of the plugin to find and load.
@param[in] find_in_std_env_var
    #BT_TRUE to try to find the plugin named \bt_p{plugin_name} in the
    colon-separated (or semicolon-separated on Windows) list of
    directories in the \c BABELTRACE_PLUGIN_PATH environment variable.
@param[in] find_in_user_dir
    #BT_TRUE to try to find the plugin named \bt_p{plugin_name} in
    the <code>$HOME/.local/lib/babeltrace2/plugins</code> directory,
    without recursing.
@param[in] find_in_sys_dir
    #BT_TRUE to try to find the plugin named \bt_p{plugin_name} in
    the system \bt_name plugin directory.
@param[in] find_in_static
    #BT_TRUE to try to find the plugin named \bt_p{plugin_name} in the
    static plugins.
@param[in] fail_on_load_error
    #BT_TRUE to make this function return #BT_PLUGIN_FIND_STATUS_ERROR
    on any plugin loading error instead of ignoring it.
@param[out] plugin
    <strong>On success</strong>, \bt_p{*plugin} is a new plugin
    reference of named \bt_p{plugin_name}.

@retval #BT_PLUGIN_FIND_STATUS_OK
    Success.
@retval #BT_PLUGIN_FIND_STATUS_NOT_FOUND
    Plugin not found.
@retval #BT_PLUGIN_FIND_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_PLUGIN_FIND_STATUS_ERROR
    Error.

@bt_pre_not_null{plugin_name}
@pre
    At least one of the \bt_p{find_in_std_env_var},
    \bt_p{find_in_user_dir}, \bt_p{find_in_sys_dir}, and
    \bt_p{find_in_static} parameters is #BT_TRUE.
@bt_pre_not_null{plugin}

@sa bt_plugin_find_all() &mdash;
    Finds and loads all plugins from the default plugin search
    directories and static plugins.
*/
extern bt_plugin_find_status bt_plugin_find(const char *plugin_name,
		bt_bool find_in_std_env_var, bt_bool find_in_user_dir,
		bt_bool find_in_sys_dir, bt_bool find_in_static,
		bt_bool fail_on_load_error, const bt_plugin **plugin)
		__BT_NOEXCEPT;

/*!
@brief
    Status codes for bt_plugin_find_all().
*/
typedef enum bt_plugin_find_all_status {
	/*!
	@brief
	    Success.
	*/
	BT_PLUGIN_FIND_ALL_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    No plugins found.
	*/
	BT_PLUGIN_FIND_ALL_STATUS_NOT_FOUND	= __BT_FUNC_STATUS_NOT_FOUND,

	/*!
	@brief
	    Out of memory.
	*/
	BT_PLUGIN_FIND_ALL_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    Error.
	*/
	BT_PLUGIN_FIND_ALL_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
} bt_plugin_find_all_status;

/*!
@brief
    Finds and loads all the plugins from the default
    plugin search directories and static plugins, setting
    \bt_p{*plugins} to the result.

This function returns all the plugins within, in order:

-# <strong>If the \bt_p{find_in_std_env_var} parameter is
   #BT_TRUE</strong>,
   the colon-separated (or semicolon-separated on Windows) list of
   directories in the \c BABELTRACE_PLUGIN_PATH environment variable,
   if it's set.

   The function searches each directory in this list, without recursing.

-# <strong>If the \bt_p{find_in_user_dir} parameter is
   #BT_TRUE</strong>, <code>$HOME/.local/lib/babeltrace2/plugins</code>,
   without recursing.

-# <strong>If the \bt_p{find_in_sys_dir} is #BT_TRUE</strong>, the
   system \bt_name plugin directory, typically
   <code>/usr/lib/babeltrace2/plugins</code> or
   <code>/usr/local/lib/babeltrace2/plugins</code> on Linux, without
   recursing.

-# <strong>If the \bt_p{find_in_static} is #BT_TRUE</strong>,
   the static plugins.

During the search process, if a found plugin shares the name of an
already loaded plugin, this function ignores it and continues.

If this function finds a file which looks like a plugin (shared object
file or Python file with the \c bt_plugin_ prefix), but it fails to load
it for any reason, the function:

<dl>
  <dt>If \bt_p{fail_on_load_error} is #BT_TRUE</dt>
  <dd>Returns #BT_PLUGIN_FIND_ALL_STATUS_ERROR.</dd>

  <dt>If \bt_p{fail_on_load_error} is #BT_FALSE</dt>
  <dd>Ignores the loading error and continues searching.</dd>
</dl>

If this function doesn't find any plugin, then it returns
#BT_PLUGIN_FIND_ALL_STATUS_NOT_FOUND and does \em not set
\bt_p{*plugins}.

@param[in] find_in_std_env_var
    #BT_TRUE to try to find all the plugins in the
    colon-separated (or semicolon-separated on Windows) list of
    directories in the \c BABELTRACE_PLUGIN_PATH environment variable.
@param[in] find_in_user_dir
    #BT_TRUE to try to find all the plugins in
    the <code>$HOME/.local/lib/babeltrace2/plugins</code> directory,
    without recursing.
@param[in] find_in_sys_dir
    #BT_TRUE to try to find all the plugins in the system \bt_name
    plugin directory.
@param[in] find_in_static
    #BT_TRUE to try to find all the plugins in the static plugins.
@param[in] fail_on_load_error
    #BT_TRUE to make this function return
    #BT_PLUGIN_FIND_ALL_STATUS_ERROR on any plugin loading error instead
    of ignoring it.
@param[out] plugins
    <strong>On success</strong>, \bt_p{*plugins} is a new plugin set
    reference which contains all the plugins found from the default
    plugin search directories and static plugins.

@retval #BT_PLUGIN_FIND_ALL_STATUS_OK
    Success.
@retval #BT_PLUGIN_FIND_ALL_STATUS_NOT_FOUND
    No plugins found.
@retval #BT_PLUGIN_FIND_ALL_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_PLUGIN_FIND_ALL_STATUS_ERROR
    Error.

@pre
    At least one of the \bt_p{find_in_std_env_var},
    \bt_p{find_in_user_dir}, \bt_p{find_in_sys_dir}, and
    \bt_p{find_in_static} parameters is #BT_TRUE.
@bt_pre_not_null{plugins}

@sa bt_plugin_find() &mdash;
    Finds and loads a single plugin by name from the default plugin search
    directories and static plugins.
*/
bt_plugin_find_all_status bt_plugin_find_all(bt_bool find_in_std_env_var,
		bt_bool find_in_user_dir, bt_bool find_in_sys_dir,
		bt_bool find_in_static, bt_bool fail_on_load_error,
		const bt_plugin_set **plugins) __BT_NOEXCEPT;

/*!
@brief
    Status codes for bt_plugin_find_all_from_file().
*/
typedef enum bt_plugin_find_all_from_file_status {
	/*!
	@brief
	    Success.
	*/
	BT_PLUGIN_FIND_ALL_FROM_FILE_STATUS_OK			= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    No plugins found.
	*/
	BT_PLUGIN_FIND_ALL_FROM_FILE_STATUS_NOT_FOUND		= __BT_FUNC_STATUS_NOT_FOUND,

	/*!
	@brief
	    Out of memory.
	*/
	BT_PLUGIN_FIND_ALL_FROM_FILE_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    Error.
	*/
	BT_PLUGIN_FIND_ALL_FROM_FILE_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
} bt_plugin_find_all_from_file_status;

/*!
@brief
    Finds and loads all the plugins from the file with path \bt_p{path},
    setting \bt_p{*plugins} to the result.

@note
    The name of a plugin isn't related to the name of its file (shared
    object or Python file). For example, a plugin found in the file
    \c patente.so can be named <code>Dan</code>.

If any plugin loading error occurs during the execution of this
function, then it:

<dl>
  <dt>If \bt_p{fail_on_load_error} is #BT_TRUE</dt>
  <dd>Returns #BT_PLUGIN_FIND_ALL_FROM_FILE_STATUS_ERROR.</dd>

  <dt>If \bt_p{fail_on_load_error} is #BT_FALSE</dt>
  <dd>Ignores the loading error and continues.</dd>
</dl>

If this function doesn't find any plugin, then it returns
#BT_PLUGIN_FIND_ALL_FROM_FILE_STATUS_NOT_FOUND and does \em not set
\bt_p{*plugins}.

@param[in] path
    Path of the file in which to find and load all the plugins.
@param[in] fail_on_load_error
    #BT_TRUE to make this function return
    #BT_PLUGIN_FIND_ALL_FROM_FILE_STATUS_ERROR on any plugin loading
    error instead of ignoring it.
@param[out] plugins
    <strong>On success</strong>, \bt_p{*plugins} is a new plugin set
    reference which contains all the plugins found in the file with path
    \bt_p{path}.

@retval #BT_PLUGIN_FIND_ALL_FROM_FILE_STATUS_OK
    Success.
@retval #BT_PLUGIN_FIND_ALL_FROM_FILE_STATUS_NOT_FOUND
    No plugins found.
@retval #BT_PLUGIN_FIND_ALL_FROM_FILE_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_PLUGIN_FIND_ALL_FROM_FILE_STATUS_ERROR
    Error.

@bt_pre_not_null{path}
@pre
    \bt_p{path} is the path of a regular file.
@bt_pre_not_null{plugins}

@sa bt_plugin_find_all_from_dir() &mdash;
    Finds and loads all plugins from a given directory.
*/
extern bt_plugin_find_all_from_file_status bt_plugin_find_all_from_file(
		const char *path, bt_bool fail_on_load_error,
		const bt_plugin_set **plugins) __BT_NOEXCEPT;

/*!
@brief
    Status codes for bt_plugin_find_all_from_dir().
*/
typedef enum bt_plugin_find_all_from_dir_status {
	/*!
	@brief
	    Success.
	*/
	BT_PLUGIN_FIND_ALL_FROM_DIR_STATUS_OK			= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    No plugins found.
	*/
	BT_PLUGIN_FIND_ALL_FROM_DIR_STATUS_NOT_FOUND		= __BT_FUNC_STATUS_NOT_FOUND,

	/*!
	@brief
	    Out of memory.
	*/
	BT_PLUGIN_FIND_ALL_FROM_DIR_STATUS_MEMORY_ERROR		= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    Error.
	*/
	BT_PLUGIN_FIND_ALL_FROM_DIR_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
} bt_plugin_find_all_from_dir_status;

/*!
@brief
    Finds and loads all the plugins from the directory with path
    \bt_p{path}, setting \bt_p{*plugins} to the result.

If \bt_p{recurse} is #BT_TRUE, then this function recurses into the
subdirectories of \bt_p{path} to find plugins.

During the search process, if a found plugin shares the name of an
already loaded plugin, this function ignores it and continues.

@attention
    As of \bt_name_version_min_maj, the file and directory traversal
    order is undefined.

If any plugin loading error occurs during the execution of this
function, then it:

<dl>
  <dt>If \bt_p{fail_on_load_error} is #BT_TRUE</dt>
  <dd>Returns #BT_PLUGIN_FIND_ALL_FROM_DIR_STATUS_ERROR.</dd>

  <dt>If \bt_p{fail_on_load_error} is #BT_FALSE</dt>
  <dd>Ignores the loading error and continues.</dd>
</dl>

If this function doesn't find any plugin, then it returns
#BT_PLUGIN_FIND_ALL_FROM_DIR_STATUS_NOT_FOUND and does \em not set
\bt_p{*plugins}.

@param[in] path
    Path of the directory in which to find and load all the plugins.
@param[in] recurse
    #BT_TRUE to make this function recurse into the subdirectories
    of \bt_p{path}.
@param[in] fail_on_load_error
    #BT_TRUE to make this function return
    #BT_PLUGIN_FIND_ALL_FROM_DIR_STATUS_ERROR on any plugin loading
    error instead of ignoring it.
@param[out] plugins
    <strong>On success</strong>, \bt_p{*plugins} is a new plugin set
    reference which contains all the plugins found in the directory with
    path \bt_p{path}.

@retval #BT_PLUGIN_FIND_ALL_FROM_DIR_STATUS_OK
    Success.
@retval #BT_PLUGIN_FIND_ALL_FROM_DIR_STATUS_NOT_FOUND
    No plugins found.
@retval #BT_PLUGIN_FIND_ALL_FROM_DIR_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_PLUGIN_FIND_ALL_FROM_DIR_STATUS_ERROR
    Error.

@bt_pre_not_null{path}
@pre
    \bt_p{path} is the path of a directory.
@bt_pre_not_null{plugins}

@sa bt_plugin_find_all_from_file() &mdash;
    Finds and loads all plugins from a given file.
*/
extern bt_plugin_find_all_from_dir_status bt_plugin_find_all_from_dir(
		const char *path, bt_bool recurse, bt_bool fail_on_load_error,
		const bt_plugin_set **plugins) __BT_NOEXCEPT;

/*!
@brief
    Status codes for bt_plugin_find_all_from_static().
*/
typedef enum bt_plugin_find_all_from_static_status {
	/*!
	@brief
	    Success.
	*/
	BT_PLUGIN_FIND_ALL_FROM_STATIC_STATUS_OK			= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    No static plugins found.
	*/
	BT_PLUGIN_FIND_ALL_FROM_STATIC_STATUS_NOT_FOUND			= __BT_FUNC_STATUS_NOT_FOUND,

	/*!
	@brief
	    Out of memory.
	*/
	BT_PLUGIN_FIND_ALL_FROM_STATIC_STATUS_MEMORY_ERROR		= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    Error.
	*/
	BT_PLUGIN_FIND_ALL_FROM_STATIC_STATUS_ERROR			= __BT_FUNC_STATUS_ERROR,
} bt_plugin_find_all_from_static_status;

/*!
@brief
    Finds and loads all the static plugins,
    setting \bt_p{*plugins} to the result.

A static plugin is built directly into the application or library
instead of being a separate shared object file.

If any plugin loading error occurs during the execution of this
function, then it:

<dl>
  <dt>If \bt_p{fail_on_load_error} is #BT_TRUE</dt>
  <dd>Returns #BT_PLUGIN_FIND_ALL_FROM_STATIC_STATUS_ERROR.</dd>

  <dt>If \bt_p{fail_on_load_error} is #BT_FALSE</dt>
  <dd>Ignores the loading error and continues.</dd>
</dl>

If this function doesn't find any plugin, then it returns
#BT_PLUGIN_FIND_ALL_FROM_STATIC_STATUS_NOT_FOUND and does \em not set
\bt_p{*plugins}.

@param[in] fail_on_load_error
    #BT_TRUE to make this function return
    #BT_PLUGIN_FIND_ALL_FROM_STATIC_STATUS_ERROR on any plugin loading
    error instead of ignoring it.
@param[out] plugins
    <strong>On success</strong>, \bt_p{*plugins} is a new plugin set
    reference which contains all the static plugins.

@retval #BT_PLUGIN_FIND_ALL_FROM_STATIC_STATUS_OK
    Success.
@retval #BT_PLUGIN_FIND_ALL_FROM_STATIC_STATUS_NOT_FOUND
    No static plugins found.
@retval #BT_PLUGIN_FIND_ALL_FROM_STATIC_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_PLUGIN_FIND_ALL_FROM_STATIC_STATUS_ERROR
    Error.

@bt_pre_not_null{plugins}
*/
extern bt_plugin_find_all_from_static_status bt_plugin_find_all_from_static(
		bt_bool fail_on_load_error, const bt_plugin_set **plugins)
		__BT_NOEXCEPT;

/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_PLUGIN_PLUGIN_LOADING_H */
