/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Francis Deslauriers <francis.deslauriers@efficios.com>
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
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/* Type */
struct bt_field_path;

/* Enumeration */
typedef enum bt_scope {
	BT_SCOPE_PACKET_CONTEXT,
	BT_SCOPE_EVENT_COMMON_CONTEXT,
	BT_SCOPE_EVENT_SPECIFIC_CONTEXT,
	BT_SCOPE_EVENT_PAYLOAD,
} bt_scope;

/* Functions */
enum bt_scope bt_field_path_get_root_scope(
		struct bt_field_path *field_path);

uint64_t bt_field_path_get_index_count(
		struct bt_field_path *field_path);

uint64_t bt_field_path_get_index_by_index(
		struct bt_field_path *field_path, uint64_t index);

extern void bt_field_path_get_ref(const bt_field_path *field_path);

extern void bt_field_path_put_ref(const bt_field_path *field_path);
