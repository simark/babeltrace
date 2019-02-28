/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>
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
struct bt_port;
struct bt_self_component;

/* Status */
//enum bt_port_status {
//	BT_PORT_STATUS_OK = 0,
//	BT_PORT_STATUS_ERROR = -1,
//	BT_PORT_STATUS_INVALID = -2,
//};

/* Port type */
typedef enum bt_port_type {
	BT_PORT_TYPE_INPUT = 0,
	BT_PORT_TYPE_OUTPUT = 1,
} bt_port_type;

/* Functions (public) */

extern const char *bt_port_get_name(const bt_port *port);

extern bt_port_type bt_port_get_type(const bt_port *port);

extern const bt_connection *bt_port_borrow_connection_const(
		const bt_port *port);

extern const bt_component *bt_port_borrow_component_const(
		const bt_port *port);

extern bt_bool bt_port_is_connected(const bt_port *port);

static inline
bt_bool bt_port_is_input(const bt_port *port)
{
	return bt_port_get_type(port) == BT_PORT_TYPE_INPUT;
}

static inline
bt_bool bt_port_is_output(const bt_port *port)
{
	return bt_port_get_type(port) == BT_PORT_TYPE_OUTPUT;
}

const bt_port *bt_port_input_as_port_const(const bt_port_input *port_input);
const bt_port *bt_port_output_as_port_const(const bt_port_output *port_input);

///* Functions (private) */

extern bt_self_component *bt_self_component_port_borrow_component(
		bt_self_component_port *self_port);

extern void *bt_self_component_port_get_data(
		const bt_self_component_port *self_port);

const bt_port *bt_self_component_port_as_port(
		bt_self_component_port *self_port);

bt_self_component_port *
bt_self_component_port_output_as_self_component_port(
		bt_self_component_port_output *self_component_port);

bt_self_component_port *
bt_self_component_port_input_as_self_component_port(
		bt_self_component_port_input *self_component_port);

extern void bt_port_get_ref(const bt_port *port);

extern void bt_port_put_ref(const bt_port *port);

/* Input port message iterator */

extern bt_self_component_port_input_message_iterator *
bt_self_component_port_input_message_iterator_create(
		bt_self_component_port_input *input_port);

extern void bt_self_component_port_input_message_iterator_get_ref(
		const bt_self_component_port_input_message_iterator *self_component_port_input_message_iterator);

extern void bt_self_component_port_input_message_iterator_put_ref(
		const bt_self_component_port_input_message_iterator *self_component_port_input_message_iterator);

//struct bt_port *bt_port_borrow_from_private(
//		struct bt_private_port *private_port);
//struct bt_private_connection *bt_private_port_get_private_connection(
//		struct bt_private_port *private_port);
//struct bt_private_component *bt_private_port_get_private_component(
//		struct bt_private_port *private_port);
//enum bt_port_status bt_private_port_remove_from_component(
//		struct bt_private_port *private_port);
//void *bt_private_port_get_user_data(
//		struct bt_private_port *private_port);
//struct bt_notification_iterator *bt_output_port_notification_iterator_create(
//		struct bt_port *port, const char *colander_component_name);
//enum bt_notification_iterator_status bt_output_port_notification_iterator_next(
//		struct bt_notification_iterator *iterator,
//		bt_notification_array *notifs, uint64_t *count);


