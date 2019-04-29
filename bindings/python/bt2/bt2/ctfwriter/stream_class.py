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

from bt2 import native_bt, utils, ctfwriter
import collections.abc
import bt2.stream
import copy
import bt2


class _EventClassIterator(collections.abc.Iterator):
    def __init__(self, stream_class):
        self._stream_class = stream_class
        self._at = 0

    def __next__(self):
        if self._at == len(self._stream_class):
            raise StopIteration

        ec_ptr = native_bt.ctf_stream_class_get_event_class_by_index(self._stream_class._ptr,
                                                                 self._at)
        assert(ec_ptr)
        ev_id = native_bt.ctf_event_class_get_id(ec_ptr)
        native_bt.ctf_object_put_ref(ec_ptr)
        utils._handle_ret(ev_id, "cannot get event class object's ID")
        self._at += 1
        return ev_id


class StreamClass(ctfwriter.object._CtfWriterSharedObject, collections.abc.Mapping):
    def __init__(self, name, id=None, packet_context_field_type=None,
                 event_header_field_type=None, event_context_field_type=None,
                 event_classes=None):
        ptr = native_bt.ctf_stream_class_create(name)

        if ptr is None:
            raise bt2.CreationError('cannot create stream class object')

        super().__init__(ptr)

        if id is not None:
            self.id = id

        if packet_context_field_type is not None:
            self.packet_context_field_type = packet_context_field_type

        if event_header_field_type is not None:
            self.event_header_field_type = event_header_field_type

        if event_context_field_type is not None:
            self.event_context_field_type = event_context_field_type

        if event_classes is not None:
            for event_class in event_classes:
                self.add_event_class(event_class)

        self._next_stream_id = 0

    def __getitem__(self, key):
        utils._check_int64(key)
        ec_ptr = native_bt.ctf_stream_class_get_event_class_by_id(self._ptr,
                                                              key)

        if ec_ptr is None:
            raise KeyError(key)

        return bt2.ctfwriter.EventClass._create_from_ptr(ec_ptr)

    def __len__(self):
        count = native_bt.ctf_stream_class_get_event_class_count(self._ptr)
        assert(count >= 0)
        return count

    def __iter__(self):
        return _EventClassIterator(self)

    def add_event_class(self, event_class):
        utils._check_type(event_class, bt2.ctfwriter.EventClass)
        ret = native_bt.ctf_stream_class_add_event_class(self._ptr, event_class._ptr)
        utils._handle_ret(ret, "cannot add event class object to stream class object's")

    @property
    def trace(self):
        tc_ptr = native_bt.ctf_stream_class_get_trace(self._ptr)

        if tc_ptr is not None:
            return bt2.ctfwriter.Trace._create_from_ptr(tc_ptr)

    @property
    def name(self):
        return native_bt.ctf_stream_class_get_name(self._ptr)

    @name.setter
    def name(self, name):
        utils._check_str(name)
        ret = native_bt.ctf_stream_class_set_name(self._ptr, name)
        utils._handle_ret(ret, "cannot set stream class object's name")

    @property
    def id(self):
        id = native_bt.ctf_stream_class_get_id(self._ptr)

        if id < 0:
            return

        return id

    @id.setter
    def id(self, id):
        utils._check_int64(id)
        ret = native_bt.ctf_stream_class_set_id(self._ptr, id)
        utils._handle_ret(ret, "cannot set stream class object's ID")

    @property
    def clock(self):
        clock_ptr = native_bt.ctf_stream_class_get_clock(self._ptr)

        if clock_ptr is None:
            return

        return bt2.ctfwriter.clock.Clock._create_from_ptr(clock_ptr)

    @clock.setter
    def clock(self, clock):
        utils._check_type(clock, bt2.ctfwriter.clock.Clock)
        ret = native_bt.ctf_stream_class_set_clock(self._ptr, clock._ptr)
        utils._handle_ret(ret, "cannot set stream class object's CTF writer clock object")

    @property
    def packet_context_field_type(self):
        ft_ptr = native_bt.ctf_stream_class_get_packet_context_type(self._ptr)

        if ft_ptr is None:
            return

        return bt2.ctfwriter.field_types._create_from_ptr(ft_ptr)

    @packet_context_field_type.setter
    def packet_context_field_type(self, packet_context_field_type):
        packet_context_field_type_ptr = None

        if packet_context_field_type is not None:
            utils._check_type(packet_context_field_type, bt2.ctfwriter.field_types._FieldType)
            packet_context_field_type_ptr = packet_context_field_type._ptr

        ret = native_bt.ctf_stream_class_set_packet_context_type(self._ptr,
                                                             packet_context_field_type_ptr)
        utils._handle_ret(ret, "cannot set stream class object's packet context field type")

    @property
    def event_header_field_type(self):
        ft_ptr = native_bt.ctf_stream_class_get_event_header_type(self._ptr)

        if ft_ptr is None:
            return

        return bt2.ctfwriter.field_types._create_from_ptr(ft_ptr)

    @event_header_field_type.setter
    def event_header_field_type(self, event_header_field_type):
        event_header_field_type_ptr = None

        if event_header_field_type is not None:
            utils._check_type(event_header_field_type, bt2.ctfwriter.field_types._FieldType)
            event_header_field_type_ptr = event_header_field_type._ptr

        ret = native_bt.ctf_stream_class_set_event_header_type(self._ptr,
                                                           event_header_field_type_ptr)
        utils._handle_ret(ret, "cannot set stream class object's event header field type")

    @property
    def event_context_field_type(self):
        ft_ptr = native_bt.ctf_stream_class_get_event_context_type(self._ptr)

        if ft_ptr is None:
            return

        return bt2.ctfwriter.field_types._create_from_ptr(ft_ptr)

    @event_context_field_type.setter
    def event_context_field_type(self, event_context_field_type):
        event_context_field_type_ptr = None

        if event_context_field_type is not None:
            utils._check_type(event_context_field_type, bt2.ctfwriter.field_types._FieldType)
            event_context_field_type_ptr = event_context_field_type._ptr

        ret = native_bt.ctf_stream_class_set_event_context_type(self._ptr,
                                                            event_context_field_type_ptr)
        utils._handle_ret(ret, "cannot set stream class object's event context field type")

    def __call__(self, name=None, id=None):
        if name is not None:
            utils._check_str(name)

        if id is None:
            id = self._next_stream_id
            self._next_stream_id += 1

        stream_ptr = native_bt.ctf_stream_create(self._ptr, name, id)

        if stream_ptr is None:
            raise bt2.CreationError('cannot create stream object')

        return bt2.ctfwriter.stream._Stream._create_from_ptr(stream_ptr)
