/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2010-2023 EfficiOS Inc. and Linux Foundation
 */

#ifndef BABELTRACE2_PLUGIN_PLUGIN_SET_H
#define BABELTRACE2_PLUGIN_PLUGIN_SET_H

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
@defgroup api-plugin-set Plugin set

@brief
    Set of plugins.

A <strong>plugin set</strong> contains zero or more \bt_p_plugin.

A plugin set is a \ref api-fund-shared-object "shared object": get a new
reference with bt_plugin_set_get_ref() and put an existing reference with
bt_plugin_set_put_ref().

Get the number of plugins in a plugin set with
bt_plugin_set_get_plugin_count().

Borrow a plugin from a plugin set with
bt_plugin_set_borrow_plugin_by_index_const().
*/

/*! @{ */

/*!
@name Type
@{

@typedef struct bt_plugin_set bt_plugin_set;

@brief
    Set of \bt_p_plugin.

@}
*/

/*!
@brief
    Returns the number of plugins contained in the
    plugin set \bt_p{plugin_set}.

@param[in] plugin_set
    Plugin set of which to get the number of contained plugins.

@returns
    Number of contained plugins in \bt_p{plugin_set}.

@bt_pre_not_null{plugin_set}
*/
extern uint64_t bt_plugin_set_get_plugin_count(
		const bt_plugin_set *plugin_set) __BT_NOEXCEPT;

/*!
@brief
    Borrows the plugin at index \bt_p{index} from the plugin set
    \bt_p{plugin_set}.

@param[in] plugin_set
    Plugin set from which to borrow the plugin at index \bt_p{index}.
@param[in] index
    Index of the plugin to borrow from \bt_p{plugin_set}.

@returns
    @parblock
    \em Borrowed reference of the plugin of \bt_p{plugin_set} at index
    \bt_p{index}.

    The returned pointer remains valid until \bt_p{plugin_set} is
    modified.
    @endparblock

@bt_pre_not_null{plugin_set}
@pre
    \bt_p{index} is less than the number of plugins in
    \bt_p{plugin_set} (as returned by bt_plugin_set_get_plugin_count()).
*/
extern const bt_plugin *bt_plugin_set_borrow_plugin_by_index_const(
		const bt_plugin_set *plugin_set, uint64_t index) __BT_NOEXCEPT;

/*!
@name Reference count
@{
*/

/*!
@brief
    Increments the \ref api-fund-shared-object "reference count" of
    the plugin set \bt_p{plugin_set}.

@param[in] plugin_set
    @parblock
    Plugin set of which to increment the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_plugin_set_put_ref() &mdash;
    Decrements the reference count of a plugin set.
*/
extern void bt_plugin_set_get_ref(const bt_plugin_set *plugin_set)
		__BT_NOEXCEPT;

/*!
@brief
    Decrements the \ref api-fund-shared-object "reference count" of
    the plugin set \bt_p{plugin_set}.

@param[in] plugin_set
    @parblock
    Plugin set of which to decrement the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_plugin_set_get_ref() &mdash;
    Increments the reference count of a plugin set.
*/
extern void bt_plugin_set_put_ref(const bt_plugin_set *plugin_set)
		__BT_NOEXCEPT;

/*!
@brief
    Decrements the reference count of the plugin set \bt_p{_plugin_set},
    and then sets \bt_p{_plugin_set} to \c NULL.

@param _plugin_set
    @parblock
    Plugin set of which to decrement the reference count.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_plugin_set}
*/
#define BT_PLUGIN_SET_PUT_REF_AND_RESET(_plugin_set)	\
	do {						\
		bt_plugin_set_put_ref(_plugin_set);	\
		(_plugin_set) = NULL;			\
	} while (0)

/*!
@brief
    Decrements the reference count of the plugin set \bt_p{_dst}, sets
    \bt_p{_dst} to \bt_p{_src}, and then sets \bt_p{_src} to \c NULL.

This macro effectively moves a plugin set reference from the expression
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
#define BT_PLUGIN_SET_MOVE_REF(_dst, _src)	\
	do {					\
		bt_plugin_set_put_ref(_dst);	\
		(_dst) = (_src);		\
		(_src) = NULL;			\
	} while (0)

/*! @} */

/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_PLUGIN_PLUGIN_SET_H */
