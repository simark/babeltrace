#ifndef BABELTRACE_GRAPH_PRIVATE_QUERY_EXECUTOR_H
#define BABELTRACE_GRAPH_PRIVATE_QUERY_EXECUTOR_H

/*
 * Copyright 2017 Philippe Proulx <pproulx@efficios.com>
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

/* For enum bt_query_executor_status */
#include <babeltrace/graph/query-executor.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_query_executor;
struct bt_private_query_executor;
struct bt_component_class;
struct bt_value;

static inline
struct bt_query_executor *bt_private_query_executor_as_query_executor(
		struct bt_private_query_executor *priv_query_executor)
{
	return (void *) priv_query_executor;
}

extern
struct bt_private_query_executor *bt_private_query_executor_create(void);

extern
enum bt_query_executor_status bt_private_query_executor_query(
		struct bt_private_query_executor *query_executor,
		struct bt_component_class *component_class,
		const char *object, const struct bt_value *params,
		const struct bt_value **result);

extern
enum bt_query_executor_status bt_private_query_executor_cancel(
		struct bt_private_query_executor *query_executor);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_GRAPH_PRIVATE_QUERY_EXECUTOR_H */