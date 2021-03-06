#ifndef BABELTRACE2_TRACE_IR_EVENT_CLASS_CONST_H
#define BABELTRACE2_TRACE_IR_EVENT_CLASS_CONST_H

/*
 * Copyright (c) 2010-2019 EfficiOS Inc. and Linux Foundation
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

#ifndef __BT_IN_BABELTRACE_H
# error "Please include <babeltrace2/babeltrace.h> instead."
#endif

#include <stdint.h>
#include <stddef.h>

#include <babeltrace2/property.h>
#include <babeltrace2/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum bt_event_class_log_level {
	BT_EVENT_CLASS_LOG_LEVEL_EMERGENCY		= 0,
	BT_EVENT_CLASS_LOG_LEVEL_ALERT			= 1,
	BT_EVENT_CLASS_LOG_LEVEL_CRITICAL		= 2,
	BT_EVENT_CLASS_LOG_LEVEL_ERROR			= 3,
	BT_EVENT_CLASS_LOG_LEVEL_WARNING		= 4,
	BT_EVENT_CLASS_LOG_LEVEL_NOTICE			= 5,
	BT_EVENT_CLASS_LOG_LEVEL_INFO			= 6,
	BT_EVENT_CLASS_LOG_LEVEL_DEBUG_SYSTEM		= 7,
	BT_EVENT_CLASS_LOG_LEVEL_DEBUG_PROGRAM		= 8,
	BT_EVENT_CLASS_LOG_LEVEL_DEBUG_PROCESS		= 9,
	BT_EVENT_CLASS_LOG_LEVEL_DEBUG_MODULE		= 10,
	BT_EVENT_CLASS_LOG_LEVEL_DEBUG_UNIT		= 11,
	BT_EVENT_CLASS_LOG_LEVEL_DEBUG_FUNCTION		= 12,
	BT_EVENT_CLASS_LOG_LEVEL_DEBUG_LINE		= 13,
	BT_EVENT_CLASS_LOG_LEVEL_DEBUG			= 14,
} bt_event_class_log_level;

extern const bt_value *bt_event_class_borrow_user_attributes_const(
		const bt_event_class *event_class);

extern const bt_stream_class *bt_event_class_borrow_stream_class_const(
		const bt_event_class *event_class);

extern const char *bt_event_class_get_name(const bt_event_class *event_class);

extern uint64_t bt_event_class_get_id(const bt_event_class *event_class);

extern bt_property_availability bt_event_class_get_log_level(
		const bt_event_class *event_class,
		bt_event_class_log_level *log_level);

extern const char *bt_event_class_get_emf_uri(
		const bt_event_class *event_class);

extern const bt_field_class *
bt_event_class_borrow_specific_context_field_class_const(
		const bt_event_class *event_class);

extern const bt_field_class *bt_event_class_borrow_payload_field_class_const(
		const bt_event_class *event_class);

extern void bt_event_class_get_ref(const bt_event_class *event_class);

extern void bt_event_class_put_ref(const bt_event_class *event_class);

#define BT_EVENT_CLASS_PUT_REF_AND_RESET(_var)		\
	do {						\
		bt_event_class_put_ref(_var);		\
		(_var) = NULL;				\
	} while (0)

#define BT_EVENT_CLASS_MOVE_REF(_var_dst, _var_src)	\
	do {						\
		bt_event_class_put_ref(_var_dst);	\
		(_var_dst) = (_var_src);		\
		(_var_src) = NULL;			\
	} while (0)

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_TRACE_IR_EVENT_CLASS_CONST_H */
