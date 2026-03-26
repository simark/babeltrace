/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2017 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_CLI_LOGGING_H
#define BABELTRACE_CLI_LOGGING_H

#define BT_LOG_OUTPUT_LEVEL bt_cli_log_level
#include "logging/log.h"

#include "common/log-and-append.h"

BT_LOG_LEVEL_EXTERN_SYMBOL(bt_cli_log_level);

#define BT_CLI_LOG_AND_APPEND(_lvl, _fmt, ...)				\
	BT_LOG_AND_APPEND(_lvl, "Babeltrace CLI", _fmt, ##__VA_ARGS__)

#define BT_CLI_LOGE_APPEND_CAUSE(_fmt, ...)				\
	BT_CLI_LOG_AND_APPEND(BT_LOG_ERROR, _fmt, ##__VA_ARGS__)
#define BT_CLI_LOGW_APPEND_CAUSE(_fmt, ...)				\
	BT_CLI_LOG_AND_APPEND(BT_LOG_WARNING, _fmt, ##__VA_ARGS__)

#endif /* BABELTRACE_CLI_LOGGING_H */
