/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2022 Francis Deslauriers <francis.deslauriers@efficios.com>
 */

#ifndef BABELTRACE_PLUGIN_CTF_COMMON_LOG_CFG_HPP
#define BABELTRACE_PLUGIN_CTF_COMMON_LOG_CFG_HPP

#include <babeltrace2/babeltrace.h>

namespace ctf {

struct LogCfg final
{
    explicit LogCfg(const bt_logging_level logLevelParam, bt_self_component * const selfCompParam) :
        logLevel {logLevelParam}, selfComp {selfCompParam}
    {
    }

    explicit LogCfg(const bt_logging_level logLevelParam,
                    bt_self_component_class * const selfCompClassParam) :
        logLevel {logLevelParam},
        selfCompClass {selfCompClassParam}
    {
    }

    bt_logging_level logLevel;
    bt_self_component *selfComp = nullptr;
    bt_self_component_class *selfCompClass = nullptr;
};

} /* namespace ctf */

#endif /* BABELTRACE_PLUGIN_CTF_COMMON_LOG_CFG_HPP */
