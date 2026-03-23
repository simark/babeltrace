# SPDX-FileCopyrightText: 2024 EfficiOS Inc.
# SPDX-License-Identifier: MIT

all-local: doxygen-doc

@DX_RULES@

$(DX_DOCDIR)/$(PACKAGE).tag: \
	Doxyfile \
	style.css \
	all.dox \
	cpp.dox \
	@top_srcdir@/src/compat/bitfield.h \
	@top_srcdir@/src/compat/compiler.h \
	@top_srcdir@/src/compat/endian.h \
	@top_srcdir@/src/compat/fcntl.h \
	@top_srcdir@/src/compat/limits.h \
	@top_srcdir@/src/compat/memstream.h \
	@top_srcdir@/src/compat/mman.h \
	@top_srcdir@/src/compat/socket.hpp \
	@top_srcdir@/src/compat/stdio.h \
	@top_srcdir@/src/compat/time.h \
	@top_srcdir@/src/compat/unistd.h \
	@top_srcdir@/src/compat/utc.h \
	@top_srcdir@/src/common/align.h \
	@top_srcdir@/src/common/assert.h \
	@top_srcdir@/src/common/common.h \
	@top_srcdir@/src/common/list.h \
	@top_srcdir@/src/common/macros.h \
	@top_srcdir@/src/common/mmap-align.h \
	@top_srcdir@/src/common/safe.h \
	@top_srcdir@/src/common/uuid.h \
	@top_srcdir@/src/cpp-common/bt2c/aliases.hpp \
	@top_srcdir@/src/cpp-common/bt2c/align.hpp \
	@top_srcdir@/src/cpp-common/bt2c/bt2-value-from-json-val.hpp \
	@top_srcdir@/src/cpp-common/bt2c/c-string-view.hpp \
	@top_srcdir@/src/cpp-common/bt2c/contains.hpp \
	@top_srcdir@/src/cpp-common/bt2c/data-len.hpp \
	@top_srcdir@/src/cpp-common/bt2c/endian.hpp \
	@top_srcdir@/src/cpp-common/bt2c/exc.hpp \
	@top_srcdir@/src/cpp-common/bt2c/file-utils.hpp \
	@top_srcdir@/src/cpp-common/bt2c/fmt.hpp \
	@top_srcdir@/src/cpp-common/bt2c/glib-up.hpp \
	@top_srcdir@/src/cpp-common/bt2c/join.hpp \
	@top_srcdir@/src/cpp-common/bt2c/json-val-req.hpp \
	@top_srcdir@/src/cpp-common/bt2c/json-val.hpp \
	@top_srcdir@/src/cpp-common/bt2c/libc-up.hpp \
	@top_srcdir@/src/cpp-common/bt2c/make-span.hpp \
	@top_srcdir@/src/cpp-common/bt2c/observable.hpp \
	@top_srcdir@/src/cpp-common/bt2c/parse-json-as-val.hpp \
	@top_srcdir@/src/cpp-common/bt2c/parse-json.hpp \
	@top_srcdir@/src/cpp-common/bt2c/prio-heap.hpp \
	@top_srcdir@/src/cpp-common/bt2c/read-fixed-len-int.hpp \
	@top_srcdir@/src/cpp-common/bt2c/regex.hpp \
	@top_srcdir@/src/cpp-common/bt2c/reverse-fixed-len-int-bits.hpp \
	@top_srcdir@/src/cpp-common/bt2c/safe-ops.hpp \
	@top_srcdir@/src/cpp-common/bt2c/std-int.hpp \
	@top_srcdir@/src/cpp-common/bt2c/str-scanner.hpp \
	@top_srcdir@/src/cpp-common/bt2c/text-loc-str.hpp \
	@top_srcdir@/src/cpp-common/bt2c/text-loc.hpp \
	@top_srcdir@/src/cpp-common/bt2c/type-traits.hpp \
	@top_srcdir@/src/cpp-common/bt2c/unicode-conv.hpp \
	@top_srcdir@/src/cpp-common/bt2c/uuid.hpp \
	@top_srcdir@/src/cpp-common/bt2c/val-req.hpp \
	@top_srcdir@/src/cpp-common/bt2c/vector.hpp \
	@top_srcdir@/src/cpp-common/bt2/borrowed-object-iterator.hpp \
	@top_srcdir@/src/cpp-common/bt2/borrowed-object-proxy.hpp \
	@top_srcdir@/src/cpp-common/bt2/borrowed-object.hpp \
	@top_srcdir@/src/cpp-common/bt2/component-class-dev.hpp \
	@top_srcdir@/src/cpp-common/bt2/error.hpp \
	@top_srcdir@/src/cpp-common/bt2/exc.hpp \
	@top_srcdir@/src/cpp-common/bt2/internal/utils.hpp \
	@top_srcdir@/src/cpp-common/bt2/message-array.hpp \
	@top_srcdir@/src/cpp-common/bt2/optional-borrowed-object.hpp \
	@top_srcdir@/src/cpp-common/bt2/shared-object.hpp \
	@top_srcdir@/src/cpp-common/bt2/wrap.hpp

MOSTLYCLEANFILES = $(DX_CLEANFILES)
EXTRA_DIST = \
	Doxyfile.in \
	style.css \
	all.dox \
	cpp.dox
