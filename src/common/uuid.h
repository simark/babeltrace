/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2019 Michael Jeanson <mjeanson@efficios.com>
 */

#ifndef BABELTRACE_COMMON_UUID_H
#define BABELTRACE_COMMON_UUID_H

/*!
@file

@brief
    UUID API (C).

@ingroup common-c

@code{.c}
#include "common/uuid.h"
@endcode

@deprecated
    For new C++ code, use cpp-common/bt2c/uuid.hpp.

This file offers a C API to parse and generate
<a href="https://en.wikipedia.org/wiki/Universally_unique_identifier">UUIDs</a>.

The type #bt_uuid_t is a simple array of #BT_UUID_LEN bytes to contain
a UUID.

Generate a new UUID with bt_uuid_generate().

Parse an existing UUID with bt_uuid_from_str() and bt_uuid_from_c_str().

Convert a UUID to its textual representation with bt_uuid_to_str().

Use #BT_UUID_FMT and BT_UUID_FMT_VALUES() to format a UUID with a
\bt_ext_man{printf,3,3}-style function.

Compare UUIDs with bt_uuid_compare().

Copy a UUID with bt_uuid_copy().
*/

#include <inttypes.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
@brief
    Length of the textual representation of a UUID, excluding a
    terminating null character.
*/
#define BT_UUID_STR_LEN 36 /* Excludes final \0 */

/*!
@brief
    Length (number of bytes) of a binary UUID.
*/
#define BT_UUID_LEN 16

/*!
@brief
    Binary UUID type.
*/
typedef uint8_t bt_uuid_t[BT_UUID_LEN];

/*!
@brief
    Partial \bt_ext_man{printf,3,3}-style format string to format
    a binary UUID.

Use this in combination with BT_UUID_FMT_VALUES(), for example:

@code{.c}
printf("Hey %s, here's my UUID: " BT_UUID_FMT,
       name, BT_UUID_FMT_VALUES(my_uuid));
@endcode
*/
#define BT_UUID_FMT \
	"%02" SCNx8 "%02" SCNx8 "%02" SCNx8 "%02" SCNx8 "-%02" SCNx8 \
	"%02" SCNx8 "-%02" SCNx8 "%02" SCNx8 "-%02" SCNx8 "%02" SCNx8 \
	"-%02" SCNx8 "%02" SCNx8 "%02" SCNx8 "%02" SCNx8 "%02" SCNx8 \
	"%02" SCNx8

/*!
@brief
    Expands to #BT_UUID_LEN individual arguments, the #BT_UUID_LEN bytes
    of the UUID \bt_p{uuid} (type #bt_uuid_t).

See #BT_UUID_FMT for usage.
*/
#define BT_UUID_FMT_VALUES(uuid) \
	(uuid)[0], (uuid)[1], (uuid)[2], (uuid)[3], (uuid)[4], (uuid)[5], \
	(uuid)[6], (uuid)[7], (uuid)[8], (uuid)[9], (uuid)[10], (uuid)[11], \
	(uuid)[12], (uuid)[13], (uuid)[14], (uuid)[15]

/*!
@brief
    Generates a new UUID, writing it to \bt_p{uuid_out}.

@param[out] uuid_out
    Where to generate the new UUID.

@bt_pre_not_null{uuid_out}
*/
void bt_uuid_generate(bt_uuid_t uuid_out);

/*!
@brief
    Writes the textual representation of \bt_p{uuid_in} to
    \bt_p{*str_out}.

@param[in] uuid_in
    UUID of which to write the textual representation to
    \bt_p{*str_out}.
@param[out] str_out
    After calling this function, \bt_p{*str_out} contains the
    textual representation of \bt_p{uuid_in}.

@pre
    - \bt_p{uuid_in} is \em not \c NULL.
    - \bt_p{str_out} is \em not \c NULL.
    - \bt_p{str_out} points to a buffer of at least
      #BT_UUID_STR_LEN&nbsp;+&nbsp;1 bytes.

@sa #BT_UUID_FMT and BT_UUID_FMT_VALUES()
*/
void bt_uuid_to_str(const bt_uuid_t uuid_in, char *str_out);

/*!
@brief
    Decodes the C string \bt_p{str} containing the textual
    representation of a UUID into \bt_p{uuid_out}.

@param[in] str
    C string containing the textual
    representation of the UUID.
@param[out] uuid_out
    Where to write the UUID represented by \bt_p{str}.

@returns
    0 on success, or -1 if \bt_p{str} doesn't contain a valid textual
    representation of UUID.

@pre
    - \bt_p{str} is \em not \c NULL.
    - \bt_p{uuid_out} is \em not \c NULL.

@sa bt_uuid_from_str()
*/
int bt_uuid_from_c_str(const char *str, bt_uuid_t uuid_out);

/*!
@brief
    Decodes the string between \bt_p{begin} and \bt_p{end} (excluded)
    containing the textual
    representation of a UUID into \bt_p{uuid_out}.

@param[in] begin
    Beginning of string containing the textual
    representation of the UUID.
@param[in] end
    End of string (excluded) containing the textual
    representation of the UUID.
@param[out] uuid_out
    Where to write the UUID represented by the string between
    \bt_p{begin} and \bt_p{end}.

@returns
    0 on success, or -1 if the string between \bt_p{begin} and
    \bt_p{end} doesn't contain a valid textual representation of UUID.

@pre
    - \bt_p{begin} is \em not \c NULL.
    - \bt_p{end} is \em not \c NULL.
    - \bt_p{begin}&nbsp;&le;&nbsp;\bt_p{end}.
    - \bt_p{uuid_out} is \em not \c NULL.

@sa bt_uuid_from_c_str()
*/
int bt_uuid_from_str(const char *begin, const char *end, bt_uuid_t uuid_out);

/*!
@brief
    Compares the UUIDs \bt_p{uuid_a} and \bt_p{uuid_b}.

@param[in] uuid_a
    First UUID.
@param[in] uuid_b
    Second UUID.

@retval negative
    \bt_p{uuid_a} is less than \bt_p{uuid_b}.
@retval 0
    \bt_p{uuid_a} is equal to \bt_p{uuid_b}.
@retval positive
    \bt_p{uuid_a} is greater than \bt_p{uuid_b}.

@pre
    - \bt_p{uuid_a} is \em not \c NULL.
    - \bt_p{uuid_b} is \em not \c NULL.
*/
int bt_uuid_compare(const bt_uuid_t uuid_a, const bt_uuid_t uuid_b);

/*!
@brief
    Copies the UUID \bt_p{uuid_src} into \bt_p{uuid_dest}.

@param[out] uuid_dest
    Destination UUID.
@param[in] uuid_src
    Source UUID.

@pre
    - \bt_p{uuid_dest} is \em not \c NULL.
    - \bt_p{uuid_src} is \em not \c NULL.
*/
void bt_uuid_copy(bt_uuid_t uuid_dest, const bt_uuid_t uuid_src);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_COMMON_UUID_H */
