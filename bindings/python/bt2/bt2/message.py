# The MIT License (MIT)
#
# Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

import copy
import collections
from bt2 import native_bt, utils
from bt2 import object
import bt2.clock_snapshot
import bt2.event
import bt2.packet
import bt2.stream
import bt2


def _create_from_ptr(ptr):
    notif_type = native_bt.message_get_type(ptr)
    cls = None

    if notif_type not in _NOTIF_TYPE_TO_CLS:
        raise bt2.Error('unknown message type: {}'.format(notif_type))

    return _NOTIF_TYPE_TO_CLS[notif_type]._create_from_ptr(ptr)


class _Message(object._SharedObject):
    pass


class _EventMessage(_Message):
    _TYPE = native_bt.MESSAGE_TYPE_EVENT

    def __init__(self, priv_conn_priv_iter, event_class, packet):
        utils._check_type(event_class, bt2.event_class._EventClass)

        ptr = native_bt.message_event_create(priv_conn_priv_iter._ptr,
                event_class._ptr, packet._ptr)

        if ptr is None:
            raise bt2.CreationError('cannot create event message object')

        super().__init__(ptr)

    @property
    def event(self):
        event_ptr = native_bt.message_event_borrow_event(self._ptr)
        assert(event_ptr)
        return bt2.event._create_event_from_ptr(event_ptr, self._ptr)


class _PacketBeginningMessage(_Message):
    _TYPE = native_bt.MESSAGE_TYPE_PACKET_BEGINNING

    def __init__(self, priv_conn_priv_iter, packet):
        utils._check_type(packet, bt2.packet._Packet)
        ptr = native_bt.message_packet_begin_create(priv_conn_priv_iter._ptr, packet._ptr)

        if ptr is None:
            raise bt2.CreationError('cannot create packet beginning message object')

        super().__init__(ptr)

    @property
    def packet(self):
        packet_ptr = native_bt.message_packet_begin_get_packet(self._ptr)
        assert(packet_ptr)
        return bt2.packet._Packet._create_from_ptr(packet_ptr)


class _PacketEndMessage(_Message):
    _TYPE = native_bt.MESSAGE_TYPE_PACKET_END

    def __init__(self, priv_conn_priv_iter, packet):
        utils._check_type(packet, bt2.packet._Packet)
        ptr = native_bt.message_packet_end_create(priv_conn_priv_iter._ptr, packet._ptr)

        if ptr is None:
            raise bt2.CreationError('cannot create packet end message object')

        super().__init__(ptr)

    @property
    def packet(self):
        packet_ptr = native_bt.message_packet_end_get_packet(self._ptr)
        assert(packet_ptr)
        return bt2.packet._Packet._create_from_ptr(packet_ptr)


class _StreamBeginningMessage(_Message):
    _TYPE = native_bt.MESSAGE_TYPE_STREAM_BEGINNING

    def __init__(self, priv_conn_priv_iter, stream):
        utils._check_type(stream, bt2.stream._Stream)
        ptr = native_bt.message_stream_begin_create(priv_conn_priv_iter._ptr, stream._ptr)

        if ptr is None:
            raise bt2.CreationError('cannot create stream beginning message object')

        super().__init__(ptr)

    @property
    def stream(self):
        stream_ptr = native_bt.message_stream_begin_get_stream(self._ptr)
        assert(stream_ptr)
        return bt2.stream._create_stream_from_ptr(stream_ptr)

    @property
    def default_clock_snapshot(self):
        value_ptr = native_bt.message_stream_begin_borrow_default_clock_snapshot(self._ptr)

        if value_ptr is None:
            return

        return bt2.clock_snapshot._ClockSnapshot._create_from_ptr(value_ptr, self._ptr)

    @default_clock_snapshot.setter
    def default_clock_snapshot(self, value):
        ret = native_bt.message_stream_begin_set_default_clock_snapshot(self._ptr, value)
        utils._handle_ret(ret, "cannot set stream begin message clock value")


class _StreamEndMessage(_Message):
    _TYPE = native_bt.MESSAGE_TYPE_STREAM_END

    def __init__(self, priv_conn_priv_iter, stream):
        utils._check_type(stream, bt2.stream._Stream)
        ptr = native_bt.message_stream_end_create(priv_conn_priv_iter._ptr, stream._ptr)

        if ptr is None:
            raise bt2.CreationError('cannot create stream end message object')

        super().__init__(ptr)

    @property
    def stream(self):
        stream_ptr = native_bt.message_stream_end_get_stream(self._ptr)
        assert(stream_ptr)
        return bt2.stream._create_stream_from_ptr(stream_ptr)

    @property
    def default_clock_snapshot(self):
        value_ptr = native_bt.message_stream_end_borrow_default_clock_snapshot(self._ptr)

        if value_ptr is None:
            return

        return bt2.clock_snapshot._ClockSnapshot._create_from_ptr(value_ptr, self._ptr)

    @default_clock_snapshot.setter
    def default_clock_snapshot(self, value):
        ret = native_bt.message_stream_end_set_default_clock_snapshot(self._ptr, value)
        utils._handle_ret(ret, "cannot set stream end message clock value")


class _InactivityMessage(_Message):
    _TYPE = native_bt.MESSAGE_TYPE_MESSAGE_ITERATOR_INACTIVITY

    def __init__(self, priv_conn_priv_iter, clock_class):
        ptr = native_bt.message_inactivity_create(priv_conn_priv_iter._ptr, clock_class._ptr)

        if ptr is None:
            raise bt2.CreationError('cannot create inactivity message object')

        super().__init__(ptr)

    @property
    def default_clock_snapshot(self):
        value_ptr = native_bt.message_inactivity_borrow_default_clock_snapshot(self._ptr)

        if value_ptr is None:
            return

        return bt2.clock_snapshot._ClockSnapshot._create_from_ptr(value_ptr, self._ptr)

    @default_clock_snapshot.setter
    def default_clock_snapshot(self, value):
        ret = native_bt.message_inactivity_set_default_clock_snapshot(self._ptr, value)
        utils._handle_ret(ret, "cannot set stream end message clock value")


_NOTIF_TYPE_TO_CLS = {
    native_bt.MESSAGE_TYPE_EVENT: _EventMessage,
    native_bt.MESSAGE_TYPE_PACKET_BEGINNING: _PacketBeginningMessage,
    native_bt.MESSAGE_TYPE_PACKET_END: _PacketEndMessage,
    native_bt.MESSAGE_TYPE_STREAM_BEGINNING: _StreamBeginningMessage,
    native_bt.MESSAGE_TYPE_STREAM_END: _StreamEndMessage,
    native_bt.MESSAGE_TYPE_MESSAGE_ITERATOR_INACTIVITY: _InactivityMessage,
}