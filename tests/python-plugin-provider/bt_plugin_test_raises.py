# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2026 EfficiOS Inc.
#

# This plugin file raises during module execution to exercise the cleanup of
# `sys.modules` on `exec_module` failure.

raise RuntimeError("boom")
