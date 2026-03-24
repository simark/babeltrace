/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2010-2023 EfficiOS Inc. and Linux Foundation
 */

#ifndef BABELTRACE2_PLUGIN_PLUGIN_H
#define BABELTRACE2_PLUGIN_PLUGIN_H

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
@defgroup api-plugin Plugin

@brief
    Package of component classes.

A <strong><em>plugin</em></strong> is a package of \bt_p_comp_cls:

@image html plugin.png "A plugin is a package of component classes."

There are three types of plugins:

<dl>
  <dt>Shared object plugin</dt>
  <dd>
    <code>.so</code> file on Unix systems;
    <code>.dll</code> file on Windows systems.

    See \ref api-plugin-dev to learn how to write a shared object plugin.
  </dd>

  <dt>Python&nbsp;3 plugin</dt>
  <dd>
      <code>.py</code> file which starts with the
      <code>bt_plugin_</code> prefix.
  </dd>

  <dt>Static plugin</dt>
  <dd>
      A plugin built directly into libbabeltrace2 or into the
      user application.
  </dd>
</dl>

libbabeltrace2 \ref api-plugin-loading "loads" shared object and
Python plugins. Those plugins need libbabeltrace2 in turn to create
and use \bt_name objects:

@image html linking.png "libbabeltrace2 loads plugins which need libbabeltrace2."

A plugin is a \ref api-fund-shared-object "shared object": get a new
reference with bt_plugin_get_ref() and put an existing reference with
bt_plugin_put_ref().

Get the number of \bt_comp_cls in a plugin with
bt_plugin_get_source_component_class_count(),
bt_plugin_get_filter_component_class_count(), and
bt_plugin_get_sink_component_class_count().

Borrow a \bt_comp_cls by index from a plugin with
bt_plugin_borrow_source_component_class_by_index_const(),
bt_plugin_borrow_filter_component_class_by_index_const(), and
bt_plugin_borrow_sink_component_class_by_index_const().

Borrow a \bt_comp_cls by name from a plugin with
bt_plugin_borrow_source_component_class_by_name_const(),
bt_plugin_borrow_filter_component_class_by_name_const(), and
bt_plugin_borrow_sink_component_class_by_name_const().

<h1>Properties</h1>

A plugin has the following properties:

<dl>
  <dt>
    \anchor api-plugin-prop-name
    Name
  </dt>
  <dd>
    Name of the plugin.

    The name of the plugin isn't related to its file name. For example,
    a plugin found in the file \c patente.so can be named
    <code>Dan</code>.

    Use bt_plugin_get_name().
  </dd>

  <dt>
    \anchor api-plugin-prop-descr
    \bt_dt_opt Description
  </dt>
  <dd>
    Description of the plugin.

    Use bt_plugin_get_description().
  </dd>

  <dt>
    \anchor api-plugin-prop-author
    \bt_dt_opt Author name(s)
  </dt>
  <dd>
    Name(s) of the author(s) of the plugin.

    Use bt_plugin_get_author().
  </dd>

  <dt>
    \anchor api-plugin-prop-license
    \bt_dt_opt License
  </dt>
  <dd>
    License or license name of the plugin.

    Use bt_plugin_get_license().
  </dd>

  <dt>
    \anchor api-plugin-prop-path
    \bt_dt_opt Path
  </dt>
  <dd>
    Path of the file which contains the plugin.

    A static plugin has no path property.

    Use bt_plugin_get_path().
  </dd>

  <dt>
    \anchor api-plugin-prop-version
    \bt_dt_opt Version
  </dt>
  <dd>
    Version of the plugin (major, minor, patch, and extra information).

    The version of the plugin is completely user-defined: the library
    doesn't use this property in any way to verify the compatibility
    of the plugin.

    Use bt_plugin_get_version().
  </dd>
</dl>
*/

/*! @{ */

/*!
@name Type
@{

@typedef struct bt_plugin bt_plugin;

@brief
    Plugin.

@}
*/

/*!
@name Plugin properties
@{
*/

/*!
@brief
    Returns the name of the plugin \bt_p{plugin}.

See the \ref api-plugin-prop-name "name" property.

@param[in] plugin
    Plugin of which to get the name.

@returns
    @parblock
    Name of \bt_p{plugin}.

    The returned pointer remains valid as long as \bt_p{plugin} exists.
    @endparblock

@bt_pre_not_null{plugin}
*/
extern const char *bt_plugin_get_name(const bt_plugin *plugin) __BT_NOEXCEPT;

/*!
@brief
    Returns the description of the plugin \bt_p{plugin}.

See the \ref api-plugin-prop-descr "description" property.

@param[in] plugin
    Plugin of which to get description.

@returns
    @parblock
    Description of \bt_p{plugin}, or \c NULL if not available.

    The returned pointer remains valid as long as \bt_p{plugin} exists.
    @endparblock

@bt_pre_not_null{plugin}
*/
extern const char *bt_plugin_get_description(const bt_plugin *plugin)
		__BT_NOEXCEPT;

/*!
@brief
    Returns the name(s) of the author(s) of the plugin \bt_p{plugin}.

See the \ref api-plugin-prop-author "author name(s)" property.

@param[in] plugin
    Plugin of which to get the author name(s).

@returns
    @parblock
    Author name(s) of \bt_p{plugin}, or \c NULL if not available.

    The returned pointer remains valid as long as \bt_p{plugin} exists.
    @endparblock

@bt_pre_not_null{plugin}
*/
extern const char *bt_plugin_get_author(const bt_plugin *plugin) __BT_NOEXCEPT;

/*!
@brief
    Returns the license text or the license name of the plugin
    \bt_p{plugin}.

See the \ref api-plugin-prop-license "license" property.

@param[in] plugin
    Plugin of which to get the license.

@returns
    @parblock
    License of \bt_p{plugin}, or \c NULL if not available.

    The returned pointer remains valid as long as \bt_p{plugin} exists.
    @endparblock

@bt_pre_not_null{plugin}
*/
extern const char *bt_plugin_get_license(const bt_plugin *plugin) __BT_NOEXCEPT;

/*!
@brief
    Returns the path of the file which contains the plugin
    \bt_p{plugin}.

See the \ref api-plugin-prop-path "path" property.

This function returns \c NULL if \bt_p{plugin} is a static plugin
because a static plugin has no path property.

@param[in] plugin
    Plugin of which to get the path of the containing file.

@returns
    @parblock
    Path of the file which contains \bt_p{plugin}, or \c NULL if
    not available.

    The returned pointer remains valid as long as \bt_p{plugin} exists.
    @endparblock

@bt_pre_not_null{plugin}
*/
extern const char *bt_plugin_get_path(const bt_plugin *plugin) __BT_NOEXCEPT;

/*!
@brief
    Returns the version of the plugin \bt_p{plugin}.

See the \ref api-plugin-prop-version "version" property.

@param[in] plugin
    Plugin of which to get the version.
@param[out] major
    <strong>If not \c NULL and this function returns
    #BT_PROPERTY_AVAILABILITY_AVAILABLE</strong>, \bt_p{*major} is the
    major version of \bt_p{plugin}.
@param[out] minor
    <strong>If not \c NULL and this function returns
    #BT_PROPERTY_AVAILABILITY_AVAILABLE</strong>, \bt_p{*minor} is the
    minor version of \bt_p{plugin}.
@param[out] patch
    <strong>If not \c NULL and this function returns
    #BT_PROPERTY_AVAILABILITY_AVAILABLE</strong>, \bt_p{*patch} is the
    patch version of \bt_p{plugin}.
@param[out] extra
    @parblock
    <strong>If not \c NULL and this function returns
    #BT_PROPERTY_AVAILABILITY_AVAILABLE</strong>, \bt_p{*extra} is the
    extra information of the version of \bt_p{plugin}.

    \bt_p{*extra} can be \c NULL if the version of the plugin has no
    extra information.

    \bt_p{*extra} remains valid as long as \bt_p{plugin} exists.
    @endparblock

@retval #BT_PROPERTY_AVAILABILITY_AVAILABLE
    The version of \bt_p{plugin} is available.
@retval #BT_PROPERTY_AVAILABILITY_NOT_AVAILABLE
    The version of \bt_p{plugin} isn't available.

@bt_pre_not_null{plugin}
*/
extern bt_property_availability bt_plugin_get_version(
		const bt_plugin *plugin, unsigned int *major,
		unsigned int *minor, unsigned int *patch, const char **extra)
		__BT_NOEXCEPT;

/*! @} */

/*!
@name Component class access
@{
*/

/*!
@brief
    Returns the number of source component classes contained in the
    plugin \bt_p{plugin}.

@param[in] plugin
    Plugin of which to get the number of contained source
    component classes.

@returns
    Number of contained source component classes in \bt_p{plugin}.

@bt_pre_not_null{plugin}
*/
extern uint64_t bt_plugin_get_source_component_class_count(
		const bt_plugin *plugin) __BT_NOEXCEPT;

/*!
@brief
    Returns the number of filter component classes contained in the
    plugin \bt_p{plugin}.

@param[in] plugin
    Plugin of which to get the number of contained filter
    component classes.

@returns
    Number of contained filter component classes in \bt_p{plugin}.

@bt_pre_not_null{plugin}
*/
extern uint64_t bt_plugin_get_filter_component_class_count(
		const bt_plugin *plugin) __BT_NOEXCEPT;

/*!
@brief
    Returns the number of sink component classes contained in the
    plugin \bt_p{plugin}.

@param[in] plugin
    Plugin of which to get the number of contained sink
    component classes.

@returns
    Number of contained sink component classes in \bt_p{plugin}.

@bt_pre_not_null{plugin}
*/
extern uint64_t bt_plugin_get_sink_component_class_count(
		const bt_plugin *plugin) __BT_NOEXCEPT;

/*!
@brief
    Borrows the source component class at index \bt_p{index} from the
    plugin \bt_p{plugin}.

@param[in] plugin
    Plugin from which to borrow the source component class at index
    \bt_p{index}.
@param[in] index
    Index of the source component class to borrow from \bt_p{plugin}.

@returns
    @parblock
    \em Borrowed reference of the source component class of
    \bt_p{plugin} at index \bt_p{index}.

    The returned pointer remains valid as long as \bt_p{plugin} exists.
    @endparblock

@bt_pre_not_null{plugin}
@pre
    \bt_p{index} is less than the number of source component classes in
    \bt_p{plugin} (as returned by
    bt_plugin_get_source_component_class_count()).

@sa bt_plugin_borrow_source_component_class_by_name_const() &mdash;
    Borrows a source component class by name from a plugin.
*/
extern const bt_component_class_source *
bt_plugin_borrow_source_component_class_by_index_const(
		const bt_plugin *plugin, uint64_t index) __BT_NOEXCEPT;

/*!
@brief
    Borrows the filter component class at index \bt_p{index} from the
    plugin \bt_p{plugin}.

@param[in] plugin
    Plugin from which to borrow the filter component class at index
    \bt_p{index}.
@param[in] index
    Index of the filter component class to borrow from \bt_p{plugin}.

@returns
    @parblock
    \em Borrowed reference of the filter component class of
    \bt_p{plugin} at index \bt_p{index}.

    The returned pointer remains valid as long as \bt_p{plugin} exists.
    @endparblock

@bt_pre_not_null{plugin}
@pre
    \bt_p{index} is less than the number of filter component classes in
    \bt_p{plugin} (as returned by
    bt_plugin_get_filter_component_class_count()).

@sa bt_plugin_borrow_filter_component_class_by_name_const() &mdash;
    Borrows a filter component class by name from a plugin.
*/
extern const bt_component_class_filter *
bt_plugin_borrow_filter_component_class_by_index_const(
		const bt_plugin *plugin, uint64_t index) __BT_NOEXCEPT;

/*!
@brief
    Borrows the sink component class at index \bt_p{index} from the
    plugin \bt_p{plugin}.

@param[in] plugin
    Plugin from which to borrow the sink component class at index
    \bt_p{index}.
@param[in] index
    Index of the sink component class to borrow from \bt_p{plugin}.

@returns
    @parblock
    \em Borrowed reference of the sink component class of
    \bt_p{plugin} at index \bt_p{index}.

    The returned pointer remains valid as long as \bt_p{plugin} exists.
    @endparblock

@bt_pre_not_null{plugin}
@pre
    \bt_p{index} is less than the number of sink component classes in
    \bt_p{plugin} (as returned by
    bt_plugin_get_sink_component_class_count()).

@sa bt_plugin_borrow_sink_component_class_by_name_const() &mdash;
    Borrows a sink component class by name from a plugin.
*/
extern const bt_component_class_sink *
bt_plugin_borrow_sink_component_class_by_index_const(
		const bt_plugin *plugin, uint64_t index) __BT_NOEXCEPT;

/*!
@brief
    Borrows the source component class named \bt_p{name} from the
    plugin \bt_p{plugin}.

If no source component class has the name \bt_p{name} within
\bt_p{plugin}, then this function returns \c NULL.

@param[in] plugin
    Plugin from which to borrow the source component class named
    \bt_p{name}.
@param[in] name
    Name of the source component class to borrow from \bt_p{plugin}.

@returns
    @parblock
    \em Borrowed reference of the source component class of
    \bt_p{plugin} named \bt_p{name}, or \c NULL if no source component
    class is named \bt_p{name} within \bt_p{plugin}.

    The returned pointer remains valid as long as \bt_p{plugin} exists.
    @endparblock

@bt_pre_not_null{plugin}
@bt_pre_not_null{name}

@sa bt_plugin_borrow_source_component_class_by_index_const() &mdash;
    Borrows a source component class by index from a plugin.
*/
extern const bt_component_class_source *
bt_plugin_borrow_source_component_class_by_name_const(
		const bt_plugin *plugin, const char *name) __BT_NOEXCEPT;

/*!
@brief
    Borrows the filter component class named \bt_p{name} from the
    plugin \bt_p{plugin}.

If no filter component class has the name \bt_p{name} within
\bt_p{plugin}, then this function returns \c NULL.

@param[in] plugin
    Plugin from which to borrow the filter component class named
    \bt_p{name}.
@param[in] name
    Name of the filter component class to borrow from \bt_p{plugin}.

@returns
    @parblock
    \em Borrowed reference of the filter component class of
    \bt_p{plugin} named \bt_p{name}, or \c NULL if no filter component
    class is named \bt_p{name} within \bt_p{plugin}.

    The returned pointer remains valid as long as \bt_p{plugin} exists.
    @endparblock

@bt_pre_not_null{plugin}
@bt_pre_not_null{name}

@sa bt_plugin_borrow_filter_component_class_by_index_const() &mdash;
    Borrows a filter component class by index from a plugin.
*/
extern const bt_component_class_filter *
bt_plugin_borrow_filter_component_class_by_name_const(
		const bt_plugin *plugin, const char *name) __BT_NOEXCEPT;

/*!
@brief
    Borrows the sink component class named \bt_p{name} from the
    plugin \bt_p{plugin}.

If no sink component class has the name \bt_p{name} within
\bt_p{plugin}, then this function returns \c NULL.

@param[in] plugin
    Plugin from which to borrow the sink component class named
    \bt_p{name}.
@param[in] name
    Name of the sink component class to borrow from \bt_p{plugin}.

@returns
    @parblock
    \em Borrowed reference of the sink component class of
    \bt_p{plugin} named \bt_p{name}, or \c NULL if no sink component
    class is named \bt_p{name} within \bt_p{plugin}.

    The returned pointer remains valid as long as \bt_p{plugin} exists.
    @endparblock

@bt_pre_not_null{plugin}
@bt_pre_not_null{name}

@sa bt_plugin_borrow_sink_component_class_by_index_const() &mdash;
    Borrows a sink component class by index from a plugin.
*/
extern const bt_component_class_sink *
bt_plugin_borrow_sink_component_class_by_name_const(
		const bt_plugin *plugin, const char *name) __BT_NOEXCEPT;

/*! @} */

/*!
@name Plugin reference count
@{
*/

/*!
@brief
    Increments the \ref api-fund-shared-object "reference count" of
    the plugin \bt_p{plugin}.

@param[in] plugin
    @parblock
    Plugin of which to increment the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_plugin_put_ref() &mdash;
    Decrements the reference count of a plugin.
*/
extern void bt_plugin_get_ref(const bt_plugin *plugin) __BT_NOEXCEPT;

/*!
@brief
    Decrements the \ref api-fund-shared-object "reference count" of
    the plugin \bt_p{plugin}.

@param[in] plugin
    @parblock
    Plugin of which to decrement the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_plugin_get_ref() &mdash;
    Increments the reference count of a plugin.
*/
extern void bt_plugin_put_ref(const bt_plugin *plugin) __BT_NOEXCEPT;

/*!
@brief
    Decrements the reference count of the plugin \bt_p{_plugin}, and
    then sets \bt_p{_plugin} to \c NULL.

@param _plugin
    @parblock
    Plugin of which to decrement the reference count.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_plugin}
*/
#define BT_PLUGIN_PUT_REF_AND_RESET(_plugin)		\
	do {						\
		bt_plugin_put_ref(_plugin);		\
		(_plugin) = NULL;			\
	} while (0)

/*!
@brief
    Decrements the reference count of the plugin \bt_p{_dst}, sets
    \bt_p{_dst} to \bt_p{_src}, and then sets \bt_p{_src} to \c NULL.

This macro effectively moves a plugin reference from the expression
\bt_p{_src} to the expression \bt_p{_dst}, putting the existing
\bt_p{_dst} reference.

@param _dst
    @parblock
    Destination expression.

    Can contain \c NULL.
    @endparblock
@param _src
    @parblock
    Source expression.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_dst}
@bt_pre_assign_expr{_src}
*/
#define BT_PLUGIN_MOVE_REF(_dst, _src)		\
	do {					\
		bt_plugin_put_ref(_dst);	\
		(_dst) = (_src);		\
		(_src) = NULL;			\
	} while (0)

/*! @} */

/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_PLUGIN_PLUGIN_H */
