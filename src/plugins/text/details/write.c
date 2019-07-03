/*
 * Copyright 2019 Philippe Proulx <pproulx@efficios.com>
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

#include <babeltrace2/babeltrace.h>
#include <stdio.h>
#include <string.h>

#include "common/assert.h"
#include "common/common.h"
#include "details.h"
#include "write.h"
#include "obj-lifetime-mgmt.h"
#include "colors.h"

static inline
const char *plural(uint64_t value)
{
	return value == 1 ? "" : "s";
}

static inline
void incr_indent_by(struct details_write_ctx *ctx, unsigned int value)
{
	BT_ASSERT(ctx);
	ctx->indent_level += value;
}

static inline
void incr_indent(struct details_write_ctx *ctx)
{
	incr_indent_by(ctx, 2);
}

static inline
void decr_indent_by(struct details_write_ctx *ctx, unsigned int value)
{
	BT_ASSERT(ctx);
	BT_ASSERT(ctx->indent_level >= value);
	ctx->indent_level -= value;
}

static inline
void decr_indent(struct details_write_ctx *ctx)
{
	decr_indent_by(ctx, 2);
}

static inline
void format_uint(char *buf, uint64_t value, unsigned int base)
{
	const char *spec = "%" PRIu64;
	char *buf_start = buf;
	unsigned int digits_per_group = 3;
	char sep = ',';
	bool sep_digits = true;

	switch (base) {
	case 2:
	case 16:
		/* TODO: Support binary format */
		spec = "%" PRIx64;
		strcpy(buf, "0x");
		buf_start = buf + 2;
		digits_per_group = 4;
		sep = ':';
		break;
	case 8:
		spec = "%" PRIo64;
		strcpy(buf, "0");
		buf_start = buf + 1;
		sep = ':';
		break;
	case 10:
		if (value <= 9999) {
			/*
			 * Do not insert digit separators for numbers
			 * under 10,000 as it looks weird.
			 */
			sep_digits = false;
		}

		break;
	default:
		abort();
	}

	sprintf(buf_start, spec, value);

	if (sep_digits) {
		bt_common_sep_digits(buf_start, digits_per_group, sep);
	}
}

static inline
void format_int(char *buf, int64_t value, unsigned int base)
{
	const char *spec = "%" PRIu64;
	char *buf_start = buf;
	unsigned int digits_per_group = 3;
	char sep = ',';
	bool sep_digits = true;
	uint64_t abs_value = value < 0 ? (uint64_t) -value : (uint64_t) value;

	if (value < 0) {
		buf[0] = '-';
		buf_start++;
	}

	switch (base) {
	case 2:
	case 16:
		/* TODO: Support binary format */
		spec = "%" PRIx64;
		strcpy(buf_start, "0x");
		buf_start += 2;
		digits_per_group = 4;
		sep = ':';
		break;
	case 8:
		spec = "%" PRIo64;
		strcpy(buf_start, "0");
		buf_start++;
		sep = ':';
		break;
	case 10:
		if (value >= -9999 && value <= 9999) {
			/*
			 * Do not insert digit separators for numbers
			 * over -10,000 and under 10,000 as it looks
			 * weird.
			 */
			sep_digits = false;
		}

		break;
	default:
		abort();
	}

	sprintf(buf_start, spec, abs_value);

	if (sep_digits) {
		bt_common_sep_digits(buf_start, digits_per_group, sep);
	}
}

static inline
void write_nl(struct details_write_ctx *ctx)
{
	BT_ASSERT(ctx);
	g_string_append_c(ctx->str, '\n');
}

static inline
void write_sp(struct details_write_ctx *ctx)
{
	BT_ASSERT(ctx);
	g_string_append_c(ctx->str, ' ');
}

static inline
void write_indent(struct details_write_ctx *ctx)
{
	uint64_t i;

	BT_ASSERT(ctx);

	for (i = 0; i < ctx->indent_level; i++) {
		write_sp(ctx);
	}
}

static inline
void write_compound_member_name(struct details_write_ctx *ctx, const char *name)
{
	write_indent(ctx);
	g_string_append_printf(ctx->str, "%s%s%s:",
		color_fg_cyan(ctx), name, color_reset(ctx));
}

static inline
void write_array_index(struct details_write_ctx *ctx, uint64_t index)
{
	char buf[32];

	write_indent(ctx);
	format_uint(buf, index, 10);
	g_string_append_printf(ctx->str, "%s[%s]%s:",
		color_fg_cyan(ctx), buf, color_reset(ctx));
}

static inline
void write_obj_type_name(struct details_write_ctx *ctx, const char *name)
{
	g_string_append_printf(ctx->str, "%s%s%s%s",
		color_fg_yellow(ctx), color_bold(ctx), name, color_reset(ctx));
}

static inline
void write_prop_name(struct details_write_ctx *ctx, const char *prop_name)
{
	g_string_append_printf(ctx->str, "%s%s%s",
		color_fg_magenta(ctx), prop_name, color_reset(ctx));
}

static inline
void write_str_prop_value(struct details_write_ctx *ctx, const char *value)
{
	g_string_append_printf(ctx->str, "%s%s%s",
		color_bold(ctx), value, color_reset(ctx));
}

static inline
void write_uint_str_prop_value(struct details_write_ctx *ctx, const char *value)
{
	write_str_prop_value(ctx, value);
}

static inline
void write_uint_prop_value(struct details_write_ctx *ctx, uint64_t value)
{
	char buf[32];

	format_uint(buf, value, 10);
	write_uint_str_prop_value(ctx, buf);
}

static inline
void write_int_prop_value(struct details_write_ctx *ctx, int64_t value)
{
	char buf[32];

	format_int(buf, value, 10);
	write_uint_str_prop_value(ctx, buf);
}

static inline
void write_float_prop_value(struct details_write_ctx *ctx, double value)
{
	g_string_append_printf(ctx->str, "%s%f%s",
		color_bold(ctx), value, color_reset(ctx));
}

static inline
void write_str_prop_line(struct details_write_ctx *ctx, const char *prop_name,
		const char *prop_value)
{
	BT_ASSERT(prop_value);
	write_indent(ctx);
	write_prop_name(ctx, prop_name);
	g_string_append(ctx->str, ": ");
	write_str_prop_value(ctx, prop_value);
	write_nl(ctx);
}

static inline
void write_uint_prop_line(struct details_write_ctx *ctx, const char *prop_name,
		uint64_t prop_value)
{
	write_indent(ctx);
	write_prop_name(ctx, prop_name);
	g_string_append(ctx->str, ": ");
	write_uint_prop_value(ctx, prop_value);
	write_nl(ctx);
}

static inline
void write_int_prop_line(struct details_write_ctx *ctx, const char *prop_name,
		int64_t prop_value)
{
	write_indent(ctx);
	write_prop_name(ctx, prop_name);
	g_string_append(ctx->str, ": ");
	write_int_prop_value(ctx, prop_value);
	write_nl(ctx);
}

static inline
void write_int_str_prop_value(struct details_write_ctx *ctx, const char *value)
{
	write_str_prop_value(ctx, value);
}

static inline
void write_bool_prop_line(struct details_write_ctx *ctx, const char *prop_name,
		bt_bool prop_value)
{
	const char *str;

	write_indent(ctx);
	write_prop_name(ctx, prop_name);
	g_string_append_printf(ctx->str, ": %s", color_bold(ctx));

	if (prop_value) {
		g_string_append(ctx->str, color_fg_green(ctx));
		str = "Yes";
	} else {
		g_string_append(ctx->str, color_fg_red(ctx));
		str = "No";
	}

	g_string_append_printf(ctx->str, "%s%s\n", str, color_reset(ctx));
}

static inline
void write_uuid_prop_line(struct details_write_ctx *ctx, const char *prop_name,
		bt_uuid uuid)
{
	BT_ASSERT(uuid);
	write_indent(ctx);
	write_prop_name(ctx, prop_name);
	g_string_append_printf(ctx->str,
		": %s%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x%s\n",
		color_bold(ctx),
		(unsigned int) uuid[0],
		(unsigned int) uuid[1],
		(unsigned int) uuid[2],
		(unsigned int) uuid[3],
		(unsigned int) uuid[4],
		(unsigned int) uuid[5],
		(unsigned int) uuid[6],
		(unsigned int) uuid[7],
		(unsigned int) uuid[8],
		(unsigned int) uuid[9],
		(unsigned int) uuid[10],
		(unsigned int) uuid[11],
		(unsigned int) uuid[12],
		(unsigned int) uuid[13],
		(unsigned int) uuid[14],
		(unsigned int) uuid[15],
		color_reset(ctx));
}

static
void write_int_field_class_props(struct details_write_ctx *ctx,
		const bt_field_class *fc, bool close)
{
	g_string_append_printf(ctx->str, "(%s%" PRIu64 "-bit%s, Base ",
		color_bold(ctx),
		bt_field_class_integer_get_field_value_range(fc),
		color_reset(ctx));

	switch (bt_field_class_integer_get_preferred_display_base(fc)) {
	case BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_BINARY:
		write_uint_prop_value(ctx, 2);
		break;
	case BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_OCTAL:
		write_uint_prop_value(ctx, 8);
		break;
	case BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL:
		write_uint_prop_value(ctx, 10);
		break;
	case BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_HEXADECIMAL:
		write_uint_prop_value(ctx, 16);
		break;
	default:
		abort();
	}

	if (close) {
		g_string_append(ctx->str, ")");
	}
}

struct enum_field_class_mapping_range {
	union {
		uint64_t u;
		int64_t i;
	} lower;

	union {
		uint64_t u;
		int64_t i;
	} upper;
};

struct enum_field_class_mapping {
	/* Weak */
	const char *label;

	/* Array of `struct enum_field_class_mapping_range` */
	GArray *ranges;
};

static
gint compare_enum_field_class_mappings(struct enum_field_class_mapping **a,
		struct enum_field_class_mapping **b)
{
	return strcmp((*a)->label, (*b)->label);
}

static
gint compare_enum_field_class_mapping_ranges_signed(
		struct enum_field_class_mapping_range *a,
		struct enum_field_class_mapping_range *b)
{

	if (a->lower.i < b->lower.i) {
		return -1;
	} else if (a->lower.i > b->lower.i) {
		return 1;
	} else {
		if (a->upper.i < b->upper.i) {
			return -1;
		} else if (a->upper.i > b->upper.i) {
			return 1;
		} else {
			return 0;
		}
	}
}

static
gint compare_enum_field_class_mapping_ranges_unsigned(
		struct enum_field_class_mapping_range *a,
		struct enum_field_class_mapping_range *b)
{
	if (a->lower.u < b->lower.u) {
		return -1;
	} else if (a->lower.u > b->lower.u) {
		return 1;
	} else {
		if (a->upper.u < b->upper.u) {
			return -1;
		} else if (a->upper.u > b->upper.u) {
			return 1;
		} else {
			return 0;
		}
	}
}

static
void destroy_enum_field_class_mapping(struct enum_field_class_mapping *mapping)
{
	if (mapping->ranges) {
		g_array_free(mapping->ranges, TRUE);
		mapping->ranges = NULL;
	}

	g_free(mapping);
}

static
void write_enum_field_class_mapping_range(struct details_write_ctx *ctx,
		struct enum_field_class_mapping_range *range, bool is_signed)
{
	g_string_append(ctx->str, "[");

	if (is_signed) {
		write_int_prop_value(ctx, range->lower.i);
	} else {
		write_int_prop_value(ctx, range->lower.u);
	}

	g_string_append(ctx->str, ", ");

	if (is_signed) {
		write_int_prop_value(ctx, range->upper.i);
	} else {
		write_int_prop_value(ctx, range->upper.u);
	}

	g_string_append(ctx->str, "]");
}

static
void write_enum_field_class_mappings(struct details_write_ctx *ctx,
		const bt_field_class *fc)
{
	GPtrArray *mappings;
	uint64_t i;
	uint64_t range_i;
	bool is_signed = bt_field_class_get_type(fc) ==
		BT_FIELD_CLASS_TYPE_SIGNED_ENUMERATION;

	mappings = g_ptr_array_new_with_free_func(
		(GDestroyNotify) destroy_enum_field_class_mapping);
	BT_ASSERT(mappings);

	/*
	 * Copy field class's mappings to our own arrays and structures
	 * to sort them.
	 */
	for (i = 0; i < bt_field_class_enumeration_get_mapping_count(fc); i++) {
		const void *fc_mapping;
		struct enum_field_class_mapping *mapping = g_new0(
			struct enum_field_class_mapping, 1);

		BT_ASSERT(mapping);
		mapping->ranges = g_array_new(FALSE, TRUE,
			sizeof(struct enum_field_class_mapping_range));
		BT_ASSERT(mapping->ranges);

		if (is_signed) {
			fc_mapping = bt_field_class_signed_enumeration_borrow_mapping_by_index_const(
				fc, i);
		} else {
			fc_mapping = bt_field_class_unsigned_enumeration_borrow_mapping_by_index_const(
				fc, i);
		}

		mapping->label = bt_field_class_enumeration_mapping_get_label(
			bt_field_class_signed_enumeration_mapping_as_mapping_const(
				fc_mapping));

		for (range_i = 0;
				range_i < bt_field_class_enumeration_mapping_get_range_count(
					bt_field_class_signed_enumeration_mapping_as_mapping_const(fc_mapping));
				range_i++) {
			struct enum_field_class_mapping_range range;

			if (is_signed) {
				bt_field_class_signed_enumeration_mapping_get_range_by_index(
					fc_mapping, range_i,
					&range.lower.i, &range.upper.i);
			} else {
				bt_field_class_unsigned_enumeration_mapping_get_range_by_index(
					fc_mapping, range_i,
					&range.lower.u, &range.upper.u);
			}

			g_array_append_val(mapping->ranges, range);
		}

		g_ptr_array_add(mappings, mapping);
	}

	/* Sort mappings, and for each mapping, sort ranges */
	g_ptr_array_sort(mappings,
		(GCompareFunc) compare_enum_field_class_mappings);

	for (i = 0; i < mappings->len; i++) {
		struct enum_field_class_mapping *mapping = mappings->pdata[i];

		if (is_signed) {
			g_array_sort(mapping->ranges,
				(GCompareFunc)
					compare_enum_field_class_mapping_ranges_signed);
		} else {
			g_array_sort(mapping->ranges,
				(GCompareFunc)
					compare_enum_field_class_mapping_ranges_unsigned);
		}
	}

	/* Write mappings */
	for (i = 0; i < mappings->len; i++) {
		struct enum_field_class_mapping *mapping = mappings->pdata[i];

		write_nl(ctx);
		write_compound_member_name(ctx, mapping->label);

		if (mapping->ranges->len == 1) {
			/* Single one: write on same line */
			write_sp(ctx);
			write_enum_field_class_mapping_range(ctx,
				&g_array_index(mapping->ranges,
					struct enum_field_class_mapping_range,
					0), is_signed);
			continue;
		}

		incr_indent(ctx);

		for (range_i = 0; range_i < mapping->ranges->len; range_i++) {
			write_nl(ctx);
			write_indent(ctx);
			write_enum_field_class_mapping_range(ctx,
				&g_array_index(mapping->ranges,
					struct enum_field_class_mapping_range,
					range_i), is_signed);
		}

		decr_indent(ctx);
	}

	g_ptr_array_free(mappings, TRUE);
}

static
void write_field_path(struct details_write_ctx *ctx,
		const bt_field_path *field_path)
{
	uint64_t i;

	g_string_append_c(ctx->str, '[');

	switch (bt_field_path_get_root_scope(field_path)) {
	case BT_SCOPE_PACKET_CONTEXT:
		write_str_prop_value(ctx, "Packet context");
		break;
	case BT_SCOPE_EVENT_COMMON_CONTEXT:
		write_str_prop_value(ctx, "Event common context");
		break;
	case BT_SCOPE_EVENT_SPECIFIC_CONTEXT:
		write_str_prop_value(ctx, "Event specific context");
		break;
	case BT_SCOPE_EVENT_PAYLOAD:
		write_str_prop_value(ctx, "Event payload");
		break;
	default:
		abort();
	}

	g_string_append(ctx->str, ": ");

	for (i = 0; i < bt_field_path_get_item_count(field_path); i++) {
		const bt_field_path_item *fp_item =
			bt_field_path_borrow_item_by_index_const(field_path, i);

		if (i != 0) {
			g_string_append(ctx->str, ", ");
		}

		switch (bt_field_path_item_get_type(fp_item)) {
		case BT_FIELD_PATH_ITEM_TYPE_INDEX:
			write_uint_prop_value(ctx,
				bt_field_path_item_index_get_index(fp_item));
			break;
		case BT_FIELD_PATH_ITEM_TYPE_CURRENT_ARRAY_ELEMENT:
			write_str_prop_value(ctx, "<current>");
			break;
		default:
			abort();
		}
	}

	g_string_append_c(ctx->str, ']');
}

static
void write_field_class(struct details_write_ctx *ctx, const bt_field_class *fc,
		const char *name)
{
	uint64_t i;
	const char *type;
	bt_field_class_type fc_type = bt_field_class_get_type(fc);

	/* Write field class's name */
	if (name) {
		write_compound_member_name(ctx, name);
		write_sp(ctx);
	}

	/* Write field class's type */
	switch (fc_type) {
	case BT_FIELD_CLASS_TYPE_UNSIGNED_INTEGER:
		type = "Unsigned integer";
		break;
	case BT_FIELD_CLASS_TYPE_SIGNED_INTEGER:
		type = "Signed integer";
		break;
	case BT_FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION:
		type = "Unsigned enumeration";
		break;
	case BT_FIELD_CLASS_TYPE_SIGNED_ENUMERATION:
		type = "Signed enumeration";
		break;
	case BT_FIELD_CLASS_TYPE_REAL:
		type = "Real";
		break;
	case BT_FIELD_CLASS_TYPE_STRING:
		type = "String";
		break;
	case BT_FIELD_CLASS_TYPE_STRUCTURE:
		type = "Structure";
		break;
	case BT_FIELD_CLASS_TYPE_STATIC_ARRAY:
		type = "Static array";
		break;
	case BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY:
		type = "Dynamic array";
		break;
	case BT_FIELD_CLASS_TYPE_VARIANT:
		type = "Variant";
		break;
	default:
		abort();
	}

	g_string_append_printf(ctx->str, "%s%s%s",
		color_fg_blue(ctx), type, color_reset(ctx));

	/* Write field class's properties */
	switch (fc_type) {
	case BT_FIELD_CLASS_TYPE_UNSIGNED_INTEGER:
	case BT_FIELD_CLASS_TYPE_SIGNED_INTEGER:
		write_sp(ctx);
		write_int_field_class_props(ctx, fc, true);
		break;
	case BT_FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION:
	case BT_FIELD_CLASS_TYPE_SIGNED_ENUMERATION:
	{
		uint64_t mapping_count =
			bt_field_class_enumeration_get_mapping_count(fc);

		write_sp(ctx);
		write_int_field_class_props(ctx, fc, false);
		g_string_append(ctx->str, ", ");
		write_uint_prop_value(ctx, mapping_count);
		g_string_append_printf(ctx->str, " mapping%s)",
			plural(mapping_count));

		if (mapping_count > 0) {
			g_string_append_c(ctx->str, ':');
			incr_indent(ctx);
			write_enum_field_class_mappings(ctx, fc);
			decr_indent(ctx);
		}

		break;
	}
	case BT_FIELD_CLASS_TYPE_REAL:
		if (bt_field_class_real_is_single_precision(fc)) {
			g_string_append(ctx->str, " (Single precision)");
		} else {
			g_string_append(ctx->str, " (Double precision)");
		}

		break;
	case BT_FIELD_CLASS_TYPE_STRUCTURE:
	{
		uint64_t member_count =
			bt_field_class_structure_get_member_count(fc);

		g_string_append(ctx->str, " (");
		write_uint_prop_value(ctx, member_count);
		g_string_append_printf(ctx->str, " member%s)",
			plural(member_count));

		if (member_count > 0) {
			g_string_append_c(ctx->str, ':');
			incr_indent(ctx);

			for (i = 0; i < member_count; i++) {
				const bt_field_class_structure_member *member =
					bt_field_class_structure_borrow_member_by_index_const(
						fc, i);

				write_nl(ctx);
				write_field_class(ctx,
					bt_field_class_structure_member_borrow_field_class_const(member),
					bt_field_class_structure_member_get_name(member));
			}

			decr_indent(ctx);
		}

		break;
	}
	case BT_FIELD_CLASS_TYPE_STATIC_ARRAY:
	case BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY:
		if (fc_type == BT_FIELD_CLASS_TYPE_STATIC_ARRAY) {
			g_string_append(ctx->str, " (Length ");
			write_uint_prop_value(ctx,
				bt_field_class_static_array_get_length(fc));
			g_string_append_c(ctx->str, ')');
		} else {
			const bt_field_path *length_field_path =
				bt_field_class_dynamic_array_borrow_length_field_path_const(
					fc);

			if (length_field_path) {
				g_string_append(ctx->str, " (Length field path ");
				write_field_path(ctx, length_field_path);
				g_string_append_c(ctx->str, ')');
			}
		}

		g_string_append_c(ctx->str, ':');
		write_nl(ctx);
		incr_indent(ctx);
		write_field_class(ctx,
			bt_field_class_array_borrow_element_field_class_const(fc),
			"Element");
		decr_indent(ctx);
		break;
	case BT_FIELD_CLASS_TYPE_VARIANT:
	{
		uint64_t option_count =
			bt_field_class_variant_get_option_count(fc);
		const bt_field_path *sel_field_path =
			bt_field_class_variant_borrow_selector_field_path_const(
				fc);

		g_string_append(ctx->str, " (");
		write_uint_prop_value(ctx, option_count);
		g_string_append_printf(ctx->str, " option%s, ",
			plural(option_count));

		if (sel_field_path) {
			g_string_append(ctx->str, "Selector field path ");
			write_field_path(ctx, sel_field_path);
		}

		g_string_append_c(ctx->str, ')');

		if (option_count > 0) {
			g_string_append_c(ctx->str, ':');
			incr_indent(ctx);

			for (i = 0; i < option_count; i++) {
				const bt_field_class_variant_option *option =
					bt_field_class_variant_borrow_option_by_index_const(
						fc, i);

				write_nl(ctx);
				write_field_class(ctx,
					bt_field_class_variant_option_borrow_field_class_const(option),
					bt_field_class_variant_option_get_name(option));
			}

			decr_indent(ctx);
		}

		break;
	}
	default:
		break;
	}
}

static
void write_root_field_class(struct details_write_ctx *ctx, const char *name,
		const bt_field_class *fc)
{
	BT_ASSERT(name);
	BT_ASSERT(fc);
	write_indent(ctx);
	write_prop_name(ctx, name);
	g_string_append(ctx->str, ": ");
	write_field_class(ctx, fc, NULL);
	write_nl(ctx);
}

static
void write_event_class(struct details_write_ctx *ctx, const bt_event_class *ec)
{
	const char *name = bt_event_class_get_name(ec);
	const char *emf_uri;
	const bt_field_class *fc;
	bt_event_class_log_level log_level;

	write_indent(ctx);
	write_obj_type_name(ctx, "Event class");

	/* Write name and ID */
	if (name) {
		g_string_append_printf(ctx->str, " `%s%s%s`",
			color_fg_green(ctx), name, color_reset(ctx));
	}

	g_string_append(ctx->str, " (ID ");
	write_uint_prop_value(ctx, bt_event_class_get_id(ec));
	g_string_append(ctx->str, "):\n");

	/* Write properties */
	incr_indent(ctx);

	/* Write log level */
	if (bt_event_class_get_log_level(ec, &log_level) ==
			BT_PROPERTY_AVAILABILITY_AVAILABLE) {
		const char *ll_str = NULL;

		switch (log_level) {
		case BT_EVENT_CLASS_LOG_LEVEL_EMERGENCY:
			ll_str = "Emergency";
			break;
		case BT_EVENT_CLASS_LOG_LEVEL_ALERT:
			ll_str = "Alert";
			break;
		case BT_EVENT_CLASS_LOG_LEVEL_CRITICAL:
			ll_str = "Critical";
			break;
		case BT_EVENT_CLASS_LOG_LEVEL_ERROR:
			ll_str = "Error";
			break;
		case BT_EVENT_CLASS_LOG_LEVEL_WARNING:
			ll_str = "Warning";
			break;
		case BT_EVENT_CLASS_LOG_LEVEL_NOTICE:
			ll_str = "Notice";
			break;
		case BT_EVENT_CLASS_LOG_LEVEL_INFO:
			ll_str = "Info";
			break;
		case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_SYSTEM:
			ll_str = "Debug (system)";
			break;
		case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_PROGRAM:
			ll_str = "Debug (program)";
			break;
		case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_PROCESS:
			ll_str = "Debug (process)";
			break;
		case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_MODULE:
			ll_str = "Debug (module)";
			break;
		case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_UNIT:
			ll_str = "Debug (unit)";
			break;
		case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_FUNCTION:
			ll_str = "Debug (function)";
			break;
		case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_LINE:
			ll_str = "Debug (line)";
			break;
		case BT_EVENT_CLASS_LOG_LEVEL_DEBUG:
			ll_str = "Debug";
			break;
		default:
			abort();
		}

		write_str_prop_line(ctx, "Log level", ll_str);
	}

	/* Write EMF URI */
	emf_uri = bt_event_class_get_emf_uri(ec);
	if (emf_uri) {
		write_str_prop_line(ctx, "EMF URI", emf_uri);
	}

	/* Write specific context field class */
	fc = bt_event_class_borrow_specific_context_field_class_const(ec);
	if (fc) {
		write_root_field_class(ctx, "Specific context field class", fc);
	}

	/* Write payload field class */
	fc = bt_event_class_borrow_payload_field_class_const(ec);
	if (fc) {
		write_root_field_class(ctx, "Payload field class", fc);
	}

	decr_indent(ctx);
}

static
void write_clock_class_prop_lines(struct details_write_ctx *ctx,
		const bt_clock_class *cc)
{
	int64_t offset_seconds;
	uint64_t offset_cycles;
	const char *str;

	str = bt_clock_class_get_name(cc);
	if (str) {
		write_str_prop_line(ctx, "Name", str);
	}

	str = bt_clock_class_get_description(cc);
	if (str) {
		write_str_prop_line(ctx, "Description", str);
	}

	write_uint_prop_line(ctx, "Frequency (Hz)",
		bt_clock_class_get_frequency(cc));
	write_uint_prop_line(ctx, "Precision (cycles)",
		bt_clock_class_get_precision(cc));
	bt_clock_class_get_offset(cc, &offset_seconds, &offset_cycles);
	write_int_prop_line(ctx, "Offset (s)", offset_seconds);
	write_uint_prop_line(ctx, "Offset (cycles)", offset_cycles);
	write_bool_prop_line(ctx, "Origin is Unix epoch",
		bt_clock_class_origin_is_unix_epoch(cc));

	if (ctx->details_comp->cfg.with_uuid) {
		bt_uuid uuid = bt_clock_class_get_uuid(cc);

		if (uuid) {
			write_uuid_prop_line(ctx, "UUID", uuid);
		}
	}
}

static
gint compare_event_classes(const bt_event_class **a, const bt_event_class **b)
{
	uint64_t id_a = bt_event_class_get_id(*a);
	uint64_t id_b = bt_event_class_get_id(*b);

	if (id_a < id_b) {
		return -1;
	} else if (id_a > id_b) {
		return 1;
	} else {
		return 0;
	}
}

static
void write_stream_class(struct details_write_ctx *ctx,
		const bt_stream_class *sc)
{
	const bt_field_class *fc;
	GPtrArray *event_classes = g_ptr_array_new();
	uint64_t i;

	write_indent(ctx);
	write_obj_type_name(ctx, "Stream class");

	/* Write name and ID */
	if (ctx->details_comp->cfg.with_stream_class_name) {
		const char *name = bt_stream_class_get_name(sc);

		if (name) {
			g_string_append(ctx->str, " `");
			write_str_prop_value(ctx, name);
			g_string_append(ctx->str, "`");
		}
	}

	g_string_append(ctx->str, " (ID ");
	write_uint_prop_value(ctx, bt_stream_class_get_id(sc));
	g_string_append(ctx->str, "):\n");

	/* Write properties */
	incr_indent(ctx);

	/* Write configuration */
	write_bool_prop_line(ctx,
		"Packets have beginning default clock snapshot",
		bt_stream_class_packets_have_beginning_default_clock_snapshot(sc));
	write_bool_prop_line(ctx,
		"Packets have end default clock snapshot",
		bt_stream_class_packets_have_end_default_clock_snapshot(sc));
	write_bool_prop_line(ctx,
		"Supports discarded events",
		bt_stream_class_supports_discarded_events(sc));
	write_bool_prop_line(ctx,
		"Discarded events have default clock snapshots",
		bt_stream_class_discarded_events_have_default_clock_snapshots(sc));
	write_bool_prop_line(ctx,
		"Supports discarded packets",
		bt_stream_class_supports_discarded_packets(sc));
	write_bool_prop_line(ctx,
		"Discarded packets have default clock snapshots",
		bt_stream_class_discarded_packets_have_default_clock_snapshots(sc));

	/* Write default clock class */
	if (bt_stream_class_borrow_default_clock_class_const(sc)) {
		write_indent(ctx);
		write_prop_name(ctx, "Default clock class");
		g_string_append_c(ctx->str, ':');
		write_nl(ctx);
		incr_indent(ctx);
		write_clock_class_prop_lines(ctx,
			bt_stream_class_borrow_default_clock_class_const(sc));
		decr_indent(ctx);
	}

	fc = bt_stream_class_borrow_packet_context_field_class_const(sc);
	if (fc) {
		write_root_field_class(ctx, "Packet context field class", fc);
	}

	fc = bt_stream_class_borrow_event_common_context_field_class_const(sc);
	if (fc) {
		write_root_field_class(ctx, "Event common context field class",
			fc);
	}

	for (i = 0; i < bt_stream_class_get_event_class_count(sc); i++) {
		g_ptr_array_add(event_classes,
			(gpointer) bt_stream_class_borrow_event_class_by_index_const(
				sc, i));
	}

	g_ptr_array_sort(event_classes, (GCompareFunc) compare_event_classes);

	for (i = 0; i < event_classes->len; i++) {
		write_event_class(ctx, event_classes->pdata[i]);
	}

	decr_indent(ctx);
	g_ptr_array_free(event_classes, TRUE);
}

static
gint compare_stream_classes(const bt_stream_class **a, const bt_stream_class **b)
{
	uint64_t id_a = bt_stream_class_get_id(*a);
	uint64_t id_b = bt_stream_class_get_id(*b);

	if (id_a < id_b) {
		return -1;
	} else if (id_a > id_b) {
		return 1;
	} else {
		return 0;
	}
}

static
gint compare_strings(const char **a, const char **b)
{
	return strcmp(*a, *b);
}

static
void write_trace_class(struct details_write_ctx *ctx, const bt_trace_class *tc)
{
	GPtrArray *stream_classes = g_ptr_array_new();
	GPtrArray *env_names = g_ptr_array_new();
	uint64_t env_count;
	uint64_t i;
	bool printed_prop = false;

	write_indent(ctx);
	write_obj_type_name(ctx, "Trace class");

	/* Write name */
	if (ctx->details_comp->cfg.with_trace_class_name) {
		const char *name = bt_trace_class_get_name(tc);

		if (name) {
			g_string_append(ctx->str, " `");
			write_str_prop_value(ctx, name);
			g_string_append(ctx->str, "`");
		}
	}

	/* Write properties */
	incr_indent(ctx);

	if (ctx->details_comp->cfg.with_uuid) {
		bt_uuid uuid = bt_trace_class_get_uuid(tc);

		if (uuid) {
			if (!printed_prop) {
				g_string_append(ctx->str, ":\n");
				printed_prop = true;
			}

			write_uuid_prop_line(ctx, "UUID", uuid);
		}
	}

	/* Write environment */
	env_count = bt_trace_class_get_environment_entry_count(tc);
	if (env_count > 0) {
		if (!printed_prop) {
			g_string_append(ctx->str, ":\n");
			printed_prop = true;
		}

		write_indent(ctx);
		write_prop_name(ctx, "Environment");
		g_string_append(ctx->str, " (");
		write_uint_prop_value(ctx, env_count);
		g_string_append_printf(ctx->str, " entr%s):",
			env_count == 1 ? "y" : "ies");
		write_nl(ctx);
		incr_indent(ctx);

		for (i = 0; i < env_count; i++) {
			const char *name;
			const bt_value *value;

			bt_trace_class_borrow_environment_entry_by_index_const(
				tc, i, &name, &value);
			g_ptr_array_add(env_names, (gpointer) name);
		}

		g_ptr_array_sort(env_names, (GCompareFunc) compare_strings);

		for (i = 0; i < env_names->len; i++) {
			const char *name = env_names->pdata[i];
			const bt_value *value =
				bt_trace_class_borrow_environment_entry_value_by_name_const(
					tc, name);

			BT_ASSERT(value);
			write_compound_member_name(ctx, name);
			write_sp(ctx);

			if (bt_value_get_type(value) ==
					BT_VALUE_TYPE_SIGNED_INTEGER) {
				write_int_prop_value(ctx,
					bt_value_signed_integer_get(value));
			} else if (bt_value_get_type(value) ==
					BT_VALUE_TYPE_STRING) {
				write_str_prop_value(ctx,
					bt_value_string_get(value));
			} else {
				abort();
			}

			write_nl(ctx);
		}

		decr_indent(ctx);
	}

	for (i = 0; i < bt_trace_class_get_stream_class_count(tc); i++) {
		g_ptr_array_add(stream_classes,
			(gpointer) bt_trace_class_borrow_stream_class_by_index_const(
				tc, i));
	}

	g_ptr_array_sort(stream_classes, (GCompareFunc) compare_stream_classes);

	if (stream_classes->len > 0) {
		if (!printed_prop) {
			g_string_append(ctx->str, ":\n");
			printed_prop = true;
		}
	}

	for (i = 0; i < stream_classes->len; i++) {
		write_stream_class(ctx, stream_classes->pdata[i]);
	}

	decr_indent(ctx);

	if (!printed_prop) {
		write_nl(ctx);
	}

	g_ptr_array_free(stream_classes, TRUE);
	g_ptr_array_free(env_names, TRUE);
}

static
int try_write_meta(struct details_write_ctx *ctx, const bt_trace_class *tc,
		const bt_stream_class *sc, const bt_event_class *ec)
{
	int ret = 0;

	BT_ASSERT(tc);

	if (details_need_to_write_trace_class(ctx, tc)) {
		uint64_t sc_i;

		if (ctx->details_comp->cfg.compact &&
				ctx->details_comp->printed_something) {
			/*
			 * There are no empty line between messages in
			 * compact mode, so write one here to decouple
			 * the trace class from the next message.
			 */
			write_nl(ctx);
		}

		/*
		 * write_trace_class() also writes all its stream
		 * classes their event classes, so we don't need to
		 * rewrite `sc`.
		 */
		write_trace_class(ctx, tc);
		write_nl(ctx);

		/*
		 * Mark this trace class as written, as well as all
		 * its stream classes and their event classes.
		 */
		ret = details_did_write_trace_class(ctx, tc);
		if (ret) {
			goto end;
		}

		for (sc_i = 0; sc_i < bt_trace_class_get_stream_class_count(tc);
				sc_i++) {
			uint64_t ec_i;
			const bt_stream_class *tc_sc =
				bt_trace_class_borrow_stream_class_by_index_const(
					tc, sc_i);

			details_did_write_meta_object(ctx, tc, tc_sc);

			for (ec_i = 0; ec_i <
					bt_stream_class_get_event_class_count(tc_sc);
					ec_i++) {
				details_did_write_meta_object(ctx, tc,
					bt_stream_class_borrow_event_class_by_index_const(
						tc_sc, ec_i));
			}
		}

		goto end;
	}

	if (sc && details_need_to_write_meta_object(ctx, tc, sc)) {
		uint64_t ec_i;

		BT_ASSERT(tc);

		if (ctx->details_comp->cfg.compact &&
				ctx->details_comp->printed_something) {
			/*
			 * There are no empty line between messages in
			 * compact mode, so write one here to decouple
			 * the stream class from the next message.
			 */
			write_nl(ctx);
		}

		/*
		 * write_stream_class() also writes all its event
		 * classes, so we don't need to rewrite `ec`.
		 */
		write_stream_class(ctx, sc);
		write_nl(ctx);

		/*
		 * Mark this stream class as written, as well as all its
		 * event classes.
		 */
		details_did_write_meta_object(ctx, tc, sc);

		for (ec_i = 0; ec_i <
				bt_stream_class_get_event_class_count(sc);
				ec_i++) {
			details_did_write_meta_object(ctx, tc,
				bt_stream_class_borrow_event_class_by_index_const(
					sc, ec_i));
		}

		goto end;
	}

	if (ec && details_need_to_write_meta_object(ctx, tc, ec)) {
		BT_ASSERT(sc);

		if (ctx->details_comp->cfg.compact &&
				ctx->details_comp->printed_something) {
			/*
			 * There are no empty line between messages in
			 * compact mode, so write one here to decouple
			 * the event class from the next message.
			 */
			write_nl(ctx);
		}

		write_event_class(ctx, ec);
		write_nl(ctx);
		details_did_write_meta_object(ctx, tc, ec);
		goto end;
	}

end:
	return ret;
}

static
void write_time_str(struct details_write_ctx *ctx, const char *str)
{
	if (!ctx->details_comp->cfg.with_time) {
		goto end;
	}

	g_string_append_printf(ctx->str, "[%s%s%s%s]",
		color_bold(ctx), color_fg_blue(ctx), str, color_reset(ctx));

	if (ctx->details_comp->cfg.compact) {
		write_sp(ctx);
	} else {
		write_nl(ctx);
	}

end:
	return;
}

static
void write_time(struct details_write_ctx *ctx, const bt_clock_snapshot *cs)
{
	bt_clock_snapshot_status status;
	int64_t ns_from_origin;
	char buf[32];

	if (!ctx->details_comp->cfg.with_time) {
		goto end;
	}

	format_uint(buf, bt_clock_snapshot_get_value(cs), 10);
	g_string_append_printf(ctx->str, "[%s%s%s%s%s",
		color_bold(ctx), color_fg_blue(ctx), buf,
		color_reset(ctx),
		ctx->details_comp->cfg.compact ? "" : " cycles");
	status = bt_clock_snapshot_get_ns_from_origin(cs, &ns_from_origin);
	if (status == BT_CLOCK_SNAPSHOT_STATUS_OK) {
		format_int(buf, ns_from_origin, 10);
		g_string_append_printf(ctx->str, "%s %s%s%s%s%s",
			ctx->details_comp->cfg.compact ? "" : ",",
			color_bold(ctx), color_fg_blue(ctx), buf,
			color_reset(ctx),
			ctx->details_comp->cfg.compact ? "" : " ns from origin");
	}

	g_string_append(ctx->str, "]");

	if (ctx->details_comp->cfg.compact) {
		write_sp(ctx);
	} else {
		write_nl(ctx);
	}

end:
	return;
}

static
int write_message_follow_tag(struct details_write_ctx *ctx,
		const bt_stream *stream)
{
	int ret;
	uint64_t unique_trace_id;
	const bt_stream_class *sc = bt_stream_borrow_class_const(stream);
	const bt_trace *trace = bt_stream_borrow_trace_const(stream);

	ret = details_trace_unique_id(ctx, trace, &unique_trace_id);
	if (ret) {
		goto end;
	}

	if (ctx->details_comp->cfg.compact) {
		g_string_append_printf(ctx->str,
			"%s{%s%" PRIu64 " %" PRIu64 " %" PRIu64 "%s%s}%s ",
			color_fg_cyan(ctx), color_bold(ctx),
			unique_trace_id, bt_stream_class_get_id(sc),
			bt_stream_get_id(stream),
			color_reset(ctx), color_fg_cyan(ctx), color_reset(ctx));
	} else {
		g_string_append_printf(ctx->str,
			"%s{Trace %s%" PRIu64 "%s%s, Stream class ID %s%" PRIu64 "%s%s, Stream ID %s%" PRIu64 "%s%s}%s\n",
			color_fg_cyan(ctx),
			color_bold(ctx), unique_trace_id,
			color_reset(ctx), color_fg_cyan(ctx),
			color_bold(ctx), bt_stream_class_get_id(sc),
			color_reset(ctx), color_fg_cyan(ctx),
			color_bold(ctx), bt_stream_get_id(stream),
			color_reset(ctx), color_fg_cyan(ctx),
			color_reset(ctx));
	}

end:
	return ret;
}

static
void write_field(struct details_write_ctx *ctx, const bt_field *field,
		const char *name)
{
	uint64_t i;
	bt_field_class_type fc_type = bt_field_get_class_type(field);
	const bt_field_class *fc;
	char buf[64];

	/* Write field's name */
	if (name) {
		write_compound_member_name(ctx, name);
	}

	/* Write field's value */
	switch (fc_type) {
	case BT_FIELD_CLASS_TYPE_UNSIGNED_INTEGER:
	case BT_FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION:
	case BT_FIELD_CLASS_TYPE_SIGNED_INTEGER:
	case BT_FIELD_CLASS_TYPE_SIGNED_ENUMERATION:
	{
		unsigned int fmt_base;
		bt_field_class_integer_preferred_display_base base;

		fc = bt_field_borrow_class_const(field);
		base = bt_field_class_integer_get_preferred_display_base(fc);

		switch (base) {
		case BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL:
			fmt_base = 10;
			break;
		case BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_OCTAL:
			fmt_base = 8;
			break;
		case BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_BINARY:
			fmt_base = 2;
			break;
		case BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_HEXADECIMAL:
			fmt_base = 16;
			break;
		default:
			abort();
		}

		if (fc_type == BT_FIELD_CLASS_TYPE_UNSIGNED_INTEGER ||
				fc_type == BT_FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION) {
			format_uint(buf,
				bt_field_unsigned_integer_get_value(field),
				fmt_base);
			write_sp(ctx);
			write_uint_str_prop_value(ctx, buf);
		} else {
			format_int(buf,
				bt_field_signed_integer_get_value(field),
				fmt_base);
			write_sp(ctx);
			write_int_str_prop_value(ctx, buf);
		}

		break;
	}
	case BT_FIELD_CLASS_TYPE_REAL:
		write_sp(ctx);
		write_float_prop_value(ctx, bt_field_real_get_value(field));
		break;
	case BT_FIELD_CLASS_TYPE_STRING:
		write_sp(ctx);
		write_str_prop_value(ctx, bt_field_string_get_value(field));
		break;
	case BT_FIELD_CLASS_TYPE_STRUCTURE:
	{
		uint64_t member_count;

		fc = bt_field_borrow_class_const(field);
		member_count = bt_field_class_structure_get_member_count(fc);

		if (member_count > 0) {
			incr_indent(ctx);

			for (i = 0; i < member_count; i++) {
				const bt_field_class_structure_member *member =
					bt_field_class_structure_borrow_member_by_index_const(
						fc, i);
				const bt_field *member_field =
					bt_field_structure_borrow_member_field_by_index_const(
						field, i);

				write_nl(ctx);
				write_field(ctx, member_field,
					bt_field_class_structure_member_get_name(member));
			}

			decr_indent(ctx);
		} else {
			g_string_append(ctx->str, " Empty");
		}

		break;
	}
	case BT_FIELD_CLASS_TYPE_STATIC_ARRAY:
	case BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY:
	{
		uint64_t length = bt_field_array_get_length(field);

		if (length == 0) {
			g_string_append(ctx->str, " Empty");
		} else {
			g_string_append(ctx->str, " Length ");
			write_uint_prop_value(ctx, length);
			g_string_append_c(ctx->str, ':');
		}

		incr_indent(ctx);

		for (i = 0; i < length; i++) {
			const bt_field *elem_field =
				bt_field_array_borrow_element_field_by_index_const(
					field, i);

			write_nl(ctx);
			write_array_index(ctx, i);
			write_field(ctx, elem_field, NULL);
		}

		decr_indent(ctx);
		break;
	}
	case BT_FIELD_CLASS_TYPE_VARIANT:
		write_field(ctx,
			bt_field_variant_borrow_selected_option_field_const(
				field), NULL);
		break;
	default:
		abort();
	}
}

static
void write_root_field(struct details_write_ctx *ctx, const char *name,
		const bt_field *field)
{
	BT_ASSERT(name);
	BT_ASSERT(field);
	write_indent(ctx);
	write_prop_name(ctx, name);
	g_string_append(ctx->str, ":");
	write_field(ctx, field, NULL);
	write_nl(ctx);
}

static
int write_event_message(struct details_write_ctx *ctx,
		const bt_message *msg)
{
	int ret = 0;
	const bt_event *event = bt_message_event_borrow_event_const(msg);
	const bt_stream *stream = bt_event_borrow_stream_const(event);
	const bt_event_class *ec = bt_event_borrow_class_const(event);
	const bt_stream_class *sc = bt_event_class_borrow_stream_class_const(ec);
	const bt_trace_class *tc = bt_stream_class_borrow_trace_class_const(sc);
	const char *ec_name;
	const bt_field *field;

	ret = try_write_meta(ctx, tc, sc, ec);
	if (ret) {
		goto end;
	}

	/* Write time */
	if (bt_stream_class_borrow_default_clock_class_const(sc)) {
		write_time(ctx,
			bt_message_event_borrow_default_clock_snapshot_const(
				msg));
	}

	/* Write follow tag for message */
	ret = write_message_follow_tag(ctx, stream);
	if (ret) {
		goto end;
	}

	/* Write object's basic properties */
	write_obj_type_name(ctx, "Event");
	ec_name = bt_event_class_get_name(ec);
	if (ec_name) {
		g_string_append_printf(ctx->str, " `%s%s%s`",
			color_fg_green(ctx), ec_name, color_reset(ctx));
	}

	g_string_append(ctx->str, " (");

	if (!ctx->details_comp->cfg.compact) {
		g_string_append(ctx->str, "Class ID ");
	}

	write_uint_prop_value(ctx, bt_event_class_get_id(ec));
	g_string_append(ctx->str, ")");

	if (ctx->details_comp->cfg.compact) {
		write_nl(ctx);
		goto end;
	}

	/* Write fields */
	g_string_append(ctx->str, ":\n");
	incr_indent(ctx);
	field = bt_event_borrow_common_context_field_const(event);
	if (field) {
		write_root_field(ctx, "Common context", field);
	}

	field = bt_event_borrow_specific_context_field_const(event);
	if (field) {
		write_root_field(ctx, "Specific context", field);
	}

	field = bt_event_borrow_payload_field_const(event);
	if (field) {
		write_root_field(ctx, "Payload", field);
	}

	decr_indent(ctx);

end:

	return ret;
}

static
gint compare_streams(const bt_stream **a, const bt_stream **b)
{
	uint64_t id_a = bt_stream_get_id(*a);
	uint64_t id_b = bt_stream_get_id(*b);

	if (id_a < id_b) {
		return -1;
	} else if (id_a > id_b) {
		return 1;
	} else {
		const bt_stream_class *a_sc = bt_stream_borrow_class_const(*a);
		const bt_stream_class *b_sc = bt_stream_borrow_class_const(*b);
		uint64_t a_sc_id = bt_stream_class_get_id(a_sc);
		uint64_t b_sc_id = bt_stream_class_get_id(b_sc);

		if (a_sc_id < b_sc_id) {
			return -1;
		} else if (a_sc_id > b_sc_id) {
			return 1;
		} else {
			return 0;
		}
	}
}

static
void write_trace(struct details_write_ctx *ctx, const bt_trace *trace)
{
	const char *name;
	const bt_trace_class *tc = bt_trace_borrow_class_const(trace);
	GPtrArray *streams = g_ptr_array_new();
	uint64_t i;
	bool printed_prop = false;

	write_indent(ctx);
	write_obj_type_name(ctx, "Trace");

	/* Write name */
	if (ctx->details_comp->cfg.with_trace_name) {
		name = bt_trace_get_name(trace);
		if (name) {
			g_string_append(ctx->str, " `");
			write_str_prop_value(ctx, name);
			g_string_append(ctx->str, "`");
		}
	}

	/* Write properties */
	incr_indent(ctx);

	if (ctx->details_comp->cfg.with_trace_class_name) {
		name = bt_trace_class_get_name(tc);
		if (name) {
			if (!printed_prop) {
				g_string_append(ctx->str, ":\n");
				printed_prop = true;
			}

			write_str_prop_line(ctx, "Class name", name);
		}
	}

	if (ctx->details_comp->cfg.with_uuid) {
		bt_uuid uuid = bt_trace_class_get_uuid(tc);

		if (uuid) {
			if (!printed_prop) {
				g_string_append(ctx->str, ":\n");
				printed_prop = true;
			}

			write_uuid_prop_line(ctx, "Class UUID", uuid);
		}
	}

	for (i = 0; i < bt_trace_get_stream_count(trace); i++) {
		g_ptr_array_add(streams,
			(gpointer) bt_trace_borrow_stream_by_index_const(
				trace, i));
	}

	g_ptr_array_sort(streams, (GCompareFunc) compare_streams);

	if (streams->len > 0 && !printed_prop) {
		g_string_append(ctx->str, ":\n");
		printed_prop = true;
	}

	for (i = 0; i < streams->len; i++) {
		const bt_stream *stream = streams->pdata[i];

		write_indent(ctx);
		write_obj_type_name(ctx, "Stream");
		g_string_append(ctx->str, " (ID ");
		write_uint_prop_value(ctx, bt_stream_get_id(stream));
		g_string_append(ctx->str, ", Class ID ");
		write_uint_prop_value(ctx, bt_stream_class_get_id(
			bt_stream_borrow_class_const(stream)));
		g_string_append(ctx->str, ")");
		write_nl(ctx);
	}

	decr_indent(ctx);

	if (!printed_prop) {
		write_nl(ctx);
	}

	g_ptr_array_free(streams, TRUE);
}

static
int write_stream_beginning_message(struct details_write_ctx *ctx,
		const bt_message *msg)
{
	int ret = 0;
	const bt_stream *stream =
		bt_message_stream_beginning_borrow_stream_const(msg);
	const bt_trace *trace = bt_stream_borrow_trace_const(stream);
	const bt_stream_class *sc = bt_stream_borrow_class_const(stream);
	const bt_clock_class *cc = bt_stream_class_borrow_default_clock_class_const(sc);
	const bt_trace_class *tc = bt_stream_class_borrow_trace_class_const(sc);
	const char *name;

	ret = try_write_meta(ctx, tc, sc, NULL);
	if (ret) {
		goto end;
	}

	/* Write time */
	if (cc) {
		const bt_clock_snapshot *cs;
		enum bt_message_stream_clock_snapshot_state cs_state =
			bt_message_stream_beginning_borrow_default_clock_snapshot_const(msg, &cs);

		if (cs_state == BT_MESSAGE_STREAM_CLOCK_SNAPSHOT_STATE_KNOWN) {
			write_time(ctx, cs);
		} else {
			write_time_str(ctx, "Unknown");
		}
	}

	/* Write follow tag for message */
	ret = write_message_follow_tag(ctx, stream);
	if (ret) {
		goto end;
	}

	/* Write stream properties */
	write_obj_type_name(ctx, "Stream beginning");

	if (ctx->details_comp->cfg.compact) {
		write_nl(ctx);
		goto end;
	}

	g_string_append(ctx->str, ":\n");
	incr_indent(ctx);

	if (ctx->details_comp->cfg.with_stream_name) {
		name = bt_stream_get_name(stream);
		if (name) {
			write_str_prop_line(ctx, "Name", name);
		}
	}

	if (ctx->details_comp->cfg.with_stream_class_name) {
		name = bt_stream_class_get_name(sc);
		if (name) {
			write_str_prop_line(ctx, "Class name", name);
		}
	}

	write_trace(ctx, trace);
	decr_indent(ctx);

end:
	return ret;
}

static
int write_stream_end_message(struct details_write_ctx *ctx,
		const bt_message *msg)
{
	int ret = 0;
	const bt_stream *stream =
		bt_message_stream_end_borrow_stream_const(msg);
	const bt_stream_class *sc =
		bt_stream_borrow_class_const(stream);
	const bt_clock_class *cc =
		bt_stream_class_borrow_default_clock_class_const(sc);

	/* Write time */
	if (cc) {
		const bt_clock_snapshot *cs;
		enum bt_message_stream_clock_snapshot_state cs_state =
			bt_message_stream_end_borrow_default_clock_snapshot_const(msg, &cs);

		if (cs_state == BT_MESSAGE_STREAM_CLOCK_SNAPSHOT_STATE_KNOWN) {
			write_time(ctx, cs);
		} else {
			write_time_str(ctx, "Unknown");
		}
	}

	/* Write follow tag for message */
	ret = write_message_follow_tag(ctx, stream);
	if (ret) {
		goto end;
	}

	/* Write stream properties */
	write_obj_type_name(ctx, "Stream end\n");

end:
	return ret;
}

static
int write_packet_beginning_message(struct details_write_ctx *ctx,
		const bt_message *msg)
{
	int ret = 0;
	const bt_packet *packet =
		bt_message_packet_beginning_borrow_packet_const(msg);
	const bt_stream *stream = bt_packet_borrow_stream_const(packet);
	const bt_stream_class *sc = bt_stream_borrow_class_const(stream);
	const bt_field *field;

	/* Write time */
	if (bt_stream_class_packets_have_beginning_default_clock_snapshot(sc)) {
		write_time(ctx,
			bt_message_packet_beginning_borrow_default_clock_snapshot_const(
				msg));
	}

	/* Write follow tag for message */
	ret = write_message_follow_tag(ctx, stream);
	if (ret) {
		goto end;
	}

	write_obj_type_name(ctx, "Packet beginning");

	if (ctx->details_comp->cfg.compact) {
		write_nl(ctx);
		goto end;
	}

	/* Write field */
	g_string_append(ctx->str, ":\n");
	incr_indent(ctx);
	field = bt_packet_borrow_context_field_const(packet);
	if (field) {
		write_root_field(ctx, "Context", field);
	}

	decr_indent(ctx);

end:
	return ret;
}

static
int write_discarded_items_message(struct details_write_ctx *ctx,
		const char *name, const bt_stream *stream,
		const bt_clock_snapshot *beginning_cs,
		const bt_clock_snapshot *end_cs, uint64_t count)
{
	int ret = 0;

	/* Write times */
	if (beginning_cs) {
		write_time(ctx, beginning_cs);
		BT_ASSERT(end_cs);
		write_time(ctx, end_cs);
	}

	/* Write follow tag for message */
	ret = write_message_follow_tag(ctx, stream);
	if (ret) {
		goto end;
	}

	write_obj_type_name(ctx, "Discarded ");
	write_obj_type_name(ctx, name);

	/* Write count */
	if (count == UINT64_C(-1)) {
		write_nl(ctx);
		goto end;
	}

	g_string_append(ctx->str, " (");
	write_uint_prop_value(ctx, count);
	g_string_append_printf(ctx->str, " %s)\n", name);

end:
	return ret;
}

static
int write_discarded_events_message(struct details_write_ctx *ctx,
		const bt_message *msg)
{
	const bt_stream *stream = bt_message_discarded_events_borrow_stream_const(
		msg);
	const bt_stream_class *sc = bt_stream_borrow_class_const(stream);
	const bt_clock_snapshot *beginning_cs = NULL;
	const bt_clock_snapshot *end_cs = NULL;
	uint64_t count;

	if (bt_stream_class_discarded_events_have_default_clock_snapshots(sc)) {
		beginning_cs =
			bt_message_discarded_events_borrow_beginning_default_clock_snapshot_const(
				msg);
		end_cs =
			bt_message_discarded_events_borrow_end_default_clock_snapshot_const(
				msg);
	}

	if (bt_message_discarded_events_get_count(msg, &count) !=
			BT_PROPERTY_AVAILABILITY_AVAILABLE) {
		count = UINT64_C(-1);
	}

	return write_discarded_items_message(ctx, "events", stream,
		beginning_cs, end_cs, count);
}

static
int write_discarded_packets_message(struct details_write_ctx *ctx,
		const bt_message *msg)
{
	const bt_stream *stream = bt_message_discarded_packets_borrow_stream_const(
		msg);
	const bt_stream_class *sc = bt_stream_borrow_class_const(stream);
	const bt_clock_snapshot *beginning_cs = NULL;
	const bt_clock_snapshot *end_cs = NULL;
	uint64_t count;

	if (bt_stream_class_discarded_packets_have_default_clock_snapshots(sc)) {
		beginning_cs =
			bt_message_discarded_packets_borrow_beginning_default_clock_snapshot_const(
				msg);
		end_cs =
			bt_message_discarded_packets_borrow_end_default_clock_snapshot_const(
				msg);
	}

	if (bt_message_discarded_packets_get_count(msg, &count) !=
			BT_PROPERTY_AVAILABILITY_AVAILABLE) {
		count = UINT64_C(-1);
	}

	return write_discarded_items_message(ctx, "packets", stream,
		beginning_cs, end_cs, count);
}

static
int write_packet_end_message(struct details_write_ctx *ctx,
		const bt_message *msg)
{
	int ret = 0;
	const bt_packet *packet =
		bt_message_packet_end_borrow_packet_const(msg);
	const bt_stream *stream = bt_packet_borrow_stream_const(packet);
	const bt_stream_class *sc = bt_stream_borrow_class_const(stream);

	/* Write time */
	if (bt_stream_class_packets_have_end_default_clock_snapshot(sc)) {
		write_time(ctx,
			bt_message_packet_end_borrow_default_clock_snapshot_const(
				msg));
	}

	/* Write follow tag for message */
	ret = write_message_follow_tag(ctx, stream);
	if (ret) {
		goto end;
	}

	write_obj_type_name(ctx, "Packet end");
	write_nl(ctx);

end:
	return ret;
}

static
int write_message_iterator_inactivity_message(struct details_write_ctx *ctx,
		const bt_message *msg)
{
	int ret = 0;
	const bt_clock_snapshot *cs =
		bt_message_message_iterator_inactivity_borrow_default_clock_snapshot_const(
			msg);

	/* Write time */
	write_time(ctx, cs);
	write_obj_type_name(ctx, "Message iterator inactivity");

	if (ctx->details_comp->cfg.compact) {
		write_nl(ctx);
		goto end;
	}

	/* Write clock class properties */
	g_string_append(ctx->str, ":\n");
	incr_indent(ctx);
	write_indent(ctx);
	write_prop_name(ctx, "Clock class");
	g_string_append_c(ctx->str, ':');
	write_nl(ctx);
	incr_indent(ctx);
	write_clock_class_prop_lines(ctx,
		bt_clock_snapshot_borrow_clock_class_const(cs));
	decr_indent(ctx);

end:
	return ret;
}

BT_HIDDEN
int details_write_message(struct details_comp *details_comp,
		const bt_message *msg)
{
	int ret = 0;
	struct details_write_ctx ctx = {
		.details_comp = details_comp,
		.str = details_comp->str,
		.indent_level = 0,
	};

	/* Reset output buffer */
	g_string_assign(details_comp->str, "");

	if (details_comp->printed_something && !details_comp->cfg.compact) {
		write_nl(&ctx);
	}

	switch (bt_message_get_type(msg)) {
	case BT_MESSAGE_TYPE_EVENT:
		ret = write_event_message(&ctx, msg);
		break;
	case BT_MESSAGE_TYPE_MESSAGE_ITERATOR_INACTIVITY:
		ret = write_message_iterator_inactivity_message(&ctx, msg);
		break;
	case BT_MESSAGE_TYPE_STREAM_BEGINNING:
		ret = write_stream_beginning_message(&ctx, msg);
		break;
	case BT_MESSAGE_TYPE_STREAM_END:
		ret = write_stream_end_message(&ctx, msg);
		break;
	case BT_MESSAGE_TYPE_PACKET_BEGINNING:
		ret = write_packet_beginning_message(&ctx, msg);
		break;
	case BT_MESSAGE_TYPE_PACKET_END:
		ret = write_packet_end_message(&ctx, msg);
		break;
	case BT_MESSAGE_TYPE_DISCARDED_EVENTS:
		ret = write_discarded_events_message(&ctx, msg);
		break;
	case BT_MESSAGE_TYPE_DISCARDED_PACKETS:
		ret = write_discarded_packets_message(&ctx, msg);
		break;
	default:
		abort();
	}

	return ret;
}
