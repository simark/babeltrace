/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2019 Philippe Proulx <pproulx@efficios.com>
 */

#include <babeltrace2/babeltrace.h>

#include "ctf-meta-configure-ir-trace.hpp"

BT_HIDDEN
int ctf_trace_class_configure_ir_trace(const ctf::src::TraceCls& tc, bt2::Trace irTrace)
{
    if (tc.uuid()) {
        irTrace.uuid(*tc.uuid());
    }

    nonstd::optional<bt2::ConstMapValue> optEnv = tc.env();

    if (optEnv) {
        bt2::ConstMapValue env = *optEnv;

        struct Oops : std::runtime_error
        {
            Oops(bt_trace_set_environment_entry_status retParam) :
                std::runtime_error("Failed to set environment entry."), ret(retParam)
            {
            }

            bt_trace_set_environment_entry_status ret;
        };

        try {
            env.forEach([irTrace](const bpstd::string_view name, bt2::ConstValue val) {
                bt_trace_set_environment_entry_status ret;

                switch (val.type()) {
                case bt2::ValueType::SIGNED_INTEGER:
                    ret = bt_trace_set_environment_entry_integer(irTrace.libObjPtr(), name.c_str(),
                                                                 val.asSignedInteger().value());
                    break;

                case bt2::ValueType::STRING:
                    ret = bt_trace_set_environment_entry_string(irTrace.libObjPtr(), name.c_str(),
                                                                val.asString().value().c_str());
                    break;

                default:
                    bt_common_abort();
                }

                if (ret != BT_TRACE_SET_ENVIRONMENT_ENTRY_STATUS_OK) {
                    throw Oops(ret);
                }
            });
        } catch (const Oops& oops) {
            return oops.ret;
        }
    }

    return 0;
}
