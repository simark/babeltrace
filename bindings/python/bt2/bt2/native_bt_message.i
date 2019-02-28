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

/* Types */

struct bt_message;

typedef enum bt_message_type {
	BT_MESSAGE_TYPE_EVENT = 0,
	BT_MESSAGE_TYPE_MESSAGE_ITERATOR_INACTIVITY = 1,
	BT_MESSAGE_TYPE_STREAM_BEGINNING = 2,
	BT_MESSAGE_TYPE_STREAM_END = 3,
	BT_MESSAGE_TYPE_PACKET_BEGINNING = 4,
	BT_MESSAGE_TYPE_PACKET_END = 5,
	BT_MESSAGE_TYPE_STREAM_ACTIVITY_BEGINNING = 6,
	BT_MESSAGE_TYPE_STREAM_ACTIVITY_END = 7,
	BT_MESSAGE_TYPE_DISCARDED_EVENTS = 8,
	BT_MESSAGE_TYPE_DISCARDED_PACKETS = 9,
} bt_message_type;

typedef enum bt_message_stream_activity_clock_snapshot_state {
	BT_MESSAGE_STREAM_ACTIVITY_CLOCK_SNAPSHOT_STATE_KNOWN,
	BT_MESSAGE_STREAM_ACTIVITY_CLOCK_SNAPSHOT_STATE_UNKNOWN,
	BT_MESSAGE_STREAM_ACTIVITY_CLOCK_SNAPSHOT_STATE_INFINITE,
} bt_message_stream_activity_clock_snapshot_state;

/* Functions */

extern bt_message_type bt_message_get_type(const bt_message *message);

extern void bt_message_get_ref(const bt_message *message);

extern void bt_message_put_ref(const bt_message *message);

/* Stream beginning */

extern
bt_message *bt_message_stream_beginning_create(
		bt_self_message_iterator *message_iterator,
		const bt_stream *stream);

extern bt_stream *bt_message_stream_beginning_borrow_stream(
		bt_message *message);

/* Stream activity beginning */

extern bt_message *bt_message_stream_activity_beginning_create(
		bt_self_message_iterator *message_iterator,
		const bt_stream *stream);

extern bt_stream *bt_message_stream_activity_beginning_borrow_stream(
		bt_message *message);

extern void bt_message_stream_activity_beginning_set_default_clock_snapshot_state(
		bt_message *msg,
		bt_message_stream_activity_clock_snapshot_state state);

extern void bt_message_stream_activity_beginning_set_default_clock_snapshot(
		bt_message *msg, uint64_t raw_value);

extern bt_message_stream_activity_clock_snapshot_state
bt_message_stream_activity_beginning_borrow_default_clock_snapshot_const(
		const bt_message *msg, const bt_clock_snapshot **BTOUTCLOCKSNAPSHOT);

extern const bt_clock_class *
bt_message_stream_activity_beginning_borrow_stream_class_default_clock_class_const(
		const bt_message *msg);

extern const bt_stream *
bt_message_stream_activity_beginning_borrow_stream_const(
		const bt_message *message);

/* Stream activity end */

extern bt_message *bt_message_stream_activity_end_create(
		bt_self_message_iterator *message_iterator,
		const bt_stream *stream);

extern void bt_message_stream_activity_end_set_default_clock_snapshot_state(
		bt_message *msg,
		bt_message_stream_activity_clock_snapshot_state state);

extern void bt_message_stream_activity_end_set_default_clock_snapshot(
		bt_message *msg, uint64_t raw_value);

extern bt_stream *bt_message_stream_activity_end_borrow_stream(
		bt_message *message);

extern bt_message_stream_activity_clock_snapshot_state
bt_message_stream_activity_end_borrow_default_clock_snapshot_const(
		const bt_message *msg, const bt_clock_snapshot **BTOUTCLOCKSNAPSHOT);

extern const bt_clock_class *
bt_message_stream_activity_end_borrow_stream_class_default_clock_class_const(
		const bt_message *msg);

extern const bt_stream *
bt_message_stream_activity_end_borrow_stream_const(
		const bt_message *message);

/* Packet beginning */

extern
bt_message *bt_message_packet_beginning_create(
		bt_self_message_iterator *message_iterator,
		const bt_packet *packet);

extern
bt_message *bt_message_packet_beginning_create_with_default_clock_snapshot(
		bt_self_message_iterator *message_iterator,
		const bt_packet *packet, uint64_t raw_value);

extern bt_packet *bt_message_packet_beginning_borrow_packet(
		bt_message *message);

/* Event */

extern
bt_message *bt_message_event_create(
		bt_self_message_iterator *message_iterator,
		const bt_event_class *event_class,
		const bt_packet *packet);

extern
bt_message *bt_message_event_create_with_default_clock_snapshot(
		bt_self_message_iterator *message_iterator,
		const bt_event_class *event_class,
		const bt_packet *packet, uint64_t raw_clock_value);

extern bt_event *bt_message_event_borrow_event(
		bt_message *message);

extern const bt_event *bt_message_event_borrow_event_const(
		const bt_message *message);

extern bt_clock_snapshot_state
bt_message_event_borrow_default_clock_snapshot_const(
		const bt_message *msg, const bt_clock_snapshot **BTOUTCLOCKSNAPSHOT);

extern const bt_clock_class *
bt_message_event_borrow_stream_class_default_clock_class_const(
		const bt_message *msg);

/* Packet end */

extern
bt_message *bt_message_packet_end_create(
		bt_self_message_iterator *message_iterator,
		const bt_packet *packet);

extern
bt_message *bt_message_packet_end_create_with_default_clock_snapshot(
		bt_self_message_iterator *message_iterator,
		const bt_packet *packet, uint64_t raw_value);

extern bt_packet *bt_message_packet_end_borrow_packet(
		bt_message *message);

/* Stream end */

extern
bt_message *bt_message_stream_end_create(
		bt_self_message_iterator *message_iterator,
		const bt_stream *stream);

extern bt_stream *bt_message_stream_end_borrow_stream(
		bt_message *message);

/* Message iterator inactivity */

extern
bt_message *bt_message_message_iterator_inactivity_create(
		bt_self_message_iterator *message_iterator,
		const bt_clock_class *default_clock_class, uint64_t raw_value);

extern bt_clock_snapshot_state
bt_message_message_iterator_inactivity_borrow_default_clock_snapshot_const(
		const bt_message *msg, const bt_clock_snapshot **BTOUTCLOCKSNAPSHOT);

///* Event notification functions */
//struct bt_notification *bt_notification_event_create(
//		struct bt_private_connection_private_notification_iterator *notification_iterator,
//		struct bt_event_class *event_class, struct bt_packet *packet);
//struct bt_event *bt_notification_event_borrow_event(
//		struct bt_notification *notification);
//
///* Inactivity notification functions */
//struct bt_notification *bt_notification_inactivity_create(
//		struct bt_private_connection_private_notification_iterator *notification_iterator,
//		struct bt_clock_class *default_clock_class);
//int bt_notification_inactivity_set_default_clock_value(
//		struct bt_notification *notif, uint64_t raw_value);
//struct bt_clock_value *bt_notification_inactivity_borrow_default_clock_value(
//		struct bt_notification *notif);
//
///* Packet notification functions */
//struct bt_notification *bt_notification_packet_begin_create(
//		struct bt_private_connection_private_notification_iterator *notification_iterator,
//		struct bt_packet *packet);
//struct bt_notification *bt_notification_packet_end_create(
//		struct bt_private_connection_private_notification_iterator *notification_iterator,
//		struct bt_packet *packet);
//struct bt_packet *bt_notification_packet_begin_borrow_packet(
//		struct bt_notification *notification);
//struct bt_packet *bt_notification_packet_end_borrow_packet(
//		struct bt_notification *notification);
//struct bt_packet *bt_notification_packet_begin_get_packet(
//		struct bt_notification *notification);
//struct bt_packet *bt_notification_packet_end_get_packet(
//		struct bt_notification *notification);
//
///* Stream notification functions */
//struct bt_notification *bt_notification_stream_begin_create(
//		struct bt_private_connection_private_notification_iterator *notification_iterator,
//		struct bt_stream *stream);
//struct bt_notification *bt_notification_stream_end_create(
//		struct bt_private_connection_private_notification_iterator *notification_iterator,
//		struct bt_stream *stream);
//struct bt_stream *bt_notification_stream_begin_borrow_stream(
//		struct bt_notification *notification);
//struct bt_stream *bt_notification_stream_begin_get_stream(
//		struct bt_notification *notification);
//int bt_notification_stream_begin_set_default_clock_value(
//		struct bt_notification *notif, uint64_t value_cycles);
//struct bt_clock_value *bt_notification_stream_begin_borrow_default_clock_value(
//		struct bt_notification *notif);
//struct bt_stream *bt_notification_stream_end_borrow_stream(
//		struct bt_notification *notification);
//struct bt_stream *bt_notification_stream_end_get_stream(
//		struct bt_notification *notification);
//int bt_notification_stream_end_set_default_clock_value(
//		struct bt_notification *notif, uint64_t value_cycles);
//struct bt_clock_value *bt_notification_stream_end_borrow_default_clock_value(
//		struct bt_notification *notif);
