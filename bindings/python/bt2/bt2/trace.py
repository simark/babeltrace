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
import functools
from . import object


def _trace_destruction_listener_from_native(user_listener, trace_ptr):
    trace = bt2.trace.Trace._create_from_ptr_and_get_ref(trace_ptr)
    user_listener(trace)


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

    def _name(self, name):
        utils._check_str(name)
        ret = native_bt.trace_set_name(self._ptr, name)
        utils._handle_ret(ret, "cannot set trace class object's name")

    _name = property(fset=_name)

    def create_stream(self, stream_class, id=None, name=None):
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

        stream = bt2.stream._Stream._create_from_ptr(stream_ptr)

        if name is not None:
            stream._name = name

        return stream

    def add_destruction_listener(self, listener):
        '''Add a listener to be called when the trace is destroyed.'''
        if not callable(listener):
            raise TypeError("'listener' parameter is not callable")

        fn = native_bt.py3_trace_add_destruction_listener
        listener_from_native = functools.partial(_trace_destruction_listener_from_native,
                                                 listener)

        listener_id = fn(self._ptr, listener_from_native)
        if listener_id is None:
            utils._raise_bt2_error('cannot add destruction listener to trace object')

        return bt2._ListenerHandle(listener_id, self)
