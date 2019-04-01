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

from bt2 import native_bt, utils
import bt2.message_iterator
import bt2.port
import bt2


class _Connection(bt2.object._SharedObject):
    _GET_REF_NATIVE_FUNC = native_bt.connection_get_ref
    _PUT_REF_NATIVE_FUNC = native_bt.connection_put_ref

    @staticmethod
    def _downstream_port(ptr):
        port_ptr = native_bt.connection_borrow_downstream_port_const(ptr)
        utils._handle_ptr(port_ptr, "cannot get connection object's downstream port object")
        return bt2.port._create_from_ptr_and_get_ref(port_ptr, native_bt.PORT_TYPE_INPUT)

    @staticmethod
    def _upstream_port(ptr):
        port_ptr = native_bt.connection_borrow_upstream_port_const(ptr)
        utils._handle_ptr(port_ptr, "cannot get connection object's upstream port object")
        return bt2.port._create_from_ptr_and_get_ref(port_ptr,  native_bt.PORT_TYPE_OUTPUT)

    @property
    def downstream_port(self):
        return self._downstream_port(self._ptr)

    @property
    def upstream_port(self):
        return self._upstream_port(self._ptr)
