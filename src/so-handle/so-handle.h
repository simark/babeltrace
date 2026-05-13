/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2024 Brice Videau <bvideau@anl.gov>
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#ifndef BABELTRACE_SO_HANDLE_SO_HANDLE_H
#define BABELTRACE_SO_HANDLE_SO_HANDLE_H

#include <gmodule.h>
#include "common/object.h"

typedef void (*so_handle_finalize_func)(void);

struct so_handle {
	struct bt_object base;
	GString *path;
	GModule *module;
	int log_level;

	/*
	 * Functions of type `bt_plugin_finalize_func` called  when this SO
	 * handle is destroyed.
	 */
	GPtrArray *finalize_funcs;
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

#define SO_HANDLE_PUT_REF_AND_RESET(_so_handle)	\
	do {					\
		so_handle_put_ref(_so_handle);	\
		(_so_handle) = NULL;		\
	} while (0)

#endif /* BABELTRACE_SO_HANDLE_SO_HANDLE_H */
