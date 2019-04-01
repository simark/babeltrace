/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Philippe Proulx <pproulx@efficios.com>
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

#ifndef SWIGPYTHON
# error Unsupported output language
#endif

%module native_bt

%{
#define BT_LOG_TAG "PY-NATIVE"
#include "logging.h"

#include <babeltrace/babeltrace.h>
#include <babeltrace/property.h>
#include <babeltrace/assert-internal.h>

typedef const unsigned char *BTUUID;
typedef const uint8_t *bt_uuid;
%}

typedef int bt_bool;

/* For uint*_t/int*_t */
%include "stdint.i"

/* Remove `bt_` and `BT_` prefixes from function names and enumeration items */
%rename("%(strip:[bt_])s", %$isfunction) "";
%rename("%(strip:[BT_])s", %$isenumitem) "";

/* Output argument typemap for string output (always appends) */
%typemap(in, numinputs=0) const char **BTOUTSTR (char *temp_value = NULL) {
	$1 = &temp_value;
}

%typemap(argout) const char **BTOUTSTR {
	if (*$1) {
		/* SWIG_Python_AppendOutput() steals the created object */
		$result = SWIG_Python_AppendOutput($result, SWIG_Python_str_FromChar(*$1));
	} else {
		/* SWIG_Python_AppendOutput() steals Py_None */
		Py_INCREF(Py_None);
		$result = SWIG_Python_AppendOutput($result, Py_None);
	}
}

/* Output argument typemap for field type output (always appends) */
%typemap(in, numinputs=0) const bt_field_class **BTOUTFC (bt_field_class *temp_ft = NULL) {
	$1 = &temp_ft;
}

%typemap(argout) const bt_field_class **BTOUTFC {
	if (*$1) {
		/* SWIG_Python_AppendOutput() steals the created object */
		$result = SWIG_Python_AppendOutput($result, SWIG_NewPointerObj(SWIG_as_voidptr(*$1), SWIGTYPE_p_bt_field_class, 0));
	} else {
		/* SWIG_Python_AppendOutput() steals Py_None */
		Py_INCREF(Py_None);
		$result = SWIG_Python_AppendOutput($result, Py_None);
	}
}

/* Output argument typemap for component output (always appends) */
%typemap(in, numinputs=0) const bt_component_source **BTOUTCOMP (bt_component_source *temp_comp = NULL) {
	$1 = &temp_comp;
}

%typemap(in, numinputs=0) const bt_component_filter **BTOUTCOMP (bt_component_filter *temp_comp = NULL) {
	$1 = &temp_comp;
}

%typemap(in, numinputs=0) const bt_component_sink **BTOUTCOMP (bt_component_sink *temp_comp = NULL) {
	$1 = &temp_comp;
}

%typemap(argout) const bt_component_source **BTOUTCOMP {
	if (*$1) {
		/* SWIG_Python_AppendOutput() steals the created object */
		$result = SWIG_Python_AppendOutput($result, SWIG_NewPointerObj(SWIG_as_voidptr(*$1), SWIGTYPE_p_bt_component_source, 0));
	} else {
		/* SWIG_Python_AppendOutput() steals Py_None */
		Py_INCREF(Py_None);
		$result = SWIG_Python_AppendOutput($result, Py_None);
	}
}

%typemap(argout) const bt_component_filter **BTOUTCOMP {
	if (*$1) {
		/* SWIG_Python_AppendOutput() steals the created object */
		$result = SWIG_Python_AppendOutput($result, SWIG_NewPointerObj(SWIG_as_voidptr(*$1), SWIGTYPE_p_bt_component_filter, 0));
	} else {
		/* SWIG_Python_AppendOutput() steals Py_None */
		Py_INCREF(Py_None);
		$result = SWIG_Python_AppendOutput($result, Py_None);
	}
}

%typemap(argout) const bt_component_sink **BTOUTCOMP {
	if (*$1) {
		/* SWIG_Python_AppendOutput() steals the created object */
		$result = SWIG_Python_AppendOutput($result, SWIG_NewPointerObj(SWIG_as_voidptr(*$1), SWIGTYPE_p_bt_component_sink, 0));
	} else {
		/* SWIG_Python_AppendOutput() steals Py_None */
		Py_INCREF(Py_None);
		$result = SWIG_Python_AppendOutput($result, Py_None);
	}
}

/* Output argument typemap for connection output (always appends) */
%typemap(in, numinputs=0) const bt_connection **BTOUTCONN (bt_connection *temp_conn = NULL) {
	$1 = &temp_conn;
}

%typemap(argout) const bt_connection **BTOUTCONN {
	if (*$1) {
		/* SWIG_Python_AppendOutput() steals the created object */
		$result = SWIG_Python_AppendOutput($result, SWIG_NewPointerObj(SWIG_as_voidptr(*$1), SWIGTYPE_p_bt_connection, 0));
	} else {
		/* SWIG_Python_AppendOutput() steals Py_None */
		Py_INCREF(Py_None);
		$result = SWIG_Python_AppendOutput($result, Py_None);
	}
}

/* Output argument typemap for private port output (always appends) */
%typemap(in, numinputs=0) bt_self_component_port_output **BTOUTSELFPORTOUTPUT (bt_self_component_port_output *temp_self_port = NULL) {
	$1 = &temp_self_port;
}

%typemap(argout) bt_self_component_port_output **BTOUTSELFPORTOUTPUT {
	if (*$1) {
		/* SWIG_Python_AppendOutput() steals the created object */
		$result = SWIG_Python_AppendOutput($result, SWIG_NewPointerObj(SWIG_as_voidptr(*$1), SWIGTYPE_p_bt_self_component_port_output, 0));
	} else {
		/* SWIG_Python_AppendOutput() steals Py_None */
		Py_INCREF(Py_None);
		$result = SWIG_Python_AppendOutput($result, Py_None);
	}
}

/* Output argument typemap for self port output (always appends) */
%typemap(in, numinputs=0) bt_self_component_port_input **BTOUTSELFPORTINPUT (bt_self_component_port_input *temp_self_port = NULL) {
	$1 = &temp_self_port;
}

%typemap(argout) bt_self_component_port_input **BTOUTSELFPORTINPUT {
	if (*$1) {
		/* SWIG_Python_AppendOutput() steals the created object */
		$result = SWIG_Python_AppendOutput($result, SWIG_NewPointerObj(SWIG_as_voidptr(*$1), SWIGTYPE_p_bt_self_component_port_input, 0));
	} else {
		/* SWIG_Python_AppendOutput() steals Py_None */
		Py_INCREF(Py_None);
		$result = SWIG_Python_AppendOutput($result, Py_None);
	}
}

/* Output argument typemap for value output (always appends) */
%typemap(in, numinputs=0) const bt_field_class_unsigned_enumeration_mapping_ranges **BTOUTENUMMAPPINGRANGE (bt_field_class_unsigned_enumeration_mapping_ranges *temp_value = NULL) {
	$1 = &temp_value;
}

%typemap(argout) const bt_field_class_unsigned_enumeration_mapping_ranges **BTOUTENUMMAPPINGRANGE {
	if (*$1) {
		/* SWIG_Python_AppendOutput() steals the created object */
		$result = SWIG_Python_AppendOutput($result,
				SWIG_NewPointerObj(SWIG_as_voidptr(*$1),
					SWIGTYPE_p_bt_field_class_unsigned_enumeration_mapping_ranges, 0));
	} else {
		/* SWIG_Python_AppendOutput() steals Py_None */
		Py_INCREF(Py_None);
		$result = SWIG_Python_AppendOutput($result, Py_None);
	}
}

/* Output argument typemap for value output (always appends) */
%typemap(in, numinputs=0) const bt_field_class_signed_enumeration_mapping_ranges **BTOUTENUMMAPPINGRANGE (bt_field_class_signed_enumeration_mapping_ranges *temp_value = NULL) {
	$1 = &temp_value;
}

%typemap(argout) const bt_field_class_signed_enumeration_mapping_ranges **BTOUTENUMMAPPINGRANGE {
	if (*$1) {
		/* SWIG_Python_AppendOutput() steals the created object */
		$result = SWIG_Python_AppendOutput($result,
				SWIG_NewPointerObj(SWIG_as_voidptr(*$1),
					SWIGTYPE_p_bt_field_class_signed_enumeration_mapping_ranges, 0));
	} else {
		/* SWIG_Python_AppendOutput() steals Py_None */
		Py_INCREF(Py_None);
		$result = SWIG_Python_AppendOutput($result, Py_None);
	}
}

/* Output argument typemap for value output (always appends) */
%typemap(in, numinputs=0) bt_value **BTOUTVALUE (struct bt_value *temp_value = NULL) {
	$1 = &temp_value;
}

%typemap(argout) bt_value **BTOUTVALUE {
	if (*$1) {
		/* SWIG_Python_AppendOutput() steals the created object */
		$result = SWIG_Python_AppendOutput($result, SWIG_NewPointerObj(SWIG_as_voidptr(*$1), SWIGTYPE_p_bt_value, 0));
	} else {
		/* SWIG_Python_AppendOutput() steals Py_None */
		Py_INCREF(Py_None);
		$result = SWIG_Python_AppendOutput($result, Py_None);
	}
}

/* Output argument typemap for clock value output (always appends) */
%typemap(in, numinputs=0) const bt_clock_snapshot **BTOUTCLOCKSNAPSHOT (bt_clock_snapshot *temp_clock_value = NULL) {
	$1 = &temp_clock_value;
}

%typemap(argout) const bt_clock_snapshot **BTOUTCLOCKSNAPSHOT {
	if (*$1) {
		/* SWIG_Python_AppendOutput() steals the created object */
		$result = SWIG_Python_AppendOutput($result, SWIG_NewPointerObj(SWIG_as_voidptr(*$1), SWIGTYPE_p_bt_clock_snapshot, 0));
	} else {
		/* SWIG_Python_AppendOutput() steals Py_None */
		Py_INCREF(Py_None);
		$result = SWIG_Python_AppendOutput($result, Py_None);
	}
}

/* Output argument typemap for initialized uint64_t output parameter (always appends) */
%typemap(in, numinputs=0) bt_event_class_log_level *OUTPUTINIT (bt_event_class_log_level temp = -1) {
	$1 = &temp;
}

%typemap(argout) bt_event_class_log_level *OUTPUTINIT {
	/* SWIG_Python_AppendOutput() steals the created object */
	$result = SWIG_Python_AppendOutput($result, SWIG_From_int(*$1));
}

/* Output argument typemap for initialized uint64_t output parameter (always appends) */
%typemap(in, numinputs=0) uint64_t *OUTPUTINIT (uint64_t temp = -1ULL) {
	$1 = &temp;
}

%typemap(argout) uint64_t *OUTPUTINIT {
	$result = SWIG_Python_AppendOutput(resultobj, SWIG_From_unsigned_SS_long_SS_long((*$1)));
}

/* Output argument typemap for initialized int64_t output parameter (always appends) */
%typemap(in, numinputs=0) int64_t *OUTPUTINIT (int64_t temp = -1ULL) {
	$1 = &temp;
}

%typemap(argout) int64_t *OUTPUTINIT {
	$result = SWIG_Python_AppendOutput(resultobj, SWIG_From_long_SS_long((*$1)));
}

/* Output argument typemap for initialized unsigned int output parameter (always appends) */
%typemap(in, numinputs=0) unsigned int *OUTPUTINIT (unsigned int temp = -1) {
	$1 = &temp;
}

%typemap(argout) unsigned int *OUTPUTINIT {
	$result = SWIG_Python_AppendOutput(resultobj, SWIG_From_unsigned_SS_long_SS_long((uint64_t) (*$1)));
}
/* Output argument typemap for initialized double output parameter (always appends) */
%typemap(in, numinputs=0) double *OUTPUTINIT (double temp = -1) {
	$1 = &temp;
}

%typemap(argout) double *OUTPUTINIT {
	$result = SWIG_Python_AppendOutput(resultobj, SWIG_From_int((*$1)));
}


/* Input argument typemap for UUID bytes */
%typemap(in) bt_uuid {
	$1 = (unsigned char *) PyBytes_AsString($input);
}

/* Output argument typemap for UUID bytes */
%typemap(out) bt_uuid {
	if (!$1) {
		Py_INCREF(Py_None);
		$result = Py_None;
	} else {
		$result = PyBytes_FromStringAndSize((const char *) $1, 16);
	}
}

/* Input argument typemap for bt_bool */
%typemap(in) bt_bool {
	$1 = PyObject_IsTrue($input);
}

/* Output argument typemap for bt_bool */
%typemap(out) bt_bool {
	if ($1 > 0) {
		$result = Py_True;
	} else {
		$result = Py_False;
	}
	Py_INCREF($result);
	return $result;
}

/*
 * Input and output argument typemaps for raw Python objects (direct).
 *
 * Those typemaps honor the convention of Python C function calls with
 * respect to reference counting: parameters are passed as borrowed
 * references, and objects are returned as new references. The wrapped
 * C function must ensure that the return value is always a new
 * reference, and never steal parameter references.
 */
%typemap(in) PyObject * {
	$1 = $input;
}

%typemap(out) PyObject * {
	$result = $1;
}

typedef enum bt_property_availability {
	BT_PROPERTY_AVAILABILITY_AVAILABLE,
	BT_PROPERTY_AVAILABILITY_NOT_AVAILABLE,
} bt_property_availability;

/* Per-module interface files */
%include "native_bt_clock_class.i"
%include "native_bt_clock_snapshot.i"
%include "native_bt_component.i"
%include "native_bt_component_class.i"
%include "native_bt_connection.i"
%include "native_bt_event.i"
%include "native_bt_event_class.i"
%include "native_bt_field.i"
%include "native_bt_field_class.i"
%include "native_bt_fieldpath.i"
%include "native_bt_graph.i"
%include "native_bt_logging.i"
%include "native_bt_message.i"
%include "native_bt_notifier.i"
%include "native_bt_packet.i"
%include "native_bt_plugin.i"
%include "native_bt_port.i"
%include "native_bt_queryexec.i"
%include "native_bt_ref.i"
%include "native_bt_stream.i"
%include "native_bt_stream_class.i"
%include "native_bt_trace.i"
%include "native_bt_trace_class.i"
%include "native_bt_values.i"
%include "native_bt_version.i"

%include "ctfwriter/native_btctf.i"
%include "ctfwriter/native_btctf_event.i"
%include "ctfwriter/native_btctf_eventclass.i"
%include "ctfwriter/native_btctf_stream.i"
%include "ctfwriter/native_btctf_streamclass.i"
%include "ctfwriter/native_btctf_clock.i"
%include "ctfwriter/native_btctf_writer.i"
%include "ctfwriter/native_btctf_fields.i"
%include "ctfwriter/native_btctf_fieldtypes.i"
%include "ctfwriter/native_btctf_trace.i"
