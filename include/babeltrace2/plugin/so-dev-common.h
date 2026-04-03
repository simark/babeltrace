/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2010-2026 EfficiOS Inc. and Linux Foundation
 */

#ifndef BABELTRACE2_PLUGIN_SO_DEV_COMMON_H
#define BABELTRACE2_PLUGIN_SO_DEV_COMMON_H

/* IWYU pragma: private, include <babeltrace2/babeltrace.h> */

#ifndef __BT_IN_BABELTRACE_H
# error "Please include <babeltrace2/babeltrace.h> instead."
#endif

#include <stdint.h>

#include <babeltrace2/graph/component-class-dev.h>
#include <babeltrace2/graph/message-iterator-class.h>
#include <babeltrace2/plugin/so-dev-common.h>
#include <babeltrace2/types.h>

/*
 * _BT_HIDDEN: set the hidden attribute for internal functions
 * On Windows, symbols are local unless explicitly exported,
 * see https://gcc.gnu.org/wiki/Visibility
 */
#if defined(_WIN32) || defined(__CYGWIN__)
#define _BT_HIDDEN
#else
#define _BT_HIDDEN __attribute__((visibility("hidden")))
#endif

/*
 * _BT_EXPORT: set the visibility for exported functions.
 */
#if defined(_WIN32) || defined(__CYGWIN__)
#define _BT_EXPORT
#else
#define _BT_EXPORT __attribute__((visibility("default")))
#endif

/* Object (user) version */
struct __bt_object_descriptor_version {
	uint32_t major;
	uint32_t minor;
	uint32_t patch;
	const char *extra;
};

#endif /* BABELTRACE2_PLUGIN_SO_DEV_COMMON_H */
