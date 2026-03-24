/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2018 EfficiOS Inc. and Linux Foundation
 * Copyright (c) 2018 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_COMMON_COMMON_H
#define BABELTRACE_COMMON_COMMON_H

#include <errno.h>
#include <glib.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <babeltrace2/babeltrace.h>

#define __BT_IN_BABELTRACE_H
#include <babeltrace2/func-status.h>
#undef __BT_IN_BABELTRACE_H

#include "common/assert.h"
#include "common/macros.h"
#include "common/safe.h"

#ifdef __cplusplus
extern "C" {
#endif

/*!
@addtogroup common-c-term
@{
*/

/*!
@brief
    Sets \bt_p{*width} and \bt_p{*height} to the current width and
    height of the connected terminal, if any.

@param[out] width
    On success, \bt_p{*width} is the number of visible text coloumns of
    the connected terminal.
@param[out] height
    On success, \bt_p{*height} is the number of visible text rows of the
    connected terminal.

@returns
    0 on success, or -1 on error (for example, if there's no connected
    terminal).

@pre
    - \bt_p{width} is \em not \c NULL.
    - \bt_p{height} is \em not \c NULL.
*/
int bt_common_get_term_size(unsigned int *width, unsigned int *height);

/*!
@brief
    Returns whether or not the connected terminal, if any, supports
    text formatting.

@retval false
    The connected terminal doesn't support text formatting or there's
    no connected terminal.
@retval true
    The connected terminal supports text formatting.
*/
bool bt_common_colors_supported(void);

/*!
@brief
    Returns the ANSI escape code to reset text formatting, or an
    empty string if the connected terminal, if any, doesn't support
    text formatting.

@returns
    ANSI escape code to reset text formatting, or an
    empty string if the connected terminal, if any, doesn't support
    text formatting.
*/
const char *bt_common_color_reset(void);

/*!
@brief
    Returns the ANSI escape code to enable bold text, or an
    empty string if the connected terminal, if any, doesn't support
    text formatting.

@returns
    ANSI escape code to enable bold text, or an
    empty string if the connected terminal, if any, doesn't support
    text formatting.
*/
const char *bt_common_color_bold(void);

/*!
@brief
    Returns the ANSI escape code to enable the default foreground
    colour, or an empty string if the connected terminal, if any,
    doesn't support text formatting.

@returns
    ANSI escape code to enable the default foreground colour, or an
    empty string if the connected terminal, if any, doesn't support text
    formatting.
*/
const char *bt_common_color_fg_default(void);

/*!
@brief
    Returns the ANSI escape code to enable red foreground, or an empty
    string if the connected terminal, if any, doesn't support text
    formatting.

@returns
    ANSI escape code to enable red foreground, or an empty string if
    the connected terminal, if any, doesn't support text formatting.
*/
const char *bt_common_color_fg_red(void);

/*!
@brief
    Returns the ANSI escape code to enable green foreground, or an empty
    string if the connected terminal, if any, doesn't support text
    formatting.

@returns
    ANSI escape code to enable green foreground, or an empty string if
    the connected terminal, if any, doesn't support text formatting.
*/
const char *bt_common_color_fg_green(void);

/*!
@brief
    Returns the ANSI escape code to enable yellow foreground, or an
    empty string if the connected terminal, if any, doesn't support text
    formatting.

@returns
    ANSI escape code to enable yellow foreground, or an empty string if
    the connected terminal, if any, doesn't support text formatting.
*/
const char *bt_common_color_fg_yellow(void);

/*!
@brief
    Returns the ANSI escape code to enable blue foreground, or an empty
    string if the connected terminal, if any, doesn't support text
    formatting.

@returns
    ANSI escape code to enable blue foreground, or an empty string if
    the connected terminal, if any, doesn't support text formatting.
*/
const char *bt_common_color_fg_blue(void);

/*!
@brief
    Returns the ANSI escape code to enable magenta foreground, or an
    empty string if the connected terminal, if any, doesn't support text
    formatting.

@returns
    ANSI escape code to enable magenta foreground, or an empty string if
    the connected terminal, if any, doesn't support text formatting.
*/
const char *bt_common_color_fg_magenta(void);

/*!
@brief
    Returns the ANSI escape code to enable cyan foreground, or an empty
    string if the connected terminal, if any, doesn't support text
    formatting.

@returns
    ANSI escape code to enable cyan foreground, or an empty string if
    the connected terminal, if any, doesn't support text formatting.
*/
const char *bt_common_color_fg_cyan(void);

/*!
@brief
    Returns the ANSI escape code to enable light gray foreground, or an
    empty string if the connected terminal, if any, doesn't support text
    formatting.

@returns
    ANSI escape code to enable light gray foreground, or an empty string
    if the connected terminal, if any, doesn't support text formatting.
*/
const char *bt_common_color_fg_light_gray(void);

/*!
@brief
    Returns the ANSI escape code to enable bright red foreground, or an
    empty string if the connected terminal, if any, doesn't support text
    formatting.

@returns
    ANSI escape code to enable bright red foreground, or an empty string
    if the connected terminal, if any, doesn't support text formatting.
*/
const char *bt_common_color_fg_bright_red(void);

/*!
@brief
    Returns the ANSI escape code to enable bright green foreground, or
    an empty string if the connected terminal, if any, doesn't support
    text formatting.

@returns
    ANSI escape code to enable bright green foreground, or an empty
    string if the connected terminal, if any, doesn't support text
    formatting.
*/
const char *bt_common_color_fg_bright_green(void);

/*!
@brief
    Returns the ANSI escape code to enable bright yellow foreground, or
    an empty string if the connected terminal, if any, doesn't support
    text formatting.

@returns
    ANSI escape code to enable bright yellow foreground, or an empty
    string if the connected terminal, if any, doesn't support text
    formatting.
*/
const char *bt_common_color_fg_bright_yellow(void);

/*!
@brief
    Returns the ANSI escape code to enable bright blue foreground, or an
    empty string if the connected terminal, if any, doesn't support text
    formatting.

@returns
    ANSI escape code to enable bright blue foreground, or an empty
    string if the connected terminal, if any, doesn't support text
    formatting.
*/
const char *bt_common_color_fg_bright_blue(void);

/*!
@brief
    Returns the ANSI escape code to enable bright magenta foreground, or
    an empty string if the connected terminal, if any, doesn't support
    text formatting.

@returns
    ANSI escape code to enable bright magenta foreground, or an empty
    string if the connected terminal, if any, doesn't support text
    formatting.
*/
const char *bt_common_color_fg_bright_magenta(void);

/*!
@brief
    Returns the ANSI escape code to enable bright cyan foreground, or an
    empty string if the connected terminal, if any, doesn't support text
    formatting.

@returns
    ANSI escape code to enable bright cyan foreground, or an empty
    string if the connected terminal, if any, doesn't support text
    formatting.
*/
const char *bt_common_color_fg_bright_cyan(void);

/*!
@brief
    Returns the ANSI escape code to enable bright light gray foreground,
    or an empty string if the connected terminal, if any, doesn't
    support text formatting.

@returns
    ANSI escape code to enable bright light gray foreground, or an empty
    string if the connected terminal, if any, doesn't support text
    formatting.
*/
const char *bt_common_color_fg_bright_light_gray(void);

/*!
@brief
    Returns the ANSI escape code to enable the default background color,
    or an empty string if the connected terminal, if any, doesn't
    support text formatting.

@returns
    ANSI escape code to enable the default background color, or an empty
    string if the connected terminal, if any, doesn't support text
    formatting.
*/
const char *bt_common_color_bg_default(void);

/*!
@brief
    Returns the ANSI escape code to enable red background, or an empty
    string if the connected terminal, if any, doesn't support text
    formatting.

@returns
    ANSI escape code to enable red background, or an empty string if the
    connected terminal, if any, doesn't support text formatting.
*/
const char *bt_common_color_bg_red(void);

/*!
@brief
    Returns the ANSI escape code to enable green background, or an empty
    string if the connected terminal, if any, doesn't support text
    formatting.

@returns
    ANSI escape code to enable green background, or an empty string if
    the connected terminal, if any, doesn't support text formatting.
*/
const char *bt_common_color_bg_green(void);

/*!
@brief
    Returns the ANSI escape code to enable yellow background, or an
    empty string if the connected terminal, if any, doesn't support text
    formatting.

@returns
    ANSI escape code to enable yellow background, or an empty string if
    the connected terminal, if any, doesn't support text formatting.
*/
const char *bt_common_color_bg_yellow(void);

/*!
@brief
    Returns the ANSI escape code to enable blue background, or an empty
    string if the connected terminal, if any, doesn't support text
    formatting.

@returns
    ANSI escape code to enable blue background, or an empty string if
    the connected terminal, if any, doesn't support text formatting.
*/
const char *bt_common_color_bg_blue(void);

/*!
@brief
    Returns the ANSI escape code to enable magenta background, or an
    empty string if the connected terminal, if any, doesn't support text
    formatting.

@returns
    ANSI escape code to enable magenta background, or an empty string if
    the connected terminal, if any, doesn't support text formatting.
*/
const char *bt_common_color_bg_magenta(void);

/*!
@brief
    Returns the ANSI escape code to enable cyan background, or an empty
    string if the connected terminal, if any, doesn't support text
    formatting.

@returns
    ANSI escape code to enable cyan background, or an empty string if
    the connected terminal, if any, doesn't support text formatting.
*/
const char *bt_common_color_bg_cyan(void);

/*!
@brief
    Returns the ANSI escape code to enable light gray background, or an
    empty string if the connected terminal, if any, doesn't support text
    formatting.

@returns
    ANSI escape code to enable light gray background, or an empty string
    if the connected terminal, if any, doesn't support text formatting.
*/
const char *bt_common_color_bg_light_gray(void);

/*!
@brief
    Type of the \bt_p{when} parameter of bt_common_color_get_codes().
*/
enum bt_common_color_when {
	/*!
	@brief
	    Only use text formatting when the connected
	    terminal supports it.
	*/
	BT_COMMON_COLOR_WHEN_AUTO,

	/*!
	@brief
	    Always use text formatting, even if the connected
	    terminal doesn't support it.
	*/
	BT_COMMON_COLOR_WHEN_ALWAYS,

	/*!
	@brief
	    Never use text formatting, even if the connected
	    terminal supports it.
	*/
	BT_COMMON_COLOR_WHEN_NEVER,
};

/*!
@brief
    Text formatting ANSI escape codes.

@sa bt_common_color_get_codes()
*/
struct bt_common_color_codes {
	/*! @brief Reset text formatting. */
	const char *reset;

	/*! @brief Enable bold text. */
	const char *bold;

	/*! @brief Enable default foreground color. */
	const char *fg_default;

	/*! @brief Enable red foreground. */
	const char *fg_red;

	/*! @brief Enable green foreground. */
	const char *fg_green;

	/*! @brief Enable yellow foreground. */
	const char *fg_yellow;

	/*! @brief Enable blue foreground. */
	const char *fg_blue;

	/*! @brief Enable magenta foreground. */
	const char *fg_magenta;

	/*! @brief Enable cyan foreground. */
	const char *fg_cyan;

	/*! @brief Enable light gray foreground. */
	const char *fg_light_gray;

	/*! @brief Enable bright red foreground. */
	const char *fg_bright_red;

	/*! @brief Enable bright green foreground. */
	const char *fg_bright_green;

	/*! @brief Enable bright yellow foreground. */
	const char *fg_bright_yellow;

	/*! @brief Enable bright blue foreground. */
	const char *fg_bright_blue;

	/*! @brief Enable bright magenta foreground. */
	const char *fg_bright_magenta;

	/*! @brief Enable bright cyan foreground. */
	const char *fg_bright_cyan;

	/*! @brief Enable bright light gray foreground. */
	const char *fg_bright_light_gray;

	/*! @brief Enable default background color. */
	const char *bg_default;

	/*! @brief Enable red background. */
	const char *bg_red;

	/*! @brief Enable green background. */
	const char *bg_green;

	/*! @brief Enable yellow background. */
	const char *bg_yellow;

	/*! @brief Enable blue background. */
	const char *bg_blue;

	/*! @brief Enable magenta background. */
	const char *bg_magenta;

	/*! @brief Enable cyan background. */
	const char *bg_cyan;

	/*! @brief Enable light gray background. */
	const char *bg_light_gray;
};

/*!
@brief
    Sets \bt_p{*codes} to the text formatting ANSI escape codes to use
    depending on \bt_p{when} and on the capabilities of the
    connected terminal, if any.

To use terminal text formatting:

-# Call this function
   to fill \bt_p{*codes} to the correct ANSI escape codes
   depending on the context (the \bt_p{when} parameter as well as
   whether or not the connected terminal, if any, supports text
   formatting).

   Example:

   @code{.c}
   struct bt_common_color_codes my_codes;

   bt_common_color_get_codes(&my_codes, BT_COMMON_COLOR_WHEN_AUTO);
   @endcode

   \c my_codes will contain empty strings, which are safe to use, if
   there's no text formatting support.

-# Use the ANSI escape codes of \bt_p{*codes} when
   writing to the standard output/error stream, for example:

   @code{.c}
   printf("%s%sValue%s: %s%u%s\n",
          my_codes.bold, my_codes.fg_blue, my_codes.reset,
          my_codes.fg_bright_green, my_value, my_codes.reset);
   @endcode

@param[out] codes
    This function sets \bt_p{*codes} to the
    ANSI escape codes to use depending on the context.
@param[in] when
    @parblock
    When to use terminal text formatting:

    <dl>
      <dt>#BT_COMMON_COLOR_WHEN_AUTO
      <dd>
        Only use text formatting when the connected
        terminal supports it.

      <dt>#BT_COMMON_COLOR_WHEN_ALWAYS
      <dd>
        Always use text formatting, even if the connected
        terminal doesn't support it.

      <dt>#BT_COMMON_COLOR_WHEN_NEVER
      <dd>
        Never use text formatting, even if the connected
        terminal supports it.
    </dl>
    @endparblock

@bt_pre_not_null{codes}
*/
void bt_common_color_get_codes(struct bt_common_color_codes *codes,
		enum bt_common_color_when when);

/*! @} */

/*!
@addtogroup common-c-lttng-live
@{
*/

/*!
@brief
    Parts of an LTTng live URL.

@sa bt_common_parse_lttng_live_url
*/
struct bt_common_lttng_live_url_parts {
	/*! @brief Protocol. */
	GString *proto;

	/*! @brief Host name. */
	GString *hostname;

	/*! @brief Target host name. */
	GString *target_hostname;

	/*! @brief Recording session name. */
	GString *session_name;

	/*! @brief Port (-1 means the default port). */
	int port;
};

/*!
@brief
    Parses \bt_p{url} to return individual parts.

For example, with \bt_p{url} containing

@code{.undefined}
net://192.168.2.8:6645/host/meow/mix
@endcode

the parts are:

<table>
  <tr>
    <th>Part name
    <th>Member
    <th>Value
  <tr>
    <td>Protocol
    <td>bt_common_lttng_live_url_parts::proto
    <td><code>net</code>
  <tr>
    <td>Host name
    <td>bt_common_lttng_live_url_parts::hostname
    <td><code>192.168.2.8</code>
  <tr>
    <td>Port
    <td>bt_common_lttng_live_url_parts::port
    <td>6645
  <tr>
    <td>Target host name
    <td>bt_common_lttng_live_url_parts::target_hostname
    <td><code>meow</code>
  <tr>
    <td>Recording session name
    <td>bt_common_lttng_live_url_parts::session_name
    <td><code>mix</code>
</table>

@param[in] url
    LTTng live URL to parse.
@param[out] error_buf
    @parblock
    On failure, \bt_p{*error_buf} contains details about the failed
    parsing operation.

    Can be \c NULL.
    @endparblock
@param[in] error_buf_size
    Size of \bt_p{error_buf}, or ignored if \bt_p{error_buf} is \c NULL.

@returns
    @parblock
    Parts of \bt_p{url} on success.

    On failure, at least the bt_common_lttng_live_url_parts::proto
    member is \c NULL.

    Destroy with bt_common_destroy_lttng_live_url_parts().
    @endparblock

@bt_pre_not_null{url}
*/
struct bt_common_lttng_live_url_parts bt_common_parse_lttng_live_url(
		const char *url, char *error_buf, size_t error_buf_size);

/*!
@brief
    Deallocates the allocated members of \bt_p{parts}.

@param[in] parts
    Parts to destroy.
*/
void bt_common_destroy_lttng_live_url_parts(
		struct bt_common_lttng_live_url_parts *parts);

/*! @} */

/*!
@brief
    Returns whether or not the current process has setuid/setgid access
    right flags.

@ingroup common-c

@code{.c}
#include "common/common.h"
@endcode

@retval false
    The current process has no setuid/setgid access right flags.
@retval true
    The current process has setuid/setgid access right flags.
*/
bool bt_common_is_setuid_setgid(void);

/*!
@brief
    Returns the system page size, or aborts and logs an error
    if it's not available.

@ingroup common-c

@code{.c}
#include "common/common.h"
@endcode

@param[in] log_level
    Log level to use (one of the #bt_log_level enumerators).

@returns
    System page size (bytes).
*/
size_t bt_common_get_page_size(int log_level);

/*!
@brief
    Aborts the current process.

@ingroup common-c

Executes the value of the \c BABELTRACE_EXEC_ON_ABORT
environment variable, if set, in shell context before terminating
the process.

@code{.c}
#include "common/common.h"
@endcode
*/
void bt_common_abort(void) __attribute__((noreturn));

/*!
@addtogroup common-c-path
@{
*/

/*!
@brief
    Returns the system-wide \bt_name plugin path.

For example, <code>/usr/lib/babeltrace2/plugins</code>.

@returns
    @parblock
    System-wide \bt_name plugin path.

    Do not free the return value.
    @endparblock
*/
const char *bt_common_get_system_plugin_path(void);

/*!
@brief
    Returns the user \bt_name plugin path.

For example, <code>/home/user/.local/lib/babeltrace2/plugins</code>.

@param[in] log_level
    Log level to use (one of the #bt_log_level enumerators).

@returns
    @parblock
    User \bt_name plugin path.

    You must free the value.
    @endparblock
*/
char *bt_common_get_home_plugin_path(int log_level);

/*!
@brief
    Appends the list of directories \bt_p{paths} to the array
    \bt_p{dirs}.

The paths in \bt_p{paths} are separated by <code>;</code> on Windows
and <code>:</code> on other platforms.

For example, if \bt_p{dirs} initially contains

- <code>/path/to/meow</code>
- <code>/path/to/mix</code>

and \bt_p{paths} is <code>/tmp/kilo:/home/user/joule</code>, then
when this function returns, \bt_p{dirs} contains

- <code>/path/to/meow</code>
- <code>/path/to/mix</code>
- <code>/tmp/kilo</code>
- <code>/home/user/joule</code>

@param[in] paths
    List of paths to append to \bt_p{dirs}.
@param[in] dirs
    @parblock
    Directories to which to append the directories in \bt_p{paths}.

    This is an array of \c GString pointers.
    @endparblock

@returns
    0 on success, or -1 on failure.

@pre
    - \bt_p{paths} is \em not \c NULL.
    - \bt_p{dirs} is \em not \c NULL.
*/
int bt_common_append_plugin_path_dirs(const char *paths, GPtrArray *dirs);

/*!
@brief
    Returns the normalized version of \bt_p{path}, using \bt_p{wd},
    if set, as the working directory.

This function:

- If \bt_p{path} is a relative path, converts it to an
  absolute path using \bt_p{wd} as the working directory, or the
  current working directory if \bt_p{wd} is \c NULL.

- Removes contiguous and trailing slashes.

- Resolves <code>..</code> and <code>.</code> in both \bt_p{path} and
  \bt_p{wd}.

- Does \em not resolve symbolic links.

@param[in] path
    Path to normalize.
@param[in] wd
    @parblock
    Specific working directory.

    Can be \c NULL, in which case this function uses the current
    working directory instead.
    @endparblock

@returns
    New \c GString containing the normalized version of \bt_p{path}
    on success, or \c NULL on failure.

@pre
    - \bt_p{path} is \em not \c NULL.
    - \bt_p{wd} is \em not \c NULL.
*/
GString *bt_common_normalize_path(const char *path, const char *wd);

/*! @} */

/*!
@addtogroup common-c-str
@{
*/

/*!
@brief
    Returns the substring from \bt_p{input} until any of the characters
    in \bt_p{end_chars} (excluded), ignoring escape sequences for
    characters in \bt_p{escapable_chars}.

If \bt_p{input} doesn't contain any of the characters in \bt_p{end_chars},
then the returned strings is exactly \bt_p{input}.

For example, with \bt_p{escapable_chars} set to \c ab and
\bt_p{end_chars} set to <code>bc</code>:

<table>
  <tr>
    <th>\bt_p{input} parameter
    <th>Returned string
  <tr>
    <td><code>home rule m\\anager</code>
    <td><code>home rule m\\an</code>
  <tr>
    <td><code>\\belong qu\\alified decade</code>
    <td><code>\\belong qu\\alified de</code>
</table>

@param[in] input
    Input string.
@param[in] escapable_chars
    @parblock
    List of escapable characters.

    An escape sequence is <code>\\</code> followed by another character.
    @endparblock
@param[in] end_chars
    @parblock
    List of characters to consider as the end of the string.

    The actual ending character itself is not part of the returned
    string.
    @endparblock
@param[out] end_pos
    On success, if not \c NULL, \bt_p{*end_pos} points to either
    the actual ending character or to the null character.

@returns
    New \c GString containing the substring on success, or \c NULL on
    failure.

@pre
    - \bt_p{input} is \em not \c NULL.
    - \bt_p{escapable_chars} is \em not \c NULL.
    - \bt_p{end_chars} is \em not \c NULL.
*/
GString *bt_common_string_until(const char *input, const char *escapable_chars,
                    const char *end_chars, size_t *end_pos);

/*!
@brief
    Returns the quoted version of \bt_p{input} for a shell.

Examples with \bt_p{with_single_quotes} set to <code>true</code>:

<table>
  <tr>
    <th>\bt_p{input} parameter
    <th>Returned string
  <tr>
    <td><code>meow/mix.c</code>
    <td><code>'meow/mix.c'</code>
  <tr>
    <td><code>In A Gadda Da Vida</code>
    <td><code>'In A Gadda Da Vida'</code>
  <tr>
    <td><code>j'aime les fraises</code>
    <td><code>'j'&quot;'&quot;'aime les fraises'</code>
</table>

@param[in] input
    String to quote for a shell.
@param[in] with_single_quotes
    @parblock
    Whether or not to prepend and append singles quotes to the whole
    returned string.

    If \c false, then add the single quotes yourself.
    @endparblock

@returns
    New \c GString containing the quoted string on success, or \c NULL
    on failure.

@bt_pre_not_null{input}
*/
GString *bt_common_shell_quote(const char *input, bool with_single_quotes);

/*!
@brief
    Returns whether or not \em all the characters of \bt_p{input}
    are printable.

@param[in] input
    String to check.

@retval false
    At least one character of \bt_p{input} isn't printable.
@retval true
    All the characters of \bt_p{input} are printable.

@bt_pre_not_null{input}
*/
bool bt_common_string_is_printable(const char *input);

/*!
@brief
    Adds the digit separator \bt_p{sep} as many times as needed to form
    groups of \bt_p{digits_per_group} digits within \bt_p{str}.

Examples:

<table>
  <tr>
    <th>\bt_p{str} parameter
    <th>\bt_p{digits_per_group} parameter
    <th>\bt_p{sep} parameter
    <th>Updated \bt_p{str}
  <tr>
    <td><code>18363279</code>
    <td>3
    <td><code>,</code>
    <td><code>18,363,279</code>
  <tr>
    <td><code>101110100101110101</code>
    <td>4
    <td><code>:</code>
    <td><code>10:1110:1001:0111:0101</code>
</table>

@param[in,out] str
    Input string to be updated.
@param[in] digits_per_group
    Number of digits per group.
@param[in] sep
    Separation character.

@pre
    - \bt_p{str} is \em not \c NULL.
    - \bt_p{str} contains at least one character.
    - The size of the \bt_p{str} buffer is at least
      <code>strlen(str)</code>&nbsp;+&nbsp;(<code>strlen(str)</code>&nbsp;/&nbsp;\bt_p{digits_per_group})&nbsp;+&nbsp;1.
    - \bt_p{digits_per_group}&nbsp;&ge;&nbsp;1.
    - \bt_p{sep} is \em not <code>\\0</code>.
*/
void bt_common_sep_digits(char *str, unsigned int digits_per_group, char sep);

/*!
@brief
    Folds \bt_p{str} to \bt_p{total_length} columns, indenting each line
    with \bt_p{indent} spaces, and returns the resulting string.

This is similar to what the \bt_ext_man{fold,1,1} command does.

If \bt_p{str} contains a word of which the length
is greater than or equal to the contents length
(\bt_p{total_length}&nbsp;−&nbsp;\bt_p{indent}), then the
corresponding folded line is also larger than the contents length.
In other words, breaking at spaces is a best effort, but it might not be
possible.

For example, with \bt_p{total_length} set to 20 and \bt_p{indent} to 4,
the original string \bt_p{str}

@code{.undefined}
Chislic ground round turducken venison kielbasashanksalami strip steak prosciutto bresaola.
@endcode

becomes (<code>·</code> means space)

@code{.undefined}
····Chislic·ground
····round·turducken
····venison
····kielbasashanksalami
····strip·steak
····prosciutto
····bresaola.
@endcode

@param[in] str
    String to fold.
@param[in] total_length
    Maximum number of columns in the returned string.
@param[in] indent
    Number of spaces to prepend to each line of the returned
    string.

@returns
    New \c GString containing the folded string on success, or \c NULL
    on failure.

@pre
    - \bt_p{str} is \em not \c NULL.
    - \bt_p{total_length}&nbsp;&gt;&nbsp;1.
*/
GString *bt_common_fold(const char *str, unsigned int total_length,
		unsigned int indent);

/*! @} */

/*!
@brief
    Normalizes the star globbing pattern \bt_p{pattern} to be used
    with bt_common_star_glob_match().

@ingroup common-c

Normalization occurs in place (\bt_p{pattern} is modified).

@code{.c}
#include "common/common.h"
@endcode

@param[in,out] pattern
    Star globbing pattern to normalize.

@bt_pre_not_null{pattern}
*/
void bt_common_normalize_star_glob_pattern(char *pattern);

/*!
@brief
    Returns whether or not the star globbing pattern \bt_p{pattern}
    matches the string \bt_p{candidate}.

@ingroup common-c

The only supported special character is <code>*</code> which matches
anything. For example, <code>b*in</code> matches <code>bain</code>
and <code>bizantin</code>.

@code{.c}
#include "common/common.h"
@endcode

@param[in] pattern
    Star globbing pattern of length \bt_p{pattern_len}.
@param[in] pattern_len
    Length of \bt_p{pattern}.
@param[in] candidate
    Candidate string of length \bt_p{candidate_len}.
@param[in] candidate_len
    Length of \bt_p{candidate}.

@retval false
    \bt_p{pattern} doesn't match \bt_p{candidate}.
@retval true
    \bt_p{pattern} matches \bt_p{candidate}.

@pre
    - \bt_p{pattern} is \em not \c NULL.
    - \bt_p{pattern_len}&nbsp;&gt;&nbsp;0.
    - \bt_p{candidate} is \em not \c NULL.
    - \bt_p{candidate_len}&nbsp;&gt;&nbsp;0.
*/
bool bt_common_star_glob_match(const char *pattern, size_t pattern_len,
                const char *candidate, size_t candidate_len);

/*!
@brief
    Custom conversion specifier handling function type for
    bt_common_custom_vsnprintf().

@ingroup common-c

Because this is an internal utility, this function doesn't
return a status: it aborts when there's any error (bad
\bt_p{*fmt}, for example).

@code{.c}
#include "common/common.h"
@endcode

@param[in] priv_data
    Private data (\bt_p{priv_data} parameter when you called
    bt_common_custom_vsnprintf()).
@param[in,out] buf
    @parblock
    Address of current output buffer pointer.

    \bt_p{*buf} is where to append new output data.

    This function must update \bt_p{*buf} when it appends new data.

    \bt_p{avail_size} bytes are available from \bt_p{*buf}.
    @endparblock
@param[in] avail_size
    Number of available bytes from \bt_p{*buf}.
@param[in,out] fmt
    @parblock
    Address of the current format string pointer.

    \bt_p{*fmt} points to the introductory \c % character of the
    conversion specifier which is followed by the value of the
    \bt_p{intro} parameter when you called bt_common_custom_vsnprintf().

    This function must update \bt_p{*fmt} so that it points to the
    character \em after the whole conversion specifier.
    @endparblock
@param[in] args
    @parblock
    Variable argument list.

    Use \bt_ext_man{va_arg,3,3} to get new arguments from this list
    and update it at the same time.
    @endparblock

@pre
    - \bt_p{buf} is \em not \c NULL.
    - \bt_p{fmt} is \em not \c NULL.
*/
typedef void (* bt_common_handle_custom_specifier_func)(void *priv_data,
		char **buf, size_t avail_size, const char **fmt, va_list *args);

/*!
@brief
    Like \bt_ext_man{vsnprintf,3,3}, but also handles
    custom conversion specifiers.

@ingroup common-c

\bt_p{fmt} is a typical \bt_ext_man{printf,3,3}-style format string,
but with the following limitations:

- The <code>*</code> width specifier isn't accepted.
- The <code>*</code> precision specifier isn't accepted.
- The \c j and \c t length modifiers aren't accepted.
- The \c n format specifier isn't accepted.
- The format specifiers defined in \bt_ext_man{inttypes.h,0,0p} aren't
  accepted, except for <code>PRId64</code>, <code>PRIu64</code>,
  <code>PRIx64</code>, <code>PRIX64</code>, <code>PRIo64</code>, and
  <code>PRIi64</code>.

\bt_p{intro} specifies which special character immediately following an
introductory <code>\%</code> character in \bt_p{fmt} indicates a custom
conversion specifier. For example, if \bt_p{intro} is <code>\@</code>,
then any <code>\%\@</code> sequence in \bt_p{fmt} is the \em beginning
of a custom conversion specifier. \bt_p{handle_specifier} entirely
determines the end of a custom conversion specifier.

When this function encounters a custom conversion specifier in
\bt_p{fmt}, it calls \bt_p{handle_specifier}. This callback receives
what's needed to consume the conversion specifier from \bt_p{fmt}
and write output data into \bt_p{buf}.

This function always writes a terminating null character to \bt_p{buf}.

Because this is an internal utility, this function doesn't
return a status: it aborts when there's any error (bad
\bt_p{fmt}, for example).

@code{.c}
#include "common/common.h"
@endcode

@param[in] buf
    Output buffer of size \bt_p{buf_size} bytes.
@param[in] buf_size
    Number of available bytes in \bt_p{buf}.
@param[in] intro
    Introductory character (after <code>\%</code>).
@param[in] handle_specifier
    Custom conversion specifier handler.
@param[in] priv_data
    Private data for \bt_p{handle_specifier}.
@param[in] fmt
    Format string.
@param[in] args
    Variable argument list.

@pre
    - \bt_p{buf} is \em not \c NULL.
    - \bt_p{buf_size}&nbsp;&gt;&nbsp;0.
    - \bt_p{intro} is \em not <code>\\0</code>.
    - \bt_p{handle_specifier} is \em not \c NULL.
    - \bt_p{fmt} is \em not \c NULL.
    - \bt_p{fmt} is a valid format string.
*/
void bt_common_custom_vsnprintf(char *buf, size_t buf_size,
		char intro,
		bt_common_handle_custom_specifier_func handle_specifier,
		void *priv_data, const char *fmt, va_list *args);

/*!
@brief
    Appends the whole remaining contents of the file \bt_p{fp} (at its
    current reading position) to the string \bt_p{str}.

@ingroup glib-c

This function does \em not rewind \bt_p{fp} once it's done or on error.

@code{.c}
#include "common/common.h"
@endcode

@param[in] str
    String to which to append the contents of \bt_p{fp}.
@param[in] fp
    File of which to append the whole remaining contents, at its current
    reading position, to \bt_p{str}.

@retval 0
    Success.
@retval -1
    Failure.

@pre
    - \bt_p{str} is \em not \c NULL.
    - \bt_p{fp} is \em not \c NULL.
    - \bt_p{fp} is open without any error flag.
*/
int bt_common_append_file_content_to_g_string(GString *str, FILE *fp);

/*!
@brief
    A more efficient version of <code>g_string_append_printf()</code>.

@ingroup glib-c

<a href="https://docs.gtk.org/glib/method.String.append_printf.html">g_string_append_printf()</a>
internally allocates a temporary buffer through
<code>vasnprintf()</code> for each call. When we did \bt_name profiling,
this clearly appeared at the top of the report.

This function is our own efficient version of
<code>g_string_append_printf()</code> which operates directly on the
\c GString buffer.

@code{.c}
#include "common/common.h"
@endcode

@param[in] str
    See
    <a href="https://docs.gtk.org/glib/method.String.append_printf.html"><code>GLib.String.append_printf</code></a>.
@param[in] fmt
    See
    <a href="https://docs.gtk.org/glib/method.String.append_printf.html"><code>GLib.String.append_printf</code></a>.
@param[in] ...
    See
    <a href="https://docs.gtk.org/glib/method.String.append_printf.html"><code>GLib.String.append_printf</code></a>.

@returns
    See
    <a href="https://docs.gtk.org/glib/method.String.append_printf.html"><code>GLib.String.append_printf</code></a>.
*/
__BT_ATTR_FORMAT_PRINTF(2, 3)
int bt_common_g_string_append_printf(GString *str, const char *fmt, ...);

/*!
@brief
    An inline, more efficient version of <code>g_string_append()</code>.

@ingroup glib-c

See
<a href="https://docs.gtk.org/glib/method.String.append.html"><code>GLib.String.append</code></a>.

@code{.c}
#include "common/common.h"
@endcode

@param[in] str
    See
    <a href="https://docs.gtk.org/glib/method.String.append.html"><code>GLib.String.append</code></a>.
@param[in] s
    See
    <a href="https://docs.gtk.org/glib/method.String.append.html"><code>GLib.String.append</code></a>.
*/
static inline
void bt_common_g_string_append(GString *str, const char *s)
{
	gsize len, allocated_len, s_len;

	/* str->len excludes \0. */
	len = str->len;
	/* Exclude \0. */
	allocated_len = str->allocated_len - 1;
	s_len = strlen(s);
	if (G_UNLIKELY(allocated_len < len + s_len)) {
		/* Resize. */
		g_string_set_size(str, len + s_len);
	} else {
		str->len = len + s_len;
	}
	memcpy(str->str + len, s, s_len + 1);
}

/*!
@brief
    An inline, more efficient version of <code>g_string_append_c()</code>.

@ingroup glib-c

See
<a href="https://docs.gtk.org/glib/method.String.append_c.html"><code>GLib.String.append_c</code></a>.

@code{.c}
#include "common/common.h"
@endcode

@param[in] str
    See
    <a href="https://docs.gtk.org/glib/method.String.append_c.html"><code>GLib.String.append_c</code></a>.
@param[in] c
    See
    <a href="https://docs.gtk.org/glib/method.String.append_c.html"><code>GLib.String.append_c</code></a>.
*/
static inline
void bt_common_g_string_append_c(GString *str, char c)
{
	gsize len, allocated_len, s_len;

	/* str->len excludes \0. */
	len = str->len;
	/* Exclude \0. */
	allocated_len = str->allocated_len - 1;
	s_len = 1;
	if (G_UNLIKELY(allocated_len < len + s_len)) {
		/* Resize. */
		g_string_set_size(str, len + s_len);
	} else {
		str->len = len + s_len;
	}
	str->str[len] = c;
	str->str[len + 1] = '\0';
}

/*!
@brief
    Nanoseconds per second as \c int64_t.

@ingroup common-c

@code{.c}
#include "common/common.h"
@endcode
*/
#define NS_PER_S_I	INT64_C(1000000000)

/*!
@brief
    Nanoseconds per second as \c uint64_t.

@ingroup common-c

@code{.c}
#include "common/common.h"
@endcode
*/
#define NS_PER_S_U	UINT64_C(1000000000)

/*!
@brief
    Sets \bt_p{*raw_value} to a raw clock value from the
    ns-from-origin value \bt_p{ns_from_origin} considering
    the offset from origin \bt_p{cc_offset_seconds} seconds and
    \bt_p{cc_offset_cycles} cycles and the frequency
    \bt_p{cc_freq}&nbsp;Hz.

@ingroup common-c

@note
    This function computes the result using integers, therefore there
    might be rounding errors when \bt_p{cc_freq} isn't 1,000,000,000.

@code{.c}
#include "common/common.h"
@endcode

@param [in] cc_offset_seconds
    Offset of \bt_p{*raw_value} from origin (seconds).
@param [in] cc_offset_cycles
    Offset of \bt_p{*raw_value} from origin (clock cycles).
@param [in] cc_freq
    Clock frequency (Hz).
@param [in] ns_from_origin
    Value to convert (nanoseconds from origin).
@param [out] raw_value
    On success, \bt_p{*raw_value} is the converted raw clock value.

@retval 0
    Success.
@retval -1
    Failure.

@pre
    - \bt_p{cc_freq}&nbsp;&gt;&nbsp;0.
    - \bt_p{raw_value} is \em not \c NULL.
*/
static inline
int bt_common_clock_value_from_ns_from_origin(
		int64_t cc_offset_seconds, uint64_t cc_offset_cycles,
		uint64_t cc_freq, int64_t ns_from_origin,
		uint64_t *raw_value)
{
	int ret = 0;
	int64_t offset_in_ns;
	uint64_t value_in_ns;
	uint64_t rem_value_in_ns;
	uint64_t value_periods;
	uint64_t value_period_cycles;
	int64_t ns_to_add;

	BT_ASSERT_DBG(raw_value);

	/* Compute offset part of requested value, in nanoseconds */
	if (!bt_safe_to_mul_int64(cc_offset_seconds, NS_PER_S_I)) {
		ret = -1;
		goto end;
	}

	offset_in_ns = cc_offset_seconds * NS_PER_S_I;

	if (cc_freq == NS_PER_S_U) {
		ns_to_add = (int64_t) cc_offset_cycles;
	} else {
		if (!bt_safe_to_mul_int64((int64_t) cc_offset_cycles,
				NS_PER_S_I)) {
			ret = -1;
			goto end;
		}

		ns_to_add = ((int64_t) cc_offset_cycles * NS_PER_S_I) /
			(int64_t) cc_freq;
	}

	if (!bt_safe_to_add_int64(offset_in_ns, ns_to_add)) {
		ret = -1;
		goto end;
	}

	offset_in_ns += ns_to_add;

	/* Value part in nanoseconds */
	if (ns_from_origin < offset_in_ns) {
		ret = -1;
		goto end;
	}

	value_in_ns = (uint64_t) (ns_from_origin - offset_in_ns);

	/* Number of whole clock periods in `value_in_ns` */
	value_periods = value_in_ns / NS_PER_S_U;

	/* Remaining nanoseconds in cycles + whole clock periods in cycles */
	rem_value_in_ns = value_in_ns - value_periods * NS_PER_S_U;

	if (value_periods > UINT64_MAX / cc_freq) {
		ret = -1;
		goto end;
	}

	if (!bt_safe_to_mul_uint64(value_periods, cc_freq)) {
		ret = -1;
		goto end;
	}

	value_period_cycles = value_periods * cc_freq;

	if (!bt_safe_to_mul_uint64(cc_freq, rem_value_in_ns)) {
		ret = -1;
		goto end;
	}

	if (!bt_safe_to_add_uint64(cc_freq * rem_value_in_ns / NS_PER_S_U,
			value_period_cycles)) {
		ret = -1;
		goto end;
	}

	*raw_value = cc_freq * rem_value_in_ns / NS_PER_S_U +
		value_period_cycles;

end:
	return ret;
}

/*!
@addtogroup common-c-enum-str
@{
*/

/*!
@brief
    Returns the string of \bt_p{class_type}.

@param[in] class_type
    Enumerator of which to get the string.

@returns
    String of \bt_p{class_type}.
*/
static inline
const char *bt_common_field_class_type_string(enum bt_field_class_type class_type)
{
	switch (class_type) {
	case BT_FIELD_CLASS_TYPE_BOOL:
		return "BOOL";
	case BT_FIELD_CLASS_TYPE_BIT_ARRAY:
		return "BIT_ARRAY";
	case BT_FIELD_CLASS_TYPE_INTEGER:
		return "INTEGER";
	case BT_FIELD_CLASS_TYPE_UNSIGNED_INTEGER:
		return "UNSIGNED_INTEGER";
	case BT_FIELD_CLASS_TYPE_SIGNED_INTEGER:
		return "SIGNED_INTEGER";
	case BT_FIELD_CLASS_TYPE_ENUMERATION:
		return "ENUMERATION";
	case BT_FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION:
		return "UNSIGNED_ENUMERATION";
	case BT_FIELD_CLASS_TYPE_SIGNED_ENUMERATION:
		return "SIGNED_ENUMERATION";
	case BT_FIELD_CLASS_TYPE_REAL:
		return "REAL";
	case BT_FIELD_CLASS_TYPE_SINGLE_PRECISION_REAL:
		return "SINGLE_PRECISION_REAL";
	case BT_FIELD_CLASS_TYPE_DOUBLE_PRECISION_REAL:
		return "DOUBLE_PRECISION_REAL";
	case BT_FIELD_CLASS_TYPE_STRING:
		return "STRING";
	case BT_FIELD_CLASS_TYPE_STRUCTURE:
		return "STRUCTURE";
	case BT_FIELD_CLASS_TYPE_ARRAY:
		return "ARRAY";
	case BT_FIELD_CLASS_TYPE_STATIC_ARRAY:
		return "STATIC_ARRAY";
	case BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY:
		return "DYNAMIC_ARRAY";
	case BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITHOUT_LENGTH_FIELD:
		return "DYNAMIC_ARRAY_WITHOUT_LENGTH_FIELD";
	case BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITH_LENGTH_FIELD:
		return "DYNAMIC_ARRAY_WITH_LENGTH_FIELD";
	case BT_FIELD_CLASS_TYPE_OPTION:
		return "OPTION";
	case BT_FIELD_CLASS_TYPE_OPTION_WITHOUT_SELECTOR_FIELD:
		return "OPTION_WITHOUT_SELECTOR_FIELD";
	case BT_FIELD_CLASS_TYPE_OPTION_WITH_SELECTOR_FIELD:
		return "OPTION_WITH_SELECTOR_FIELD";
	case BT_FIELD_CLASS_TYPE_OPTION_WITH_BOOL_SELECTOR_FIELD:
		return "OPTION_WITH_BOOL_SELECTOR_FIELD";
	case BT_FIELD_CLASS_TYPE_OPTION_WITH_INTEGER_SELECTOR_FIELD:
		return "OPTION_WITH_INTEGER_SELECTOR_FIELD";
	case BT_FIELD_CLASS_TYPE_OPTION_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD:
		return "OPTION_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD";
	case BT_FIELD_CLASS_TYPE_OPTION_WITH_SIGNED_INTEGER_SELECTOR_FIELD:
		return "OPTION_WITH_SIGNED_INTEGER_SELECTOR_FIELD";
	case BT_FIELD_CLASS_TYPE_VARIANT:
		return "VARIANT";
	case BT_FIELD_CLASS_TYPE_VARIANT_WITHOUT_SELECTOR_FIELD:
		return "VARIANT_WITHOUT_SELECTOR_FIELD";
	case BT_FIELD_CLASS_TYPE_VARIANT_WITH_SELECTOR_FIELD:
		return "VARIANT_WITH_SELECTOR_FIELD";
	case BT_FIELD_CLASS_TYPE_VARIANT_WITH_INTEGER_SELECTOR_FIELD:
		return "VARIANT_WITH_INTEGER_SELECTOR_FIELD";
	case BT_FIELD_CLASS_TYPE_VARIANT_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD:
		return "VARIANT_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD";
	case BT_FIELD_CLASS_TYPE_VARIANT_WITH_SIGNED_INTEGER_SELECTOR_FIELD:
		return "VARIANT_WITH_SIGNED_INTEGER_SELECTOR_FIELD";
	case BT_FIELD_CLASS_TYPE_BLOB:
		return "BT_FIELD_CLASS_TYPE_BLOB";
	case BT_FIELD_CLASS_TYPE_STATIC_BLOB:
		return "BT_FIELD_CLASS_TYPE_STATIC_BLOB";
	case BT_FIELD_CLASS_TYPE_DYNAMIC_BLOB:
		return "BT_FIELD_CLASS_TYPE_DYNAMIC_BLOB";
	case BT_FIELD_CLASS_TYPE_DYNAMIC_BLOB_WITHOUT_LENGTH_FIELD:
		return "BT_FIELD_CLASS_TYPE_DYNAMIC_BLOB_WITHOUT_LENGTH_FIELD";
	case BT_FIELD_CLASS_TYPE_DYNAMIC_BLOB_WITH_LENGTH_FIELD:
		return "BT_FIELD_CLASS_TYPE_DYNAMIC_BLOB_WITH_LENGTH_FIELD";
	case __BT_FIELD_CLASS_TYPE_BIG_VALUE:
		bt_common_abort ();
	}

	bt_common_abort();
};

/*!
@brief
    Returns the string of \bt_p{base}.

@param[in] base
    Enumerator of which to get the string.

@returns
    String of \bt_p{base}.
*/
static inline
const char *bt_common_field_class_integer_preferred_display_base_string(enum bt_field_class_integer_preferred_display_base base)
{
	switch (base) {
	case BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_BINARY:
		return "BINARY";
	case BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_OCTAL:
		return "OCTAL";
	case BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL:
		return "DECIMAL";
	case BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_HEXADECIMAL:
		return "HEXADECIMAL";
	}

	bt_common_abort();
}

/*!
@brief
    Returns the string of \bt_p{scope}.

@param[in] scope
    Enumerator of which to get the string.

@returns
    String of \bt_p{scope}.
*/
static inline
const char *bt_common_field_path_scope_string(enum bt_field_path_scope scope)
{
	switch (scope) {
	case BT_FIELD_PATH_SCOPE_PACKET_CONTEXT:
		return "PACKET_CONTEXT";
	case BT_FIELD_PATH_SCOPE_EVENT_COMMON_CONTEXT:
		return "EVENT_COMMON_CONTEXT";
	case BT_FIELD_PATH_SCOPE_EVENT_SPECIFIC_CONTEXT:
		return "EVENT_SPECIFIC_CONTEXT";
	case BT_FIELD_PATH_SCOPE_EVENT_PAYLOAD:
		return "EVENT_PAYLOAD";
	}

	bt_common_abort();
}

/*!
@brief
    Returns the string of \bt_p{scope}.

@param[in] scope
    Enumerator of which to get the string.

@returns
    String of \bt_p{scope}.
*/
static inline
const char *bt_common_field_location_scope_string(enum bt_field_location_scope scope)
{
	switch (scope) {
	case BT_FIELD_LOCATION_SCOPE_PACKET_CONTEXT:
		return "PACKET_CONTEXT";
	case BT_FIELD_LOCATION_SCOPE_EVENT_COMMON_CONTEXT:
		return "EVENT_COMMON_CONTEXT";
	case BT_FIELD_LOCATION_SCOPE_EVENT_SPECIFIC_CONTEXT:
		return "EVENT_SPECIFIC_CONTEXT";
	case BT_FIELD_LOCATION_SCOPE_EVENT_PAYLOAD:
		return "EVENT_PAYLOAD";
	}

	bt_common_abort();
}

/*!
@brief
    Returns the string of \bt_p{level}.

@param[in] level
    Enumerator of which to get the string.

@returns
    String of \bt_p{level}.
*/
static inline
const char *bt_common_event_class_log_level_string(
		enum bt_event_class_log_level level)
{
	switch (level) {
	case BT_EVENT_CLASS_LOG_LEVEL_EMERGENCY:
		return "EMERGENCY";
	case BT_EVENT_CLASS_LOG_LEVEL_ALERT:
		return "ALERT";
	case BT_EVENT_CLASS_LOG_LEVEL_CRITICAL:
		return "CRITICAL";
	case BT_EVENT_CLASS_LOG_LEVEL_ERROR:
		return "ERROR";
	case BT_EVENT_CLASS_LOG_LEVEL_WARNING:
		return "WARNING";
	case BT_EVENT_CLASS_LOG_LEVEL_NOTICE:
		return "NOTICE";
	case BT_EVENT_CLASS_LOG_LEVEL_INFO:
		return "INFO";
	case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_SYSTEM:
		return "DEBUG_SYSTEM";
	case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_PROGRAM:
		return "DEBUG_PROGRAM";
	case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_PROCESS:
		return "DEBUG_PROCESS";
	case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_MODULE:
		return "DEBUG_MODULE";
	case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_UNIT:
		return "DEBUG_UNIT";
	case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_FUNCTION:
		return "DEBUG_FUNCTION";
	case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_LINE:
		return "DEBUG_LINE";
	case BT_EVENT_CLASS_LOG_LEVEL_DEBUG:
		return "DEBUG";
	}

	bt_common_abort();
};

/*!
@brief
    Returns the string of \bt_p{type}.

@param[in] type
    Enumerator of which to get the string.

@returns
    String of \bt_p{type}.
*/
static inline
const char *bt_common_value_type_string(enum bt_value_type type)
{
	switch (type) {
	case BT_VALUE_TYPE_NULL:
		return "NULL";
	case BT_VALUE_TYPE_BOOL:
		return "BOOL";
	case BT_VALUE_TYPE_INTEGER:
		return "INTEGER";
	case BT_VALUE_TYPE_UNSIGNED_INTEGER:
		return "UNSIGNED_INTEGER";
	case BT_VALUE_TYPE_SIGNED_INTEGER:
		return "SIGNED_INTEGER";
	case BT_VALUE_TYPE_REAL:
		return "REAL";
	case BT_VALUE_TYPE_STRING:
		return "STRING";
	case BT_VALUE_TYPE_ARRAY:
		return "ARRAY";
	case BT_VALUE_TYPE_MAP:
		return "MAP";
	}

	bt_common_abort();
};

/*!
@brief
    Returns the string of \bt_p{path}.

@param[in] path
    Enumerator of which to get the string.

@returns
    String of \bt_p{path}.

@bt_pre_not_null{path}
*/
static inline
GString *bt_common_field_path_string(struct bt_field_path *path)
{
	GString *str = g_string_new(NULL);
	uint64_t i;

	BT_ASSERT_DBG(path);

	if (!str) {
		goto end;
	}

	g_string_append_printf(str, "[%s", bt_common_field_path_scope_string(
		bt_field_path_get_root_scope(path)));

	for (i = 0; i < bt_field_path_get_item_count(path); i++) {
		const struct bt_field_path_item *fp_item =
			bt_field_path_borrow_item_by_index_const(path, i);

		switch (bt_field_path_item_get_type(fp_item)) {
		case BT_FIELD_PATH_ITEM_TYPE_INDEX:
			g_string_append_printf(str, ", %" PRIu64,
				bt_field_path_item_index_get_index(fp_item));
			break;
		case BT_FIELD_PATH_ITEM_TYPE_CURRENT_ARRAY_ELEMENT:
			g_string_append(str, ", <CUR>");
			break;
		default:
			bt_common_abort();
		}
	}

	g_string_append(str, "]");

end:
	return str;
}

/*!
@brief
    Returns the string of \bt_p{level}.

@param[in] level
    Enumerator of which to get the string.

@returns
    String of \bt_p{level}.
*/
static inline
const char *bt_common_logging_level_string(
		enum bt_logging_level level)
{
	switch (level) {
	case BT_LOGGING_LEVEL_TRACE:
		return "TRACE";
	case BT_LOGGING_LEVEL_DEBUG:
		return "DEBUG";
	case BT_LOGGING_LEVEL_INFO:
		return "INFO";
	case BT_LOGGING_LEVEL_WARNING:
		return "WARNING";
	case BT_LOGGING_LEVEL_ERROR:
		return "ERROR";
	case BT_LOGGING_LEVEL_FATAL:
		return "FATAL";
	case BT_LOGGING_LEVEL_NONE:
		return "NONE";
	}

	bt_common_abort();
};

/*!
@brief
    Returns the string of \bt_p{status}.

@param[in] status
    One of the <code>__BT_FUNC_STATUS_*</code> definitions, as found
    in the public <code>include/babeltrace2/func-status.h</code>
    header.

@returns
    String of \bt_p{status}.
*/
static inline
const char *bt_common_func_status_string(int status)
{
	switch (status) {
	case __BT_FUNC_STATUS_OVERFLOW_ERROR:
		return "OVERFLOW";
	case __BT_FUNC_STATUS_MEMORY_ERROR:
		return "MEMORY_ERROR";
	case __BT_FUNC_STATUS_USER_ERROR:
		return "USER_ERROR";
	case __BT_FUNC_STATUS_ERROR:
		return "ERROR";
	case __BT_FUNC_STATUS_OK:
		return "OK";
	case __BT_FUNC_STATUS_END:
		return "END";
	case __BT_FUNC_STATUS_NOT_FOUND:
		return "NOT_FOUND";
	case __BT_FUNC_STATUS_INTERRUPTED:
		return "INTERRUPTED";
	case __BT_FUNC_STATUS_NO_MATCH:
		return "NO_MATCH";
	case __BT_FUNC_STATUS_AGAIN:
		return "AGAIN";
	case __BT_FUNC_STATUS_UNKNOWN_OBJECT:
		return "UNKNOWN_OBJECT";
	}

	bt_common_abort();
}

/*!
@brief
    Returns the string of \bt_p{type}.

@param[in] type
    Enumerator of which to get the string.

@returns
    String of \bt_p{type}.
*/
static inline
const char *bt_common_component_class_type_string(
		enum bt_component_class_type type)
{
	switch (type) {
	case BT_COMPONENT_CLASS_TYPE_SOURCE:
		return "SOURCE";
	case BT_COMPONENT_CLASS_TYPE_SINK:
		return "SINK";
	case BT_COMPONENT_CLASS_TYPE_FILTER:
		return "FILTER";
	}

	bt_common_abort();
}

/*!
@brief
    Returns the string of \bt_p{type}.

@param[in] type
    Enumerator of which to get the string.

@returns
    String of \bt_p{type}.
*/
static inline
const char *bt_common_message_type_string(enum bt_message_type type)
{
	switch (type) {
	case BT_MESSAGE_TYPE_STREAM_BEGINNING:
		return "STREAM_BEGINNING";
	case BT_MESSAGE_TYPE_STREAM_END:
		return "STREAM_END";
	case BT_MESSAGE_TYPE_EVENT:
		return "EVENT";
	case BT_MESSAGE_TYPE_PACKET_BEGINNING:
		return "PACKET_BEGINNING";
	case BT_MESSAGE_TYPE_PACKET_END:
		return "PACKET_END";
	case BT_MESSAGE_TYPE_DISCARDED_EVENTS:
		return "DISCARDED_EVENTS";
	case BT_MESSAGE_TYPE_DISCARDED_PACKETS:
		return "DISCARDED_PACKETS";
	case BT_MESSAGE_TYPE_MESSAGE_ITERATOR_INACTIVITY:
		return "MESSAGE_ITERATOR_INACTIVITY";
	}

	bt_common_abort();
}

/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_COMMON_COMMON_H */
