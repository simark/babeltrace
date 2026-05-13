/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2026 EfficiOS Inc. and Linux Foundation
 */

#ifndef BABELTRACE2_PLUGIN_PLUGIN_PROVIDER_INFO_H
#define BABELTRACE2_PLUGIN_PLUGIN_PROVIDER_INFO_H

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
@defgroup api-plugin-provider-info Plugin provider information

@brief
    Information about a plugin provider.

Plugin providers load libbabeltrace2 plugins.

A plugin provider information is a
\ref api-fund-shared-object "shared object": get a new reference with
bt_plugin_provider_info_get_ref() and put an existing reference with
bt_plugin_provider_info_put_ref().

Query the number of plugin providers currently loaded by libbabeltrace2
with bt_get_plugin_provider_count(). Query the information about a
specific plugin provider using bt_borrow_plugin_provider_info_by_index().

See \ref api-plugin-provider-loading-def-dirs "Automatic plugin provider loading"
to learn how libbabeltrace2 automatically loads plugin providers.

<h1>Plugin provider information properties</h1>

A plugin provider information has the following properties:

<dl>
  <dt>
    \anchor api-plugin-provider-info-prop-name
    Name
  </dt>
  <dd>
    Name of the plugin provider.

    The name of a plugin provider is not related to its file name. For
    example, a plugin provider found in the file \c fabienk.so can
    be named <code>Angine</code>.

    A plugin provider has a mandatory name property.

    Use bt_plugin_provider_info_get_name().
  </dd>

  <dt>
    \anchor api-plugin-provider-info-prop-descr
    \bt_dt_opt Description
  </dt>
  <dd>
    Description of the plugin provider.

    Use bt_plugin_provider_info_get_description().
  </dd>

  <dt>
    \anchor api-plugin-provider-info-prop-author
    \bt_dt_opt Author name(s)
  </dt>
  <dd>
    Name(s) of the plugin provider's author(s).

    Use bt_plugin_provider_info_get_author().
  </dd>

  <dt>
    \anchor api-plugin-provider-info-prop-license
    \bt_dt_opt License
  </dt>
  <dd>
    License or license name of the plugin provider.

    Use bt_plugin_provider_info_get_license().
  </dd>

  <dt>
    \anchor api-plugin-provider-info-prop-path
    \bt_dt_opt Path
  </dt>
  <dd>
    Path of the file which contains the plugin provider.

    A static plugin provider has no path property.

    Use bt_plugin_provider_info_get_path().
  </dd>

  <dt>
    \anchor api-plugin-provider-info-prop-version
    \bt_dt_opt Version
  </dt>
  <dd>
    Version of the plugin provider (major, minor, patch, and extra
    information).

    The version of a plugin provider is completely user-defined: the library
    does not use this property in any way to verify the compatibility of the
    plugin provider.

    Use bt_plugin_provider_info_get_version().
  </dd>
</dl>
*/

/*! @{ */

/*!
@name Type
@{

@typedef struct bt_plugin_provider_info bt_plugin_provider_info;

@brief
    Plugin provider information.

@}
*/

/*!
@name Plugin provider information query
@{
*/

/*!
@brief
    Get the number of plugin providers loaded by libbabeltrace2.

This function attempts to load plugin providers when libbabeltrace2
didn't already do so.

@returns
    The number of plugin providers loaded by libbabeltrace2.
*/
extern uint64_t bt_get_plugin_provider_count(void) __BT_NOEXCEPT;

/*!
@brief
    Borrows the information from the plugin provider at index
    \bt_p{index} from the set of plugin providers loaded by
    libbabeltrace2.

This function attempts to load plugin providers when libbabeltrace2
didn't already do so.

@param[in] index
    Index of the plugin provider to borrow the information from.

@returns
    @parblock
    \em Borrowed reference of the information of the plugin provider
    at index \bt_p{index}.

    The returned pointer remains valid until libbabeltrace2 is unloaded.
    @endparblock

@pre
    \bt_p{index} is less than the number of loaded plugin providers, as
    returned by bt_get_plugin_provider_count().
*/
extern const bt_plugin_provider_info *
bt_borrow_plugin_provider_info_by_index(uint64_t index) __BT_NOEXCEPT;

/*! @} */

/*!
@name Plugin provider information properties
@{
*/

/*!
@brief
    Returns the name property of \bt_p{plugin_provider_info}.

See the \ref api-plugin-provider-info-prop-name "name" property.

@param[in] plugin_provider_info
    Plugin provider information of which to get the name property.

@returns
    @parblock
    Name property of \bt_p{plugin_provider_info}.

    The returned pointer remains valid as long as
    \bt_p{plugin_provider_info} exists.
    @endparblock

@bt_pre_not_null{plugin_provider_info}
*/
extern const char *bt_plugin_provider_info_get_name(
		const bt_plugin_provider_info *plugin_provider_info)
		__BT_NOEXCEPT;

/*!
@brief
    Returns the description property of \bt_p{plugin_provider_info}.

See the \ref api-plugin-provider-info-prop-descr "description" property.

@param[in] plugin_provider_info
    Plugin provider information of which to get the description property.

@returns
    @parblock
    Description property of \bt_p{plugin_provider_info}, or \c NULL if
    not available.

    The returned pointer remains valid as long as
    \bt_p{plugin_provider_info} exists.
    @endparblock

@bt_pre_not_null{plugin_provider_info}
*/

extern const char *bt_plugin_provider_info_get_description(
		const bt_plugin_provider_info *plugin_provider_info)
		__BT_NOEXCEPT;

/*!
@brief
    Returns the name(s) of the author(s) contained in
    \bt_p{plugin_provider_info}.

See the \ref api-plugin-provider-info-prop-author "author name(s)"
property.

@param[in] plugin_provider_info
    Plugin provider information of which to get the author name(s)
    property.

@returns
    @parblock
    Author name(s) contained in \bt_p{plugin_provider_info}, or \c NULL
    if not available.

    The returned pointer remains valid as long as
    \bt_p{plugin_provider_info} exists.
    @endparblock

@bt_pre_not_null{plugin_provider_info}
*/
extern const char *bt_plugin_provider_info_get_author(
		const bt_plugin_provider_info *plugin_provider_info)
		__BT_NOEXCEPT;

/*!
@brief
    Returns the license text or the license name contained in
    \bt_p{plugin_provider_info}.

See the \ref api-plugin-provider-info-prop-license "license" property.

@param[in] plugin_provider_info
    Plugin provider information of which to get the license property.

@returns
    @parblock
    License contained in \bt_p{plugin_provider_info}, or \c NULL
    if not available.

    The returned pointer remains valid as long as
    \bt_p{plugin_provider_info} exists.
    @endparblock

@bt_pre_not_null{plugin_provider_info}
*/
extern const char *bt_plugin_provider_info_get_license(
		const bt_plugin_provider_info *plugin_provider_info)
		__BT_NOEXCEPT;

/*!
@brief
    Returns the path of the file which contains the plugin provider
    of \bt_p{plugin_provider_info}.

See the \ref api-plugin-provider-info-prop-path "path" property.

This function returns \c NULL if \bt_p{plugin_provider_info} is the
information of a static plugin provider because a static plugin provider
has no path property.

@param[in] plugin_provider_info
    Plugin provider information of which to get the containing file path.

@returns
    @parblock
    Path of the file which contains the plugin provider of
    \bt_p{plugin_provider_info}, or \c NULL if not available.

    The returned pointer remains valid as long as
    \bt_p{plugin_provider_info} exists.
    @endparblock

@bt_pre_not_null{plugin_provider_info}
*/
extern const char *bt_plugin_provider_info_get_path(
		const bt_plugin_provider_info *plugin_provider_info)
		__BT_NOEXCEPT;

/*!
@brief
    Returns the version of the plugin provider of
    \bt_p{plugin_provider_info}.

See the \ref api-plugin-provider-info-prop-version "version" property.

@param[in] plugin_provider_info
    Plugin provider information of which to get the version property.
@param[out] major
    <strong>If not \c NULL and this function returns
    #BT_PROPERTY_AVAILABILITY_AVAILABLE</strong>, \bt_p{*major} is the
    major version of the plugin provider of \bt_p{plugin_provider_info}.
@param[out] minor
    <strong>If not \c NULL and this function returns
    #BT_PROPERTY_AVAILABILITY_AVAILABLE</strong>, \bt_p{*minor} is the
    minor version of the plugin provider of \bt_p{plugin_provider_info}.
@param[out] patch
    <strong>If not \c NULL and this function returns
    #BT_PROPERTY_AVAILABILITY_AVAILABLE</strong>, \bt_p{*patch} is the
    patch version of the plugin provider of \bt_p{plugin_provider_info}.
@param[out] extra
    @parblock
    <strong>If not \c NULL and this function returns
    #BT_PROPERTY_AVAILABILITY_AVAILABLE</strong>, \bt_p{*extra} is the
    extra information of the version of the plugin provider of
    \bt_p{plugin_provider_info}.

    \bt_p{*extra} can be \c NULL if the version of the plugin provider has no
    extra information.

    \bt_p{*extra} remains valid as long as \bt_p{plugin_provider_info}
    exists.
    @endparblock

@retval #BT_PROPERTY_AVAILABILITY_AVAILABLE
    The version property of \bt_p{plugin_provider_info} is available.
@retval #BT_PROPERTY_AVAILABILITY_NOT_AVAILABLE
    The version property of \bt_p{plugin_provider_info} is not available.

@bt_pre_not_null{plugin_provider_info}
*/
extern bt_property_availability bt_plugin_provider_info_get_version(
		const bt_plugin_provider_info *plugin_provider_info,
		unsigned int *major, unsigned int *minor,
		unsigned int *patch, const char **extra) __BT_NOEXCEPT;

/*! @} */

/*!
@name Plugin provider information reference count
@{
*/

/*!
@brief
    Increments the \ref api-fund-shared-object "reference count" of
    the plugin provider information \bt_p{plugin_provider_info}.

@param[in] plugin_provider_info
    @parblock
    Plugin provider information of which to increment the reference
    count.

    Can be \c NULL.
    @endparblock

@sa bt_plugin_provider_info_put_ref() &mdash;
    Decrements the reference count of a plugin provider information.
*/
extern void bt_plugin_provider_info_get_ref(
		const bt_plugin_provider_info *plugin_provider_info)
		__BT_NOEXCEPT;

/*!
@brief
    Decrements the \ref api-fund-shared-object "reference count" of
    the plugin provider information \bt_p{plugin_provider_info}.

@param[in] plugin_provider_info
    @parblock
    Plugin provider information of which to decrement the reference
    count.

    Can be \c NULL.
    @endparblock

@sa bt_plugin_provider_info_get_ref() &mdash;
    Increments the reference count of a plugin provider information.
*/
extern void bt_plugin_provider_info_put_ref(
		const bt_plugin_provider_info *plugin_provider_info)
		__BT_NOEXCEPT;

/*!
@brief
    Decrements the reference count of the plugin provider information
    \bt_p{_plugin_provider_info}, and then sets
    \bt_p{_plugin_provider_info} to \c NULL.

@param _plugin_provider_info
    @parblock
    Plugin provider information of which to decrement the reference
    count.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_plugin_provider_info}
*/
#define BT_PLUGIN_PROVIDER_INFO_PUT_REF_AND_RESET(_plugin_provider_info)	\
	do {								\
		bt_plugin_provider_info_put_ref(_plugin_provider_info);	\
		(_plugin_provider_info) = NULL;				\
	} while (0)

/*!
@brief
    Decrements the reference count of the plugin provider information
    \bt_p{_dst}, sets \bt_p{_dst} to \bt_p{_src}, and then sets
    \bt_p{_src} to \c NULL.

This macro effectively moves a plugin provider information reference
from the expression \bt_p{_src} to the expression \bt_p{_dst}, putting
the existing \bt_p{_dst} reference.

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
#define BT_PLUGIN_PROVIDER_INFO_MOVE_REF(_dst, _src)		\
	do {							\
		bt_plugin_provider_info_put_ref(_dst);		\
		(_dst) = (_src);				\
		(_src) = NULL;					\
	} while (0)

/*! @} */

/*! @} */


#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_PLUGIN_PLUGIN_PROVIDER_INFO_H */
