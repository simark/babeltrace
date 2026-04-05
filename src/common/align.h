/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2010 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 */

#ifndef BABELTRACE_COMMON_ALIGN_H
#define BABELTRACE_COMMON_ALIGN_H

/*!
@file

@brief
    Alignment macros.

@ingroup common-c

@code{.c}
#include "common/align.h"
@endcode

@deprecated
    For new C++ code, use bt2c::align().
*/

/*!
@brief
    Returns the \em next multiple of \bt_p{a} from \bt_p{x},
    (an integer variable), or \bt_p{x} if it's already a multiple
    of \bt_p{a}.

@pre
    - \bt_p{a} is greater than 0.
    - \bt_p{a} is a power of two.

@sa bt2c::align()
@sa BT_ALIGN_FLOOR()
*/
#define BT_ALIGN(x, a)		__BT_ALIGN_MASK(x, (__typeof__(x))(a) - 1)

#define __BT_ALIGN_MASK(x, mask)	(((x) + (mask)) & ~(mask))

/*!
@brief
    Returns the \em previous multiple of \bt_p{a} from \bt_p{x},
    (an integer variable), or \bt_p{x} if it's already a multiple
    of \bt_p{a}.

@pre
    - \bt_p{a} is greater than 0.
    - \bt_p{a} is a power of two.

@sa BT_ALIGN()
*/
#define BT_ALIGN_FLOOR(x, a)	__BT_ALIGN_FLOOR_MASK(x, (__typeof__(x)) (a) - 1)

#define __BT_ALIGN_FLOOR_MASK(x, mask)	((x) & ~(mask))

#endif /* BABELTRACE_COMMON_ALIGN_H */
