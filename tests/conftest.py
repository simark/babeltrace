# SPDX-FileCopyrightText: 2025 Philippe Proulx <pproulx@efficios.com>
# SPDX-License-Identifier: MIT

# pyright: strict, reportMissingTypeStubs=false, reportPrivateUsage=false

import os
import re
import sys
import logging
import pathlib
from typing import Any, List, Optional

import pytest

try:
    import bt2
    import bt_tests_utils as btu
except ImportError:
    pytest.exit(
        "Cannot import `bt2` or `bt_tests_utils`: make sure to source `tests/utils/env.sh` before you run `bt-pytest`!",
        returncode=1,
    )

_logger = logging.getLogger(__name__)


# Helper functions to access/set dynamic config attributes (satisfies type checker)
def _config_build_root_dir(config: pytest.Config) -> pathlib.Path:
    return getattr(config, "build_root_dir")


def _set_config_build_root_dir(config: pytest.Config, path: pathlib.Path) -> None:
    setattr(config, "build_root_dir", path)


def _config_build_tests_dir(config: pytest.Config) -> pathlib.Path:
    return getattr(config, "build_tests_dir")


def _src_root_dir() -> pathlib.Path:
    return pathlib.Path(__file__).resolve().parent.parent


def _src_tests_dir() -> pathlib.Path:
    return pathlib.Path(__file__).resolve().parent


def _set_config_build_root_tests_dirs(config: pytest.Config) -> None:
    bt_tests_builddir = os.environ.get("BT_TESTS_BUILDDIR")

    if bt_tests_builddir:
        # `BT_TESTS_BUILDDIR` specifies the `tests` build directory
        _set_config_build_root_dir(
            config, pathlib.Path(bt_tests_builddir).resolve().parent
        )
    else:
        # Default to the root source directory (in-tree build)
        _set_config_build_root_dir(config, _src_root_dir())

    setattr(config, "build_tests_dir", _config_build_root_dir(config) / "tests")


def _log_env_var(name: str) -> None:
    _logger.info(f"  `{name}` environment variable: `{os.environ.get(name, '')}`")


# pytest hook.
def pytest_sessionstart(session: pytest.Session) -> None:
    _logger.info("pytest_sessionstart():")
    _logger.info(f"  Root source directory: `{_src_root_dir()}`")
    _logger.info(f"  Root build directory: `{_config_build_root_dir(session.config)}`")
    _logger.info(
        f"  `tests` build directory: `{_config_build_tests_dir(session.config)}`"
    )
    _log_env_var("ASAN_OPTIONS")
    _log_env_var("BABELTRACE_PLUGIN_PATH")
    _log_env_var("BABELTRACE_PLUGIN_PROVIDER_PATH")
    _log_env_var("BT_TESTS_BUILDDIR")
    _log_env_var("BT_TESTS_CC_BIN")
    _log_env_var("BT_TESTS_PYTHON_BIN")
    _log_env_var("BT_TESTS_PYTHON_CONFIG_BIN")
    _log_env_var("BT_TESTS_SRCDIR")
    _log_env_var("DYLD_LIBRARY_PATH")
    _log_env_var("LD_LIBRARY_PATH")
    _log_env_var("LD_PRELOAD")
    _log_env_var("PATH")
    _logger.info(f"  `sys.path` value: {sys.path}")
    _logger.info(f"  `bt2` package version: {bt2.__version__}")


# pytest hook.
def pytest_configure(config: pytest.Config) -> None:
    _set_config_build_root_tests_dirs(config)
    setattr(config, "src_tests_dir", _src_tests_dir())
    setattr(config, "ctf_traces_dir", _src_tests_dir() / "common-data/ctf-traces")


def _find_plugin(name: str):
    return bt2.find_plugin(name, find_in_sys_dir=False, find_in_user_dir=False)


# pytest hook.
def pytest_collection_modifyitems(
    config: pytest.Config, items: List[pytest.Item]
) -> None:
    # Cache plugin/feature availability once (bt2.find_plugin() can
    # be expensive).
    lttng_utils_plugin_avail = _find_plugin("lttng-utils") is not None

    for item in items:
        # Handle `needs_lttng_utils_plugin` marker
        if (
            item.get_closest_marker("needs_lttng_utils_plugin")
            and not lttng_utils_plugin_avail
        ):
            item.add_marker(
                pytest.mark.skip(reason="`lttng-utils` plugin isn't available")
            )


@pytest.fixture(scope="session")
def src_root_dir() -> pathlib.Path:
    return _src_root_dir()


@pytest.fixture(scope="session")
def src_tests_dir() -> pathlib.Path:
    return _src_tests_dir()


@pytest.fixture(scope="session")
def build_root_dir(request: pytest.FixtureRequest) -> pathlib.Path:
    return _config_build_root_dir(request.config)


@pytest.fixture(scope="session")
def build_tests_dir(request: pytest.FixtureRequest) -> pathlib.Path:
    return _config_build_tests_dir(request.config)


@pytest.fixture(scope="session")
def common_data_dir() -> pathlib.Path:
    return _src_tests_dir() / "common-data"


@pytest.fixture(scope="session")
def ctf_traces_dir() -> pathlib.Path:
    return _src_tests_dir() / "common-data/ctf-traces"


@pytest.fixture(scope="session")
def muxer_comp_cls() -> bt2._FilterComponentClassConst:
    return _find_plugin(
        "utils"
    ).filter_component_classes[  # pyright: ignore[reportOptionalMemberAccess]
        "muxer"
    ]


@pytest.fixture(scope="session")
def sink_ctf_comp_cls() -> bt2._SinkComponentClassConst:
    return _find_plugin(
        "ctf"
    ).sink_component_classes[  # pyright: ignore[reportOptionalMemberAccess]
        "fs"
    ]


@pytest.fixture(scope="session")
def src_ctf_comp_cls() -> bt2._SourceComponentClassConst:
    return _find_plugin(
        "ctf"
    ).source_component_classes[  # pyright: ignore[reportOptionalMemberAccess]
        "fs"
    ]


@pytest.fixture(scope="session")
def trimmer_comp_cls() -> bt2._FilterComponentClassConst:
    return _find_plugin(
        "utils"
    ).filter_component_classes[  # pyright: ignore[reportOptionalMemberAccess]
        "trimmer"
    ]


@pytest.fixture(scope="session")
def details_comp_cls() -> bt2._SinkComponentClassConst:
    return _find_plugin(
        "text"
    ).sink_component_classes[  # pyright: ignore[reportOptionalMemberAccess]
        "details"
    ]


@pytest.fixture(scope="session")
def pretty_comp_cls() -> bt2._SinkComponentClassConst:
    return _find_plugin(
        "text"
    ).sink_component_classes[  # pyright: ignore[reportOptionalMemberAccess]
        "pretty"
    ]


@pytest.fixture(scope="session")
def dmesg_comp_cls() -> bt2._SourceComponentClassConst:
    return _find_plugin(
        "text"
    ).source_component_classes[  # pyright: ignore[reportOptionalMemberAccess]
        "dmesg"
    ]


@pytest.fixture(scope="session")
def dummy_comp_cls() -> bt2._SinkComponentClassConst:
    return _find_plugin(
        "utils"
    ).sink_component_classes[  # pyright: ignore[reportOptionalMemberAccess]
        "dummy"
    ]


@pytest.fixture(scope="session")
def os_type() -> btu.OsType:
    return btu.os_type()


class _Catch2TestItem(pytest.Item):
    def __init__(
        self,
        *,
        test_binary: pathlib.Path,
        catch2_test_name: str,
        build_root_dir: pathlib.Path,
        **kwargs: Any,
    ):
        super().__init__(**kwargs)  # pyright: ignore[reportUnknownMemberType]
        self._test_binary = test_binary
        self._catch2_test_name = catch2_test_name
        self._build_root_dir = build_root_dir

    # Catch2 test name with special test spec characters escaped.
    @property
    def _escaped_catch2_test_name(self) -> str:
        return re.sub(r"([\\,\[*~])", r"\\\1", self._catch2_test_name)

    def runtest(self) -> None:
        _logger.info(
            f"Running Catch2 test case `{self._catch2_test_name}` in `{self._test_binary}`"
        )
        result = btu.run(
            self._build_root_dir,
            self._test_binary,
            [self._escaped_catch2_test_name, "-s", "--colour-mode", "none"],
            text=False,
        )

        sys.stdout.buffer.write(result.stdout)

        if result.returncode != 0:
            pytest.fail(f"Catch2 test failed: `{self._catch2_test_name}`")

    def reportinfo(self):
        return self.path, None, self.name


class _Catch2TestFile(pytest.File):
    @staticmethod
    def _normalize_catch2_test_name(test_name: str) -> str:
        test_name = test_name.lower()
        test_name = re.sub(r"[ :-]", "_", test_name)
        test_name = re.sub(r"_{2,}", "_", test_name)
        test_name = re.sub(r"\W", "", test_name)
        return f"test_{test_name}"

    def collect(self) -> List[_Catch2TestItem]:
        # Build the test binary path (remove `.cpp` extension)
        test_binary = btu.exe_path(
            _config_build_tests_dir(self.config)
            / self.path.resolve().relative_to(_src_tests_dir()).with_suffix("")
        )

        # Skip if the binary doesn't exist
        if not test_binary.exists():
            pytest.skip(f"`{test_binary}` Catch2 binary doesn't exist")
        else:
            assert os.access(test_binary, os.X_OK)

        # List test names from Catch2.
        _logger.info(f"Listing Catch2 test case names from `{test_binary}`")
        build_root_dir = _config_build_root_dir(self.config)
        result = btu.run(
            build_root_dir,
            test_binary,
            ["--list-tests", "-v", "quiet", "--order", "decl"],
            check=True,
        )
        _logger.debug(result.stdout)

        # Parse test names and create corresponding test items
        items: List[_Catch2TestItem] = []

        for line in result.stdout.strip().split("\n"):
            test_name = line.strip()

            if test_name:
                # Normalize the test name for the pytest node ID
                _logger.info(
                    f"Adding Catch2 test case `{test_name}` from `{test_binary}`"
                )
                items.append(
                    _Catch2TestItem.from_parent(  # pyright: ignore[reportUnknownMemberType]
                        name=self._normalize_catch2_test_name(test_name),
                        parent=self,
                        test_binary=test_binary,
                        catch2_test_name=test_name,
                        build_root_dir=build_root_dir,
                    )
                )

        # Return test items
        return items


# pytest hook.
def pytest_collect_file(
    file_path: pathlib.Path, parent: pytest.Collector
) -> Optional[_Catch2TestFile]:
    if file_path.suffix == ".cpp" and file_path.name.startswith("test-"):
        return _Catch2TestFile.from_parent(  # pyright: ignore[reportUnknownMemberType]
            parent=parent, path=file_path
        )
