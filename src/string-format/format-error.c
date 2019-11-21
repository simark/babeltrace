/*
 * Copyright EfficiOS, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define BT_LOG_OUTPUT_LEVEL log_level
#define BT_LOG_TAG "COMMON/FORMAT-ERROR"
#include <logging/log.h>

#include "format-error.h"

#include <stdint.h>
#include <string-format/format-plugin-comp-cls-name.h>

gchar *format_bt_error(const bt_error *error, unsigned int columns,
		bt_logging_level log_level)
{
	GString *str;
	gchar *ret;
	int64_t i;
	GString *folded = NULL;
	gchar *comp_cls_str = NULL;

	BT_ASSERT(error);
	BT_ASSERT(bt_error_get_cause_count(error) > 0);

	str = g_string_new(NULL);
	if (!str) {
		BT_LOGE_STR("Could not allocate a GString.");
		goto end;
	}

	/* Reverse order: deepest (root) cause printed at the end */
	for (i = bt_error_get_cause_count(error) - 1; i >= 0; i--) {
		const bt_error_cause *cause =
			bt_error_borrow_cause_by_index(error, (uint64_t) i);
		const char *prefix_fmt =
			i == bt_error_get_cause_count(error) - 1 ?
				"%s%sERROR%s:    " : "%s%sCAUSED BY%s ";

		/* Print prefix */
		g_string_append_printf(str, prefix_fmt,
			bt_common_color_bold(), bt_common_color_fg_bright_red(),
			bt_common_color_reset());

		/* Print actor name */
		g_string_append_c(str, '[');
		switch (bt_error_cause_get_actor_type(cause)) {
		case BT_ERROR_CAUSE_ACTOR_TYPE_UNKNOWN:
			g_string_append_printf(str, "%s%s%s",
				bt_common_color_bold(),
				bt_error_cause_get_module_name(cause),
				bt_common_color_reset());
			break;
		case BT_ERROR_CAUSE_ACTOR_TYPE_COMPONENT:
			comp_cls_str = format_plugin_comp_cls_opt(
				bt_error_cause_component_actor_get_plugin_name(cause),
				bt_error_cause_component_actor_get_component_class_name(cause),
				bt_error_cause_component_actor_get_component_class_type(cause));
			BT_ASSERT(comp_cls_str);

			g_string_append_printf(str, "%s%s%s: %s",
				bt_common_color_bold(),
				bt_error_cause_component_actor_get_component_name(cause),
				bt_common_color_reset(),
				comp_cls_str);

			break;
		case BT_ERROR_CAUSE_ACTOR_TYPE_COMPONENT_CLASS:
			comp_cls_str = format_plugin_comp_cls_opt(
				bt_error_cause_component_class_actor_get_plugin_name(cause),
				bt_error_cause_component_class_actor_get_component_class_name(cause),
				bt_error_cause_component_class_actor_get_component_class_type(cause));
			BT_ASSERT(comp_cls_str);

			g_string_append(str, comp_cls_str);
			break;
		case BT_ERROR_CAUSE_ACTOR_TYPE_MESSAGE_ITERATOR:
			comp_cls_str = format_plugin_comp_cls_opt(
				bt_error_cause_message_iterator_actor_get_plugin_name(cause),
				bt_error_cause_message_iterator_actor_get_component_class_name(cause),
				bt_error_cause_message_iterator_actor_get_component_class_type(cause));
			BT_ASSERT(comp_cls_str);

			g_string_append_printf(str, "%s%s%s (%s%s%s): %s",
				bt_common_color_bold(),
				bt_error_cause_message_iterator_actor_get_component_name(cause),
				bt_common_color_reset(),
				bt_common_color_bold(),
				bt_error_cause_message_iterator_actor_get_component_output_port_name(cause),
				bt_common_color_reset(),
				comp_cls_str);

			break;
		default:
			bt_common_abort();
		}

		/* Print file name and line number */
		g_string_append_printf(str, "] (%s%s%s%s:%s%" PRIu64 "%s)\n",
			bt_common_color_bold(),
			bt_common_color_fg_bright_magenta(),
			bt_error_cause_get_file_name(cause),
			bt_common_color_reset(),
			bt_common_color_fg_green(),
			bt_error_cause_get_line_number(cause),
			bt_common_color_reset());

		/* Print message */
		folded = bt_common_fold(bt_error_cause_get_message(cause),
			columns, 2);
		if (folded) {
			g_string_append(str, folded->str);
			g_string_free(folded, TRUE);
			folded = NULL;
		} else {
			BT_LOGE_STR("Could not fold string.");
			g_string_append(str, bt_error_cause_get_message(cause));
		}

		/*
		 * Don't append a newline at the end, since that is used to
		 * generate
		 */
		if (i > 0) {
			g_string_append_c(str, '\n');
		}
	}

end:
	BT_ASSERT(!folded);

	if (str) {
		ret = g_string_free(str, FALSE);
	} else {
		ret = NULL;
	}

	g_free(comp_cls_str);

	return ret;
}
