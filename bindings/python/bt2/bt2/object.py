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


class _BaseObject:

    @property
    def addr(self):
        return int(self._ptr)

    def __repr__(self):
        return '<{}.{} object @ {}>'.format(self.__class__.__module__,
                                            self.__class__.__name__,
                                            hex(self.addr))


class _UniqueObject(_BaseObject):
    """A Python object that is itself not refcounted, but is wholly owned by
    an object that is itself refcounted (a _SharedObject).  A Babeltrace unique
    object gets destroyed once its owner gets destroyed (its refcount drops to
    0).

    In the Python bindings, to avoid having to deal with issues with the
    lifetime of unique objects, we make it so acquiring a reference on a unique
    object acquires a reference on its owner."""

    @classmethod
    def _create_from_ptr_and_get_ref(cls, ptr, owner_ptr,
                                     owner_get_ref, owner_put_ref):
        """Create a _UniqueObject.

        - ptr: Swig Object, pointer to the unique object.
        - owner_ptr: Swig Object, pointer to the owner of the unique
          object.  A new reference is acquired.
        - owner_get_ref: Callback to get a reference on the owner
        - owner_put_ref: Callback to put a reference on the owner.
        """
        obj = cls.__new__(cls)
        obj._ptr = ptr
        obj._owner_ptr = owner_ptr
        obj._owner_put_ref = owner_put_ref
        owner_get_ref(obj._owner_ptr)
        return obj

    def __del__(self):
        owner_ptr = self._owner_ptr
        self._owner_ptr = None
        self._owner_put_ref(owner_ptr)


class _SharedObject(_BaseObject):
    """A Python object that owns a reference to a Babeltrace object."""

    @staticmethod
    def _get_ref(ptr):
        """Get a new reference on ptr.

        This must be implemented by subclasses to work correctly with a pointer
        of the native type they wrap."""
        raise NotImplementedError

    @staticmethod
    def _put_ref(ptr):
        """Put a reference on ptr.

        This must be implemented by subclasses to work correctly with a pointer
        of the native type they wrap."""
        raise NotImplementedError

    @classmethod
    def _create_from_ptr(cls, ptr_owned):
        """Create a _SharedObject from an existing reference.

        This assumes that the caller owns a reference to the Babeltrace object
        and transfers this ownership to the newly created Python object"""
        obj = cls.__new__(cls)
        obj._ptr = ptr_owned
        return obj

    @classmethod
    def _create_from_ptr_and_get_ref(cls, ptr):
        """Like _create_from_ptr, but acquire a new reference rather than
        stealing the caller's reference."""
        obj = cls._create_from_ptr(ptr)
        cls._get_ref(obj._ptr)
        return obj

    def __del__(self):
        ptr = getattr(self, '_ptr', None)
        self._put_ref(ptr)
        self._ptr = None
