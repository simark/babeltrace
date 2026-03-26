/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2017 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_CLI_LOGGING_H
#define BABELTRACE_CLI_LOGGING_H

#define BT_LOG_OUTPUT_LEVEL bt_cli_log_level
#include "common/log-and-append.h"
#include "logging/log-api.h"

BT_LOG_LEVEL_EXTERN_SYMBOL(bt_cli_log_level);

/*
 * Logs with level `_lvl` and appends a cause from module `_module_name`
 * to the error of the current thread.
 */
#define BT_CLI_LOG_AND_APPEND(_lvl, _fmt, ...)				\
	BT_LOG_AND_APPEND(_lvl, "Babeltrace CLI", _fmt, ##__VA_ARGS__)

#define BT_CLI_LOGE_APPEND_CAUSE(_fmt, ...)				\
	BT_CLI_LOG_AND_APPEND(BT_LOG_ERROR, _fmt, ##__VA_ARGS__)
#define BT_CLI_LOGW_APPEND_CAUSE(_fmt, ...)				\
	BT_CLI_LOG_AND_APPEND(BT_LOG_WARNING, _fmt, ##__VA_ARGS__)

#endif /* BABELTRACE_CLI_LOGGING_H */
