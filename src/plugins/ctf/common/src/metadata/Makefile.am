# SPDX-License-Identifier: MIT

SUBDIRS = tsdl

noinst_LTLIBRARIES = libctf-src-metadata.la

libctf_src_metadata_la_SOURCES = \
	ctf-ir.hpp ctf-ir.cpp \
	ctf-ir-generator.hpp \
	ctf-ir-generator.cpp \
	finalize-trace-cls.hpp finalize-trace-cls.cpp

libctf_src_metadata_la_LIBADD = \
	$(builddir)/tsdl/libctf-parser.la \
	$(builddir)/tsdl/libctf-ast.la
