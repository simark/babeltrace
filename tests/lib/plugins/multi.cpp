/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2026 EfficiOS, Inc.
 */

#include <cstdlib>
#include <string>

#include <glib.h>

#include <babeltrace2/babeltrace.h>

namespace {

void incrementCounter(const char * const name)
{
    const auto val = g_getenv(name);

    g_setenv(name, std::to_string(val ? std::atoi(val) + 1 : 1).c_str(), 1);
}

bt_plugin_initialize_func_status pluginOneInit(bt_self_plugin *)
{
    incrementCounter("BT_TEST_PLUGIN_MULTI_ONE_INITIALIZE_CALLED");
    return BT_PLUGIN_INITIALIZE_FUNC_STATUS_OK;
}

void pluginOneFinalize()
{
    incrementCounter("BT_TEST_PLUGIN_MULTI_ONE_FINALIZE_CALLED");
}

bt_plugin_initialize_func_status pluginTwoInit(bt_self_plugin *)
{
    incrementCounter("BT_TEST_PLUGIN_MULTI_TWO_INITIALIZE_CALLED");
    return BT_PLUGIN_INITIALIZE_FUNC_STATUS_OK;
}

void pluginTwoFinalize()
{
    incrementCounter("BT_TEST_PLUGIN_MULTI_TWO_FINALIZE_CALLED");
}

} /* namespace */

BT_PLUGIN_MODULE();

BT_PLUGIN_WITH_ID(plugin_one, "test_multi_one");
BT_PLUGIN_DESCRIPTION_WITH_ID(plugin_one, "First plugin in a multi-plugin shared object");
BT_PLUGIN_AUTHOR_WITH_ID(plugin_one, "EfficiOS");
BT_PLUGIN_LICENSE_WITH_ID(plugin_one, "MIT");
BT_PLUGIN_INITIALIZE_FUNC_WITH_ID(plugin_one, pluginOneInit);
BT_PLUGIN_FINALIZE_FUNC_WITH_ID(plugin_one, pluginOneFinalize);

BT_PLUGIN_WITH_ID(plugin_two, "test_multi_two");
BT_PLUGIN_DESCRIPTION_WITH_ID(plugin_two, "Second plugin in a multi-plugin shared object");
BT_PLUGIN_AUTHOR_WITH_ID(plugin_two, "EfficiOS");
BT_PLUGIN_LICENSE_WITH_ID(plugin_two, "MIT");
BT_PLUGIN_INITIALIZE_FUNC_WITH_ID(plugin_two, pluginTwoInit);
BT_PLUGIN_FINALIZE_FUNC_WITH_ID(plugin_two, pluginTwoFinalize);
