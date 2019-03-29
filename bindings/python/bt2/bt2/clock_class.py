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

__all__ = ['ClockClass', 'ClockClassOffset']

import uuid as uuidp
from bt2 import utils, native_bt
import bt2


class ClockClassOffset:
    def __init__(self, seconds=0, cycles=0):
        utils._check_int64(seconds)
        utils._check_int64(cycles)
        self._seconds = seconds
        self._cycles = cycles

    @property
    def seconds(self):
        return self._seconds

    @property
    def cycles(self):
        return self._cycles

    def __eq__(self, other):
        if not isinstance(other, self.__class__):
            # not comparing apples to apples
            return False

        return (self.seconds, self.cycles) == (other.seconds, other.cycles)


class ClockClass(bt2.object._SharedObject):
    _GET_REF_NATIVE_FUNC = native_bt.clock_class_get_ref
    _PUT_REF_NATIVE_FUNC = native_bt.clock_class_put_ref

    @property
    def name(self):
        return native_bt.clock_class_get_name(self._ptr)

    def _name(self, name):
        utils._check_str(name)
        ret = native_bt.clock_class_set_name(self._ptr, name)
        utils._handle_ret(ret, "cannot set clock class object's name")

    _name = property(fset=_name)

    @property
    def description(self):
        return native_bt.clock_class_get_description(self._ptr)

    @description.setter
    def description(self, description):
        utils._check_str(description)
        ret = native_bt.clock_class_set_description(self._ptr, description)
        utils._handle_ret(ret, "cannot set clock class object's description")

    @property
    def frequency(self):
        return native_bt.clock_class_get_frequency(self._ptr)

    @frequency.setter
    def frequency(self, frequency):
        utils._check_uint64(frequency)
        native_bt.clock_class_set_frequency(self._ptr, frequency)

    @property
    def precision(self):
        precision = native_bt.clock_class_get_precision(self._ptr)
        return precision

    @precision.setter
    def precision(self, precision):
        utils._check_uint64(precision)
        assert(precision >= 0)
        native_bt.clock_class_set_precision(self._ptr, precision)

    @property
    def offset(self):
        offset_s, offset_cycles = native_bt.clock_class_get_offset(self._ptr)
        return ClockClassOffset(offset_s, offset_cycles)

    @offset.setter
    def offset(self, offset):
        utils._check_type(offset, ClockClassOffset)
        native_bt.clock_class_set_offset(self._ptr, offset.seconds, offset.cycles)

    @property
    def origin_is_unix_epoch(self):
        return native_bt.clock_class_origin_is_unix_epoch(self._ptr)

    @origin_is_unix_epoch.setter
    def origin_is_unix_epoch(self, origin_is_unix_epoch):
        utils._check_bool(origin_is_unix_epoch)
        native_bt.clock_class_set_origin_is_unix_epoch(self._ptr, int(origin_is_unix_epoch))

    @property
    def uuid(self):
        uuid_bytes = native_bt.clock_class_get_uuid(self._ptr)

        if uuid_bytes is None:
            return

        return uuidp.UUID(bytes=uuid_bytes)

    @uuid.setter
    def uuid(self, uuid):
        utils._check_type(uuid, uuidp.UUID)
        native_bt.clock_class_set_uuid(self._ptr, uuid.bytes)
