/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2019 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef _CTF_META_CONFIGURE_IR_TRACE_H
#define _CTF_META_CONFIGURE_IR_TRACE_H

#include <cstdint>

#include "../ctf-ir.hpp"

namespace bt2c {

class Logger;

} /* namespace bt2c */

void ctf_trace_class_configure_ir_trace(struct ctf_trace_class *tc, bt2::Trace ir_trace);

void ctf_trace_class_configure_ir_trace(const ctf::src::TraceCls& tc, bt2::Trace irTrace,
                                        std::uint64_t mipVersion, const bt2c::Logger& parentLogger);

#endif /* _CTF_META_CONFIGURE_IR_TRACE_H */
