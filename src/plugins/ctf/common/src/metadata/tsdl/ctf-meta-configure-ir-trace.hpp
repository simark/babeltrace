/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2019 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef _CTF_META_CONFIGURE_IR_TRACE_H
#define _CTF_META_CONFIGURE_IR_TRACE_H

#include "common/macros.h"

#include "ctf-meta.hpp"
#include "plugins/ctf/common/src/metadata/ctf-ir.hpp"

BT_HIDDEN
int ctf_trace_class_configure_ir_trace(const ctf::src::TraceCls& tc, bt2::Trace irTrace);

#endif /* _CTF_META_CONFIGURE_IR_TRACE_H */
