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

__all__ = ['EventClassLogLevel']


from bt2 import utils, native_bt
import bt2


class EventClassLogLevel:
    EMERGENCY = native_bt.EVENT_CLASS_LOG_LEVEL_EMERGENCY
    ALERT = native_bt.EVENT_CLASS_LOG_LEVEL_ALERT
    CRITICAL = native_bt.EVENT_CLASS_LOG_LEVEL_CRITICAL
    ERROR = native_bt.EVENT_CLASS_LOG_LEVEL_ERROR
    WARNING = native_bt.EVENT_CLASS_LOG_LEVEL_WARNING
    NOTICE = native_bt.EVENT_CLASS_LOG_LEVEL_NOTICE
    INFO = native_bt.EVENT_CLASS_LOG_LEVEL_INFO
    DEBUG_SYSTEM = native_bt.EVENT_CLASS_LOG_LEVEL_DEBUG_SYSTEM
    DEBUG_PROGRAM = native_bt.EVENT_CLASS_LOG_LEVEL_DEBUG_PROGRAM
    DEBUG_PROCESS = native_bt.EVENT_CLASS_LOG_LEVEL_DEBUG_PROCESS
    DEBUG_MODULE = native_bt.EVENT_CLASS_LOG_LEVEL_DEBUG_MODULE
    DEBUG_UNIT = native_bt.EVENT_CLASS_LOG_LEVEL_DEBUG_UNIT
    DEBUG_FUNCTION = native_bt.EVENT_CLASS_LOG_LEVEL_DEBUG_FUNCTION
    DEBUG_LINE = native_bt.EVENT_CLASS_LOG_LEVEL_DEBUG_LINE
    DEBUG = native_bt.EVENT_CLASS_LOG_LEVEL_DEBUG


class _EventClass(bt2.object._SharedObject):
    _GET_REF_NATIVE_FUNC = native_bt.event_class_get_ref
    _PUT_REF_NATIVE_FUNC = native_bt.event_class_put_ref

    @property
    def stream_class(self):
        sc_ptr = native_bt.event_class_borrow_stream_class(self._ptr)

        # TODO: Can this be None?  If not we could assert is not None

        if sc_ptr is not None:
            return bt2.stream_class._StreamClass._create_from_ptr_and_get_ref(sc_ptr)

    @property
    def name(self):
        return native_bt.event_class_get_name(self._ptr)

    def _name(self, name):
        utils._check_str(name)
        return native_bt.event_class_set_name(self._ptr, name)

    _name = property(fset=_name)

    @property
    def id(self):
        id = native_bt.event_class_get_id(self._ptr)
        return id if id >= 0 else None

    @property
    def log_level(self):
        is_available, log_level = native_bt.event_class_get_log_level(self._ptr)
        return _EVENT_CLASS_LOG_LEVEL_TO_OBJ[log_level] if is_available == native_bt.PROPERTY_AVAILABILITY_AVAILABLE else None

    def _log_level(self, log_level):
        log_levels = (
            EventClassLogLevel.EMERGENCY,
            EventClassLogLevel.ALERT,
            EventClassLogLevel.CRITICAL,
            EventClassLogLevel.ERROR,
            EventClassLogLevel.WARNING,
            EventClassLogLevel.NOTICE,
            EventClassLogLevel.INFO,
            EventClassLogLevel.DEBUG_SYSTEM,
            EventClassLogLevel.DEBUG_PROGRAM,
            EventClassLogLevel.DEBUG_PROCESS,
            EventClassLogLevel.DEBUG_MODULE,
            EventClassLogLevel.DEBUG_UNIT,
            EventClassLogLevel.DEBUG_FUNCTION,
            EventClassLogLevel.DEBUG_LINE,
            EventClassLogLevel.DEBUG,
        )

        if log_level not in log_levels:
            raise ValueError("'{}' is not a valid log level".format(log_level))

        native_bt.event_class_set_log_level(self._ptr, log_level)

    _log_level = property(fset=_log_level)

    @property
    def emf_uri(self):
        return native_bt.event_class_get_emf_uri(self._ptr)

    def _emf_uri(self, emf_uri):
        utils._check_str(emf_uri)
        ret = native_bt.event_class_set_emf_uri(self._ptr, emf_uri)
        utils._handle_ret(ret, "cannot set event class object's EMF URI")

    _emf_uri = property(fset=_emf_uri)

    @property
    def specific_context_field_class(self):
        ft_ptr = native_bt.event_class_borrow_specific_context_field_class_const(self._ptr)

        if ft_ptr is None:
            return

        return bt2.field_class._create_field_class_from_ptr_and_get_ref(ft_ptr)

    def _specific_context_field_class(self, context_field_class):
        if context_field_class is not None:
            utils._check_type(context_field_class, bt2.field_class._FieldClass)
            ret = native_bt.event_class_set_specific_context_field_class(self._ptr, context_field_class._ptr)
            utils._handle_ret(ret, "cannot set event class object's context field type")

    _specific_context_field_class = property(fset=_specific_context_field_class)

    @property
    def payload_field_class(self):
        ft_ptr = native_bt.event_class_borrow_payload_field_class_const(self._ptr)

        if ft_ptr is None:
            return

        return bt2.field_class._create_field_class_from_ptr_and_get_ref(ft_ptr)

    def _payload_field_class(self, payload_field_class):

        if payload_field_class is not None:
            utils._check_type(payload_field_class, bt2.field_class._FieldClass)
            ret = native_bt.event_class_set_payload_field_class(self._ptr, payload_field_class._ptr)
            utils._handle_ret(ret, "cannot set event class object's payload field class")
        # TODO: Else what, raise, clear the field?

    _payload_field_class = property(fset=_payload_field_class)

_EVENT_CLASS_LOG_LEVEL_TO_OBJ = {
        native_bt.EVENT_CLASS_LOG_LEVEL_EMERGENCY: EventClassLogLevel.EMERGENCY,
        native_bt.EVENT_CLASS_LOG_LEVEL_ALERT: EventClassLogLevel.ALERT,
        native_bt.EVENT_CLASS_LOG_LEVEL_CRITICAL: EventClassLogLevel.CRITICAL,
        native_bt.EVENT_CLASS_LOG_LEVEL_ERROR: EventClassLogLevel.ERROR,
        native_bt.EVENT_CLASS_LOG_LEVEL_WARNING: EventClassLogLevel.WARNING,
        native_bt.EVENT_CLASS_LOG_LEVEL_NOTICE: EventClassLogLevel.NOTICE,
        native_bt.EVENT_CLASS_LOG_LEVEL_INFO: EventClassLogLevel.INFO,
        native_bt.EVENT_CLASS_LOG_LEVEL_DEBUG_SYSTEM: EventClassLogLevel.DEBUG_SYSTEM,
        native_bt.EVENT_CLASS_LOG_LEVEL_DEBUG_PROGRAM: EventClassLogLevel.DEBUG_PROGRAM,
        native_bt.EVENT_CLASS_LOG_LEVEL_DEBUG_PROCESS: EventClassLogLevel.DEBUG_PROCESS,
        native_bt.EVENT_CLASS_LOG_LEVEL_DEBUG_MODULE: EventClassLogLevel.DEBUG_MODULE,
        native_bt.EVENT_CLASS_LOG_LEVEL_DEBUG_UNIT: EventClassLogLevel.DEBUG_UNIT,
        native_bt.EVENT_CLASS_LOG_LEVEL_DEBUG_FUNCTION: EventClassLogLevel.DEBUG_FUNCTION,
        native_bt.EVENT_CLASS_LOG_LEVEL_DEBUG_LINE: EventClassLogLevel.DEBUG_LINE,
        native_bt.EVENT_CLASS_LOG_LEVEL_DEBUG: EventClassLogLevel.DEBUG,
}

