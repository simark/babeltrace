# SPDX-FileCopyrightText: 2019 EfficiOS Inc.
# SPDX-License-Identifier: GPL-2.0-only

import sys

import bt2
import pytest


@pytest.mark.parametrize("fail_on_load_error", [False, True])
class TestPythonPluginLoading:
    def test_python_plugin_provider(self, src_tests_dir, fail_on_load_error):
        plugin_path = (
            src_tests_dir
            / "python-plugin-provider/bt_plugin_test_python_plugin_provider.py"
        )
        pset = bt2.find_plugins_in_path(
            str(plugin_path), fail_on_load_error=fail_on_load_error
        )

        assert pset
        assert len(pset) == 1
        plugin = pset[0]
        assert plugin.name == "sparkling"
        assert plugin.author == "Philippe Proulx"
        assert plugin.description == "A delicious plugin."
        assert plugin.version
        assert plugin.version.major == 1
        assert plugin.version.minor == 2
        assert plugin.version.patch == 3
        assert plugin.version.extra == "EXTRA"
        assert plugin.license == "MIT"
        assert len(plugin.source_component_classes) == 1
        assert len(plugin.filter_component_classes) == 1
        assert len(plugin.sink_component_classes) == 1
        assert plugin.source_component_classes["MySource"].name == "MySource"
        assert plugin.filter_component_classes["MyFilter"].name == "MyFilter"
        assert plugin.sink_component_classes["MySink"].name == "MySink"

    # Test a Python plugin that calls bt2.register_plugin() but has no component
    # classes.
    def test_python_plugin_provider_empty(self, src_tests_dir, fail_on_load_error):
        plugin_path = src_tests_dir / "python-plugin-provider/bt_plugin_test_empty.py"
        pset = bt2.find_plugins_in_path(
            str(plugin_path), fail_on_load_error=fail_on_load_error
        )

        assert pset
        assert len(pset) == 1
        plugin = pset[0]
        assert plugin.name == "empty"
        assert len(plugin.source_component_classes) == 0
        assert len(plugin.filter_component_classes) == 0
        assert len(plugin.sink_component_classes) == 0

    # Test a Python file named `bt_plugin_*.py` but without a
    # bt2.register_plugin() call.
    def test_python_plugin_provider_no_register(
        self, src_tests_dir, fail_on_load_error
    ):
        plugin_path = (
            src_tests_dir / "python-plugin-provider/bt_plugin_test_no_register.py"
        )

        if fail_on_load_error:
            with pytest.raises(bt2._Error, match="Cannot load Python plugin"):
                bt2.find_plugins_in_path(
                    str(plugin_path), fail_on_load_error=fail_on_load_error
                )
        else:
            assert (
                bt2.find_plugins_in_path(
                    str(plugin_path), fail_on_load_error=fail_on_load_error
                )
                is None
            )

    # Test a Python plugin file that raises during module execution.  The
    # module must not be left registered in `sys.modules`.
    def test_python_plugin_provider_raises(self, src_tests_dir, fail_on_load_error):
        plugin_path = src_tests_dir / "python-plugin-provider/bt_plugin_test_raises.py"

        before = {k for k in sys.modules if k.startswith("bt_plugin_")}

        if fail_on_load_error:
            with pytest.raises(bt2._Error, match="Cannot load Python plugin"):
                bt2.find_plugins_in_path(
                    str(plugin_path), fail_on_load_error=fail_on_load_error
                )
        else:
            assert (
                bt2.find_plugins_in_path(
                    str(plugin_path), fail_on_load_error=fail_on_load_error
                )
                is None
            )

        after = {k for k in sys.modules if k.startswith("bt_plugin_")}
        assert before == after
