/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2026 Efficios Inc.
 */

#ifndef BABELTRACE_COMMON_LOG_AND_APPEND_H
#define BABELTRACE_COMMON_LOG_AND_APPEND_H

#include <babeltrace2/babeltrace.h>
#include "logging/log.h"

#define BT_LOG_AND_APPEND(_lvl, _module_name, _fmt, ...)			\
	do {									\
		BT_LOG_WRITE_PRINTF(_lvl, BT_LOG_TAG, _fmt, ##__VA_ARGS__);	\
		(void) BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_UNKNOWN(	\
			(_module_name), _fmt, ##__VA_ARGS__);			\
	} while (0)

#endif /* BABELTRACE_COMMON_LOG_AND_APPEND_H */
