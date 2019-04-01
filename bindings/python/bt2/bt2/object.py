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

__all__ = ['_SharedObject', '_UniqueObject']


class _BaseObject:
    def __new__(cls, *args, **kwargs):
        obj = super().__new__(cls)
        obj._ptr = None
        return obj

    def __init__(self, ptr):
        self._ptr = ptr

    @property
    def addr(self):
        return int(self._ptr)

    def __repr__(self):
        return '<{}.{} object @ {}>'.format(self.__class__.__module__,
                                            self.__class__.__name__,
                                            hex(self.addr))

    def __copy__(self):
        raise NotImplementedError

    def __deepcopy__(self):
        raise NotImplementedError

    def __eq__(self, other):
        if isinstance(other, _BaseObject):
            return self.addr == other.addr

        return False


class _UniqueObject(_BaseObject):
    @classmethod
    def _create_from_ptr(cls, ptr_borrowed, owning_ptr_borrowed, owning_ptr_get_func, owning_ptr_put_func):
        obj = cls.__new__(cls)
        obj._ptr = ptr_borrowed
        obj._owning_ptr = owning_ptr_borrowed
        obj._owning_ptr_get_func = owning_ptr_get_func
        obj._owning_ptr_put_func = owning_ptr_put_func
        owning_ptr_get_func(obj._owning_ptr)
        return obj

    def __del__(self):
        owning_ptr = getattr(self, '_owning_ptr', None)
        self._owning_ptr = None
        self._owning_ptr_put_func(owning_ptr)


class _SharedObject(_BaseObject):
    @classmethod
    def _create_from_ptr(cls, ptr_owned):
        """Create a Python object wrapping a Babeltrace object pointer.

        This assumes that the caller owns a reference to the Babeltrace object
        and transfers this ownership to the newly created Python object"""
        obj = cls.__new__(cls)
        obj._ptr = ptr_owned
        return obj

    @classmethod
    def _create_from_ptr_and_get_ref(cls, ptr):
        obj = cls._create_from_ptr(ptr)
        cls._GET_REF_NATIVE_FUNC(obj._ptr)
        return obj

    def _get(self):
        self._GET_REF_NATIVE_FUNC(self._ptr)

    def __del__(self):
        ptr = getattr(self, '_ptr', None)
        self._PUT_REF_NATIVE_FUNC(ptr)
        self._ptr = None
