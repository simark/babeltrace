/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2024 Brice Videau <bvideau@anl.gov>
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#ifndef BABELTRACE_COMMON_SO_HANDLE_H
#define BABELTRACE_COMMON_SO_HANDLE_H

#include <gmodule.h>
#include "common/object.h"

typedef void (*so_handle_finalize_func)(void);

struct so_handle {
	struct bt_object base;
	GString *path;
	GModule *module;
	int log_level;

	/* True if initialization function was called */
	/* FIXME: plugin-provider-so.c doesn't appear to set this */
	bool init_called;
	so_handle_finalize_func exit;
};

int create_so_handle(const char *path, int log_level,
		struct so_handle **so_handle);

static inline
void so_handle_get_ref(const struct so_handle *so_handle)
{
	bt_object_get_ref(so_handle);
}

static inline
void so_handle_put_ref(const struct so_handle *so_handle)
{
	bt_object_put_ref(so_handle);
}

#endif /* BABELTRACE_COMMON_SO_HANDLE_H */
