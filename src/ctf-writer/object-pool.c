/*
 * Copyright (c) 2018 EfficiOS Inc. and Linux Foundation
 * Copyright (c) 2018 Philippe Proulx <pproulx@efficios.com>
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

#define BT_LOG_TAG "CTF-WRITER/OBJECT-POOL"
#include "logging.h"

#include <stdint.h>

#include "common/assert.h"

#include "object-pool.h"

int bt_ctf_object_pool_initialize(struct bt_ctf_object_pool *pool,
		bt_ctf_object_pool_new_object_func new_object_func,
		bt_ctf_object_pool_destroy_object_func destroy_object_func,
		void *data)
{
	int ret = 0;

	BT_ASSERT_DBG(new_object_func);
	BT_ASSERT_DBG(destroy_object_func);
	BT_LOGD("Initializing object pool: addr=%p, data-addr=%p",
		pool, data);
	pool->objects = g_ptr_array_new();
	if (!pool->objects) {
		BT_LOGE_STR("Failed to allocate a GPtrArray.");
		goto error;
	}

	pool->funcs.new_object = new_object_func;
	pool->funcs.destroy_object = destroy_object_func;
	pool->data = data;
	pool->size = 0;
	BT_LOGD("Initialized object pool.");
	goto end;

error:
	if (pool) {
		bt_ctf_object_pool_finalize(pool);
	}

	ret = -1;

end:
	return ret;
}

void bt_ctf_object_pool_finalize(struct bt_ctf_object_pool *pool)
{
	uint64_t i;

	BT_ASSERT_DBG(pool);
	BT_LOGD("Finalizing object pool.");

	if (pool->objects) {
		for (i = 0; i < pool->size; i++) {
			void *obj = pool->objects->pdata[i];

			if (obj) {
				pool->funcs.destroy_object(obj, pool->data);
			}
		}

		g_ptr_array_free(pool->objects, TRUE);
		pool->objects = NULL;
	}
}
