# The MIT License (MIT)
#
# Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>
# Copyright (c) 2018 Francis Deslauriers <francis.deslauriers@efficios.com>
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

__all__ = ['Trace']

import collections.abc
from bt2 import utils, native_bt
import bt2
from . import object


class Trace(object._SharedObject, collections.abc.Sequence):
    _GET_REF_NATIVE_FUNC = native_bt.trace_get_ref
    _PUT_REF_NATIVE_FUNC = native_bt.trace_put_ref

    def __getitem__(self, index):
        utils._check_uint64(index)

        if index >= len(self):
            raise IndexError

        stream_ptr = native_bt.trace_borrow_stream_by_index_const(self._ptr, index)
        assert stream_ptr
        return bt2.stream._Stream._create_from_ptr_and_get_ref(stream_ptr)

    def __len__(self):
        count = native_bt.trace_get_stream_count(self._ptr)
        assert count >= 0
        return count

    @property
    def name(self):
        return native_bt.trace_get_name(self._ptr)

    @name.setter
    def name(self, name):
        utils._check_str(name)
        ret = native_bt.trace_set_name(self._ptr, name)
        utils._handle_ret(ret, "cannot set trace class object's name")

    @property
    def packet_header_field_class(self):
        ft_ptr = native_bt.trace_borrow_packet_header_field_class(self._ptr)

        if ft_ptr is None:
            return
        native_bt.get(ft_ptr)

        return bt2.field_class._create_field_class_from_ptr(ft_ptr)

    @packet_header_field_class.setter
    def packet_header_field_class(self, packet_header_field_class):
        packet_header_field_class_ptr = None

        if packet_header_field_class is not None:
            utils._check_type(packet_header_field_class, field_class._FieldType)
            packet_header_field_class_ptr = packet_header_field_class._ptr

        ret = native_bt.trace_set_packet_header_field_class(self._ptr,
                                                     packet_header_field_class_ptr)
        utils._handle_ret(ret, "cannot set trace class object's packet header field type")

    def create_stream(self, stream_class, id=None):
        utils._check_type(stream_class, bt2.stream_class._StreamClass)

        if stream_class.assigns_automatic_stream_id:
            if id is not None:
                raise bt2.CreationError("id provided, but stream class assigns automatic stream ids")

            stream_ptr = native_bt.stream_create(stream_class._ptr, self._ptr)
        else:
            if id is None:
                raise bt2.CreationError("id not provided, but stream class does not assign automatic stream ids")

            utils._check_uint64(id)
            stream_ptr = native_bt.stream_create_with_id(stream_class._ptr, self._ptr, id)

        if stream_ptr is None:
            raise bt2.CreationError('cannot create stream object')

        return bt2.stream._Stream._create_from_ptr(stream_ptr)
