dnl Process this file with autoconf to produce a configure script.
dnl
dnl Copyright (c) 2017 EfficiOS, Inc.
dnl
dnl Permission is hereby granted, free of charge, to any person obtaining a copy
dnl of this software and associated documentation files (the "Software"), to deal
dnl in the Software without restriction, including without limitation the rights
dnl to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
dnl copies of the Software, and to permit persons to whom the Software is
dnl furnished to do so, subject to the following conditions:
dnl
dnl The above copyright notice and this permission notice shall be included in
dnl all copies or substantial portions of the Software.
dnl
dnl THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
dnl IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
dnl FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
dnl AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
dnl LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
dnl OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
dnl SOFTWARE.

##                     ##
## Autoconf base setup ##
##                     ##

AC_PREREQ([2.50])

m4_define([bt_version_major], [2])
m4_define([bt_version_minor], [0])
m4_define([bt_version_patch], [0])
m4_define([bt_version_extra], [-rc4])
m4_define([bt_version], bt_version_major[.]bt_version_minor[.]bt_version_patch[]bt_version_extra)

AC_INIT([babeltrace], bt_version, [jeremie dot galarneau at efficios dot com], [], [https://efficios.com/babeltrace/])

# Following the numbering scheme proposed by libtool for the library version
# http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
m4_define([bt_lib_version_current], [0])
m4_define([bt_lib_version_revision], [0])
m4_define([bt_lib_version_age], [0])
m4_define([bt_lib_version], bt_lib_version_current[:]bt_lib_version_revision[:]bt_lib_version_age)

AC_SUBST([BABELTRACE_LIBRARY_VERSION], bt_lib_version)

AC_CONFIG_HEADERS([src/common/config.h])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([m4])

AC_DEFINE([BT_VERSION_MAJOR], bt_version_major, [Babeltrace major version])
AC_DEFINE([BT_VERSION_MINOR], bt_version_minor, [Babeltrace minor version])
AC_DEFINE([BT_VERSION_PATCH], bt_version_patch, [Babeltrace patch version])
AC_DEFINE([BT_VERSION_EXTRA], ["]bt_version_extra["], [Babeltrace extra version])

AC_CANONICAL_TARGET
AC_CANONICAL_HOST


##                     ##
## Automake base setup ##
##                     ##

AM_INIT_AUTOMAKE([1.12 foreign dist-bzip2 no-dist-gzip tar-ustar nostdinc])
AM_MAINTAINER_MODE([enable])

# Enable silent rules if available (Introduced in AM 1.11)
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])


##                      ##
## OS specific defaults ##
##                      ##

MINGW32=no
DEFAULT_ENABLE_DEBUG_INFO=yes
LT_NO_UNDEFINED=""
AS_CASE([$host_os],
  [solaris*|darwin*],
    [
	DEFAULT_ENABLE_DEBUG_INFO=no
    ],
  [cygwin*],
    [
	DEFAULT_ENABLE_DEBUG_INFO=no
	LT_NO_UNDEFINED="-no-undefined"
    ],
  [mingw*],
    [
	MINGW32=yes
	DEFAULT_ENABLE_DEBUG_INFO=no
	LT_NO_UNDEFINED="-no-undefined"
    ]
)

AM_CONDITIONAL([BABELTRACE_BUILD_WITH_MINGW], [test "x$MINGW32" = "xyes"])
AC_SUBST(LT_NO_UNDEFINED)


##                   ##
## C compiler checks ##
##                   ##

AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE

# Choose the c compiler
AC_PROG_CC

# Make sure the c compiler supports C99
AC_PROG_CC_C99([], [AC_MSG_ERROR([The compiler does not support C99])])

# Make sure the c compiler supports __attributes__
AX_C___ATTRIBUTE__
AS_IF([test "x$ax_cv___attribute__" != "xyes"],
  [AC_MSG_ERROR([The compiler does not support __attribute__ extensions])])

# Make sur we have pthread support
AX_PTHREAD([], [AC_MSG_ERROR([Could not configure pthread support])])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_C_TYPEOF
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_INT8_T
AC_TYPE_OFF_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T
AC_CHECK_TYPES([ptrdiff_t])


##               ##
## Header checks ##
##               ##

AC_HEADER_STDBOOL
AC_CHECK_HEADERS([ \
	fcntl.h \
	float.h \
	ftw.h \
	libintl.h \
	limits.h \
	malloc.h \
	netdb.h \
	netinet/in.h \
	stddef.h \
	sys/socket.h \
	sys/time.h
])


##               ##
## Linker checks ##
##               ##

# Check if the linker supports no-as-needed
AX_APPEND_LINK_FLAGS([-Wl,--no-as-needed], [LD_NO_AS_NEEDED])
AC_SUBST([LD_NO_AS_NEEDED])

# Check if the linker supports whole-archive
AX_CHECK_LINK_FLAG([-Wl,--whole-archive,--no-whole-archive],
  [
    AC_SUBST([LD_WHOLE_ARCHIVE], [-Wl,--whole-archive,])
    AC_SUBST([LD_NO_WHOLE_ARCHIVE], [,--no-whole-archive])
  ],[
    # Fallback to force_load for the macOS linker
    AX_CHECK_LINK_FLAG([-Wl,-force_load],
      [
        AC_SUBST([LD_WHOLE_ARCHIVE], [-Wl,-force_load,])
        AC_SUBST([LD_NO_WHOLE_ARCHIVE], [])
      ],[
        AC_MSG_WARN([Can't find a linker option to force the inclusion of the static plugin archive objects.])
      ]
    )
  ]
)

# Initialize and configure libtool
LT_INIT([win32-dll])


##                 ##
## Programs checks ##
##                 ##

AC_PROG_MAKE_SET
AC_PROG_MKDIR_P
AC_PROG_LN_S

# set $IN_GIT_REPO if we're in the Git repository; the `bootstrap` file
# is not distributed in tarballs
AS_IF([test -f "$srcdir/bootstrap"], [in_git_repo=yes], [in_git_repo=no])
AM_CONDITIONAL([IN_GIT_REPO], [test "x$in_git_repo" = "xyes"])

# check for bison
AC_PROG_YACC
AX_PROG_BISON_VERSION([2.5], [have_bison=yes])

AS_IF([test "x$have_bison" != "xyes"], [
  AS_IF([test "x$in_git_repo" = "xyes"], [
    AC_MSG_FAILURE([dnl
Bison >= 2.4 is required when building from the Git repository. You can
set the YACC variable to override automatic detection.
    ])
  ], [
    AC_MSG_WARN([dnl
Missing Bison >= 2.4. Note that the parser files are already built in
this distribution tarball, so Bison is only needed if you intend to
modify their sources. You can set the YACC variable to override automatic
detection.
    ])
  ])
])
AM_CONDITIONAL([HAVE_BISON], [test "x$have_bison" = "xyes"])

# check for flex
AC_PROG_LEX
AX_PROG_FLEX_VERSION([2.5.35], [have_flex=yes])

AS_IF([test "x$have_flex" != "xyes"], [
  AS_IF([test "x$in_git_repo" = "xyes"], [
    AC_MSG_FAILURE([dnl
Flex >= 2.5.35 is required when building from the Git repository. You can
set the LEX variable to override automatic detection.
    ])
  ], [
    AC_MSG_WARN([dnl
Missing Flex >= 2.5.35. Note that the lexer files are already built in
this distribution tarball, so Flex is only needed if you intend to
modify their sources. You can set the LEX variable to override automatic
detection.
    ])
  ])
])
AM_CONDITIONAL([HAVE_FLEX], [test "x$have_flex" = "xyes"])


##                ##
## Library checks ##
##                ##

# Check what libraries are required on this platform to link sockets programs.
AX_LIB_SOCKET_NSL

# Check for glib >= 2.28 with gmodule support
AM_PATH_GLIB_2_0([2.28.0], [],
  AC_MSG_ERROR([glib >= 2.28 is required - download it from ftp://ftp.gtk.org/pub/gtk]),
  [gmodule-no-export]
)

# Checks for library functions.
AC_FUNC_ALLOCA
AC_FUNC_FORK
AC_FUNC_MKTIME
AC_FUNC_MMAP
AC_FUNC_STRERROR_R
AC_FUNC_STRNLEN
AC_CHECK_FUNCS([ \
	atexit \
	dup2 \
	ftruncate \
	gethostbyname \
	gettimeofday \
	localtime_r \
	memchr \
	memset \
	mkdir \
	mkdtemp \
	munmap \
	rmdir \
	setenv \
	socket \
	strchr \
	strdup \
	strerror \
	strndup \
	strnlen \
	strrchr \
	strstr \
	strtoul \
	strtoull \
	tzset \
])

# AC_FUNC_MALLOC causes problems when cross-compiling.
#AC_FUNC_MALLOC
#AC_FUNC_REALLOC

# Check for fmemopen
AC_CHECK_LIB([c], [fmemopen],
  [AC_DEFINE_UNQUOTED([BABELTRACE_HAVE_FMEMOPEN], 1, [Has fmemopen support.])]
)

# Check for open_memstream
AC_CHECK_LIB([c], [open_memstream],
  [AC_DEFINE_UNQUOTED([BABELTRACE_HAVE_OPEN_MEMSTREAM], 1, [Has open_memstream support.])]
)

# Check for posix_fallocate
AC_CHECK_LIB([c], [posix_fallocate],
  [AC_DEFINE_UNQUOTED([BABELTRACE_HAVE_POSIX_FALLOCATE], 1, [Has posix_fallocate support.])]
)

##                 ##
## User variables  ##
##                 ##

AC_ARG_VAR([PYTHON_INCLUDE], [Include flags for Python, bypassing python-config])
AC_ARG_VAR([PYTHON_LDFLAGS], [Linker flags for Python, bypassing python-config])
AC_ARG_VAR([PYTHON_CONFIG], [Path to python-config])

# BABELTRACE_PLUGINS_DIR: Plugins directory
AC_ARG_VAR([BABELTRACE_PLUGINS_DIR], [built-in plugins install directory [LIBDIR/babeltrace2/plugins]])
AS_IF([test "x$BABELTRACE_PLUGINS_DIR" = x], [BABELTRACE_PLUGINS_DIR='${libdir}/babeltrace2/plugins'])

# BABELTRACE_PLUGIN_PROVIDERS_DIR: Plugin providers directory
AC_ARG_VAR([BABELTRACE_PLUGIN_PROVIDERS_DIR], [built-in plugin providers install directory [LIBDIR/babeltrace2/plugin-providers]])
AS_IF([test "x$BABELTRACE_PLUGIN_PROVIDERS_DIR" = x], [BABELTRACE_PLUGIN_PROVIDERS_DIR='${libdir}/babeltrace2/plugin-providers'])

# BABELTRACE_MINIMAL_LOG_LEVEL:
AC_ARG_VAR([BABELTRACE_MINIMAL_LOG_LEVEL], [Minimal log level for Babeltrace program, library, and plugins (TRACE, DEBUG (default), or INFO)])
AS_IF([test "x$BABELTRACE_MINIMAL_LOG_LEVEL" = x], [BABELTRACE_MINIMAL_LOG_LEVEL="DEBUG"])
AS_IF([test "$BABELTRACE_MINIMAL_LOG_LEVEL" != "TRACE" && \
       test "$BABELTRACE_MINIMAL_LOG_LEVEL" != "DEBUG" && \
       test "$BABELTRACE_MINIMAL_LOG_LEVEL" != "INFO"],
  [AC_MSG_ERROR([Invalid BABELTRACE_MINIMAL_LOG_LEVEL value ($BABELTRACE_MINIMAL_LOG_LEVEL): use TRACE, DEBUG, or INFO.])]
)
AC_DEFINE_UNQUOTED([BT_MINIMAL_LOG_LEVEL], [BT_LOG_$BABELTRACE_MINIMAL_LOG_LEVEL], [Minimal log level])

# BABELTRACE_DEV_MODE:
AC_ARG_VAR([BABELTRACE_DEV_MODE], [Set to 1 to enable the Babeltrace developer mode (enables run-time checks for plugin developers)])
AS_IF([test "x$BABELTRACE_DEV_MODE" = x1], [
	AC_DEFINE([BT_DEV_MODE], 1, [Babeltrace developer mode])
], [BABELTRACE_DEV_MODE=0])

# BABELTRACE_DEBUG_MODE:
AC_ARG_VAR([BABELTRACE_DEBUG_MODE], [Set to 1 to enable the Babeltrace debug mode (enables internal assertions for Babeltrace maintainers)])
AS_IF([test "x$BABELTRACE_DEBUG_MODE" = x1], [
	AC_DEFINE([BT_DEBUG_MODE], 1, [Babeltrace debug mode])
], [BABELTRACE_DEBUG_MODE=0])


##                              ##
## Optionnal features selection ##
##                              ##

# Python bindings
# Disabled by default
AC_ARG_ENABLE([python-bindings],
  [AC_HELP_STRING([--enable-python-bindings], [build the Python bindings])],
  [], dnl AC_ARG_ENABLE will fill enable_python_bindings with the user choice
  [enable_python_bindings=no]
)

# Python bindings documentation
# Disabled by default
AC_ARG_ENABLE([python-bindings-doc],
  [AC_HELP_STRING([--enable-python-bindings-doc], [build the Python bindings documentation])],
  [], dnl AC_ARG_ENABLE will fill enable_python_bindings_doc with the user choice
  [enable_python_bindings_doc=no]
)

# Python plugins
# Disabled by default
AC_ARG_ENABLE([python-plugins],
  [AC_HELP_STRING([--enable-python-plugins], [enable the Python plugins support for the library and converter])],
  [], dnl AC_ARG_ENABLE will fill enable_python_plugins with the user choice
  [enable_python_plugins=no]
)

# Debug info
# Enabled by default, except on some platforms
AC_ARG_ENABLE([debug-info],
  [AC_HELP_STRING([--disable-debug-info], [disable the debug info support (default on macOS, Solaris and Windows)])],
  [], dnl AC_ARG_ENABLE will fill enable_debug_info with the user choice
  [enable_debug_info="$DEFAULT_ENABLE_DEBUG_INFO"]
)

# API documentation
# Disabled by default
AC_ARG_ENABLE([api-doc],
  [AC_HELP_STRING([--enable-api-doc], [build the HTML API documentation])],
  [enable_api_doc=$enableval],
  [enable_api_doc=no]
)

# Built-in plugins
# Disabled by default
AC_ARG_ENABLE([built-in-plugins],
  [AC_HELP_STRING([--enable-built-in-plugins], [Statically-link in-tree plug-ins into the babeltrace2 executable])],
  [], dnl AC_ARG_ENABLE will fill enable_built_in_plugins with the user choice
  [enable_built_in_plugins=no]
)

# Built-in python plugin support
# Disabled by default
AC_ARG_ENABLE([built-in-python-plugin-support],
  [AC_HELP_STRING([--enable-built-in-python-plugin-support], [Statically-link Python plugin support into the babeltrace library])],
  [], dnl AC_ARG_ENABLE will fill enable_built_in_python_plugin_support with the user choice
  [enable_built_in_python_plugin_support=no]
)

# Man pages
# Enabled by default
AC_ARG_ENABLE([man-pages],
  [AS_HELP_STRING([--disable-man-pages], [Do not build and install man pages (already built in a distributed tarball])],
  [], dnl AC_ARG_ENABLE will fill enable_built_in_plugins with the user choice
  [enable_man_pages=yes]
)


# Set automake variables for optionnal feature conditionnals in Makefile.am
AM_CONDITIONAL([ENABLE_PYTHON_BINDINGS], [test "x$enable_python_bindings" = xyes])
AM_CONDITIONAL([ENABLE_PYTHON_BINDINGS_DOC], [test "x$enable_python_bindings_doc" = xyes])
AM_CONDITIONAL([ENABLE_PYTHON_PLUGINS], [test "x$enable_python_plugins" = xyes])
AM_CONDITIONAL([ENABLE_DEBUG_INFO], [test "x$enable_debug_info" = xyes])
AM_CONDITIONAL([ENABLE_API_DOC], [test "x$enable_api_doc" = xyes])
AM_CONDITIONAL([ENABLE_BUILT_IN_PLUGINS], [test "x$enable_built_in_plugins" = xyes])
AM_CONDITIONAL([ENABLE_BUILT_IN_PYTHON_PLUGIN_SUPPORT], [test "x$enable_built_in_python_plugin_support" = xyes])
AM_CONDITIONAL([ENABLE_MAN_PAGES], [test "x$enable_man_pages" = xyes])
AM_CONDITIONAL([ENABLE_PYTHON_COMMON_DEPS], [test "x$enable_python_bindings" = xyes || test "x$enable_python_plugins" = xyes])

# Set defines for optionnal features conditionnals in the source code

## jgalar: still used?
AS_IF([test "x$enable_python_plugins" = xyes],
  [AC_DEFINE([PYTHON_PLUGINS], [1], [Python plugin support.])]
)

AS_IF([test "x$enable_debug_info" = xyes],
  [AC_DEFINE([ENABLE_DEBUG_INFO], [1], [Define to 1 if you enable the 'debug info' feature])]
)

AS_IF([test "x$enable_built_in_plugins" = xyes],
  [AC_DEFINE([BT_BUILT_IN_PLUGINS], [1], [Define to 1 to register plug-in attributes in static executable sections])]
)

AS_IF([test "x$enable_built_in_python_plugin_support" = xyes],
  [AC_DEFINE([BT_BUILT_IN_PYTHON_PLUGIN_SUPPORT], [1], [Define to 1 to register plug-in attributes in static executable sections])]
)

AS_IF([test "x$enable_debug_info" = xyes],
  [ENABLE_DEBUG_INFO_VAL=1],
  [ENABLE_DEBUG_INFO_VAL=0]
)

AC_SUBST([ENABLE_DEBUG_INFO_VAL])


# Check for conflicting optional features user choices

AS_IF([test "x$enable_python_bindings" = xno],
  [
    AS_IF([test "x$enable_python_bindings_doc" = xyes], [AC_MSG_ERROR([--enable-python-bindings-doc was specified without --enable-python-bindings])])
  ]
)

AS_IF([test "x$enable_built_in_plugins" = xyes],
  [
    # Built-in plug-ins are only available when the --disable-shared --enable-static options are used.
    AS_IF([test "x$enable_static" != xyes], [AC_MSG_ERROR(--enable-static must be used to bundle plug-ins in the babeltrace2 executable)])
    AS_IF([test "x$enable_shared" = xyes], [AC_MSG_ERROR(--disable-shared must be used to bundle plug-ins in the babeltrace2 executable)])
  ]
)

AS_IF([test "x$enable_built_in_python_plugin_support" = xyes],
  [
    AS_IF([test "x$enable_python_plugins" = xno], [AC_MSG_ERROR([--enable-python-plugins must be used to bundle Python plugin support in the babeltrace2 executable])])
    # Built-in plug-ins are only available when the --disable-shared --enable-static options are used.
    AS_IF([test "x$enable_static" != xyes], [AC_MSG_ERROR(--enable-static must be used to bundle Python plugin support in the babeltrace2 executable)])
    AS_IF([test "x$enable_shared" = xyes], [AC_MSG_ERROR(--disable-shared must be used to bundle Python plugin support in the babeltrace2 executable)])
  ]
)


# Check for optionnal features dependencies

AS_IF([test "x$enable_python_bindings" = xyes],
  [AX_PKG_SWIG(2.0.0, [], [AC_MSG_ERROR([SWIG 2.0.0 or newer is required to build the python bindings])])]
)

AM_PATH_PYTHON([3.0], [
  AM_PATH_PYTHON_MODULES([PYTHON])

  # pythondir is the path where extra modules are to be installed
  pythondir=$PYTHON_PREFIX/$PYTHON_MODULES_PATH

  # pyexecdir is the path that contains shared objects used by the extra modules
  pyexecdir=$PYTHON_EXEC_PREFIX/$PYTHON_MODULES_PATH

  AS_IF([test -z "$PYTHON_CONFIG"], [
    AC_PATH_PROGS([PYTHON_CONFIG],
                  [python$PYTHON_VERSION-config python-config],
                  [],
                  [`dirname $PYTHON`])
  ])

  AS_IF([test -n "$PYTHON_CONFIG"], [
    AS_IF([test -z "$PYTHON_INCLUDE"], [
      AC_MSG_CHECKING([Python include flags])
      PYTHON_INCLUDE=`$PYTHON_CONFIG --includes`
      AC_MSG_RESULT([$PYTHON_INCLUDE])
    ])

    AS_IF([test -z "$PYTHON_LDFLAGS"], [
      AC_MSG_CHECKING([Python library flags])
      # Python 3.8+ requires that we pass --embed to get the -lpython3.x flag.
      AS_IF([! PYTHON_LDFLAGS=`$PYTHON_CONFIG --ldflags --embed`], [
        PYTHON_LDFLAGS=`$PYTHON_CONFIG --ldflags`
      ])
      AC_MSG_RESULT([$PYTHON_LDFLAGS])
    ])
  ])
], [:])

AS_IF([test "$PYTHON" != :], [have_python=yes], [have_python=no])
AS_IF([test -n "$PYTHON_CONFIG"], [have_python_dev=yes], [have_python_dev=no])

AM_CONDITIONAL([HAVE_PYTHON], [test "x$have_python" = xyes])
AM_CONDITIONAL([HAVE_PYTHON_DEV], [test "x$have_python_dev" = xyes])

AS_IF([test "x$enable_python_bindings" = xyes || test "x$enable_python_plugins" = xyes], [
  AS_IF([test "x$have_python_dev" = xno], [
    AC_MSG_ERROR([Cannot find a suitable python-config. You can override the python-config path with the PYTHON_CONFIG environment variable.])
  ])
])

AS_IF([test "x$enable_python_bindings_doc" = xyes],
  [
    AM_CHECK_PYTHON_SPHINX([PYTHON])
    AS_IF([test "x$PYTHON_SPHINX_EXISTS" = xno],
      [AC_MSG_ERROR([The Sphinx package for Python 3 is required to build the Python bindings documentation])]
    )
  ]
)

AS_IF([test "x$enable_debug_info" = xyes],
  [
    # Check if libelf and libdw are present
    AC_CHECK_LIB([elf], [elf_version], [:], [AC_MSG_ERROR(Missing libelf (from elfutils >= 0.154) which is required by debug info. You can disable this feature using --disable-debug-info.)])
    AC_CHECK_LIB([dw], [dwarf_begin], [:], [AC_MSG_ERROR(Missing libdw (from elfutils >= 0.154) which is required by debug info. You can disable this feature using --disable-debug-info.)])
    BT_LIB_ELFUTILS([0], [154], [:], [AC_MSG_ERROR(elfutils >= 0.154 is required to use the debug info feature. You can disable this feature using --disable-debug-info.)])
    ELFUTILS_LIBS="-lelf -ldw"
  ]
)
AC_SUBST([ELFUTILS_LIBS])

AS_IF([test "x$enable_api_doc" = "xyes"],
  [
    DX_DOXYGEN_FEATURE(ON)
    DX_DOT_FEATURE(OFF)
    DX_HTML_FEATURE(ON)
    DX_CHM_FEATURE(OFF)
    DX_CHI_FEATURE(OFF)
    DX_MAN_FEATURE(OFF)
    DX_RTF_FEATURE(OFF)
    DX_XML_FEATURE(OFF)
    DX_PDF_FEATURE(OFF)
    DX_PS_FEATURE(OFF)
    DX_INIT_DOXYGEN([Babeltrace], [$(srcdir)/Doxyfile], [output])
    AS_IF([test -z "$DX_DOXYGEN"],
	  [AC_MSG_ERROR([You need doxygen to enable the API documentation])]
    )
  ]
)

have_asciidoc_xmlto=no
warn_prebuilt_man_pages=no

AC_PATH_PROG([ASCIIDOC], [asciidoc], [no])
AC_PATH_PROG([XMLTO], [xmlto], [no])

AS_IF([test "x$enable_man_pages" = "xyes"], [
  AS_IF([test "x$ASCIIDOC" = "xno" || test "x$XMLTO" = "xno"], [
    AS_IF([test "x$in_git_repo" = "xyes"], [
      # this is an error because we're in the Git repo, which
      # means the man pages are not already generated for us,
      # thus asciixmlto are required because we were asked
      # to build the man pages
      AC_MSG_ERROR([
You need asciidoc and xmlto to build the Babeltrace man pages. Use
--disable-man-pages to disable building the man pages, in which case
they will not be installed.
      ])
    ], [
      # only warn here: since we're in the tarball, the man
      # pages should already be generated at this point, thus
      # asciidoc/xmlto are not strictly required
      warn_prebuilt_man_pages=yes
    ])
  ], [
    have_asciidoc_xmlto=yes
  ])
])

# export AsciiDoc and xmlto existence
AM_CONDITIONAL([HAVE_ASCIIDOC_XMLTO], [test "x$have_asciidoc_xmlto" = "xyes"])

# a wonderful hack that seems necessary because $libdir is
# literally `${exec_prefix}/lib`, and $exec_prefix is set to `NONE`
# by autoconf when it's not specified by the user
AS_IF([test "x$exec_prefix" = xNONE], [
  AS_IF([test "x$prefix" = xNONE], [
    PREFIX="$ac_default_prefix"
  ], [
    PREFIX="$prefix"
  ])

  LIBDIR="$PREFIX/lib"
], [
  LIBDIR="$libdir"
])

AC_SUBST(LIBDIR)

# CFLAGS from libraries (the glib ones are needed for the following sizeof
# test).
AM_CFLAGS="${PTHREAD_CFLAGS} ${GLIB_CFLAGS}"

# Check that the current size_t matches the size that glib thinks it should
# be. This catches problems on multi-arch where people try to do a 32-bit
# build while pointing at 64-bit glib headers. This is a common error because
# glib.h is not platform specific but it includes glibconfig.h which is and
# is usually installed in a non-standard path.

# Do this before enabling all the warning flags, as the
# AC_LANG_PROGRAM-generated program may generate some warning, which makes this
# test fail unnecessarily.

# Older versions of the pkg-config macros disallows PKG_* in the autoconf
# output. Specifically allow pkg_config_libdir to be able to print the
# error message.
m4_pattern_allow([PKG_CONFIG_LIBDIR])

save_CFLAGS=${CFLAGS}
CFLAGS="${CFLAGS} ${AM_CFLAGS}"
AC_COMPILE_IFELSE([
  AC_LANG_PROGRAM([dnl
#include <glib.h>
#include <unistd.h>
  ], [dnl
G_STATIC_ASSERT(sizeof(size_t) == GLIB_SIZEOF_SIZE_T);
  ])
],[:],[
  AC_MSG_ERROR([dnl
sizeof(size_t) doesn't match GLIB_SIZEOF_SIZE_T. You probably need to set
PKG_CONFIG_LIBDIR to point to the right pkg-config files for your build
target.
  ])
])
CFLAGS=${save_CFLAGS}

# Detect C and LD warning flags supported by the compiler.
AX_COMPILER_FLAGS(
	[WARN_CFLAGS], dnl CFLAGS variable name
	[WARN_LDFLAGS], dnl LDFLAGS variable name (unused for now)
	[], dnl is-release
	[], dnl Extra base CFLAGS
	[ dnl Extra "yes" CFLAGS
		dnl Disable these flags, either because we don't want them
		dnl or because we want them but are not ready to enable them
		dnl yet.
		-Wno-sign-compare dnl
		-Wno-inline dnl
		-Wno-declaration-after-statement dnl
		-Wno-switch-enum dnl
		-Wno-switch-default dnl
		-Wno-packed dnl
		-Wno-pointer-arith dnl
		-Wno-format-nonliteral dnl
		-Wno-double-promotion dnl
		-Wno-cast-align dnl
		dnl
		dnl Some versions of SWIG (like 3.0.12) generate code that produces
		dnl -Wcast-function-type warnings.  This warning is present in gcc >= 8.  This
		dnl combo happens on RHEL/Centos 8, for example.  Later versions of SWIG (like
		dnl 4.0.1) have the correct function signatures to not produce this warning.
		dnl It's simpler to just disable the warning globally.
		dnl
		dnl Note that the Debian/Ubuntu SWIG package 3.0.12-2 contains a local patch to
		dnl fix this (python-fix-function-cast-warnings.patch), so you won't be able to
		dnl reproduce the warning using that package.
		dnl
		dnl Ref: https://github.com/swig/swig/issues/1259
		-Wno-cast-function-type dnl
	])

# CFLAGS from AX_COMPILER_FLAGS.
AM_CFLAGS="${AM_CFLAGS} ${WARN_CFLAGS}"

# The test used in AX_COMPILER_FLAGS, generated using AC_LANG_PROGRAM, is
# written in such a way that it triggers a -Wold-style-definition warning.  So
# if the user has -Werror in their CFLAGS, that warning flag will end up
# disabled, because the test program will not build.
#
# Enable it here unconditionally.  It is supported by GCC >= 4.8 and by Clang
# (it is accepted for compatibility although it has no effect), and there is
# not reason to not want it.

AM_CFLAGS="${AM_CFLAGS} -Wold-style-definition"

# We want this one to always be an error.
AM_CFLAGS="${AM_CFLAGS} -Werror=implicit-function-declaration"

# Done for AM_CFLAGS.
AC_SUBST(AM_CFLAGS)

# Set global CPPFLAGS in AM_CPPFLAGS
AM_CPPFLAGS="-I\$(top_srcdir)/include -I\$(top_builddir)/src -I\$(top_srcdir)/src -include common/config.h"
AC_SUBST(AM_CPPFLAGS)

# Add glib to global link libs
LIBS="$LIBS $GLIB_LIBS"

# Abuse autoconf's AC_ARG_PROGRAM output variable 'program_transform_name'
# to rename babeltrace2.bin to babeltrace2 at install time.
program_transform_name="s&babeltrace2\.bin&babeltrace2&;$program_transform_name"
AC_SUBST(program_transform_name)

AC_CONFIG_FILES([
	doc/api/Doxyfile
	doc/api/Makefile
	doc/bindings/Makefile
	doc/bindings/python/Makefile
	doc/contributing-images/Makefile
	doc/Makefile
	doc/man/asciidoc-attrs.conf
	doc/man/Makefile
	include/Makefile
	Makefile
	src/argpar/Makefile
	src/autodisc/Makefile
	src/babeltrace2-ctf-writer.pc
	src/babeltrace2.pc
	src/bindings/Makefile
	src/bindings/python/bt2/bt2/version.py
	src/bindings/python/bt2/Makefile
	src/bindings/python/bt2/setup.py
	src/bindings/python/Makefile
	src/cli/Makefile
	src/common/Makefile
	src/compat/Makefile
	src/ctfser/Makefile
	src/ctf-writer/Makefile
	src/fd-cache/Makefile
	src/lib/graph/Makefile
	src/lib/graph/message/Makefile
	src/lib/Makefile
	src/lib/plugin/Makefile
	src/lib/prio-heap/Makefile
	src/lib/trace-ir/Makefile
	src/logging/Makefile
	src/Makefile
	src/plugins/common/Makefile
	src/plugins/common/muxing/Makefile
	src/plugins/common/param-validation/Makefile
	src/plugins/ctf/common/bfcr/Makefile
	src/plugins/ctf/common/Makefile
	src/plugins/ctf/common/metadata/Makefile
	src/plugins/ctf/common/msg-iter/Makefile
	src/plugins/ctf/fs-sink/Makefile
	src/plugins/ctf/fs-src/Makefile
	src/plugins/ctf/lttng-live/Makefile
	src/plugins/ctf/Makefile
	src/plugins/lttng-utils/debug-info/Makefile
	src/plugins/lttng-utils/Makefile
	src/plugins/Makefile
	src/plugins/text/dmesg/Makefile
	src/plugins/text/Makefile
	src/plugins/text/pretty/Makefile
	src/plugins/text/details/Makefile
	src/plugins/utils/counter/Makefile
	src/plugins/utils/dummy/Makefile
	src/plugins/utils/Makefile
	src/plugins/utils/muxer/Makefile
	src/plugins/utils/trimmer/Makefile
	src/py-common/Makefile
	src/python-plugin-provider/Makefile
	src/param-parse/Makefile
	src/string-format/Makefile
	tests/bitfield/Makefile
	tests/ctf-writer/Makefile
	tests/lib/Makefile
	tests/lib/test-plugin-plugins/Makefile
	tests/Makefile
	tests/param-validation/Makefile
	tests/plugins/Makefile
	tests/plugins/src.ctf.fs/Makefile
	tests/plugins/src.ctf.fs/succeed/Makefile
	tests/plugins/sink.ctf.fs/Makefile
	tests/plugins/sink.ctf.fs/succeed/Makefile
	tests/plugins/flt.lttng-utils.debug-info/Makefile
	tests/plugins/flt.utils.muxer/Makefile
	tests/plugins/flt.utils.muxer/succeed/Makefile
	tests/plugins/flt.utils.trimmer/Makefile
	tests/utils/Makefile
	tests/utils/tap/Makefile
])

AC_OUTPUT

#
# Mini-report on what will be built.
#

PPRINT_INIT
PPRINT_SET_INDENT(1)
PPRINT_SET_TS(38)

AS_ECHO
AS_ECHO("${PPRINT_COLOR_BLDBLU}Babeltrace $PACKAGE_VERSION$PPRINT_COLOR_RST")
AS_ECHO

PPRINT_SUBTITLE([System])

# Target architecture we're building for.
target_arch=$host_cpu
[
for f in $CFLAGS; do
	if test $f = "-m32"; then
		target_arch="32-bit"
	elif test $f = "-m64"; then
		target_arch="64-bit"
	fi
done
]

PPRINT_PROP_STRING([Target architecture], $target_arch)

AS_ECHO
PPRINT_SUBTITLE([Python 3 language support])
test "x$have_python" = "xyes" && value=1 || value=0
PPRINT_PROP_BOOL([Have Python interpreter], $value)
test "x$have_python_dev" = "xyes" && value=1 || value=0
PPRINT_PROP_BOOL([Have Python development libraries], $value)
test "x$enable_python_bindings" = "xyes" && value=1 || value=0
PPRINT_PROP_BOOL([Python bindings], $value)
test "x$enable_python_plugins" = "xyes" && value=1 || value=0
PPRINT_PROP_BOOL([Python plugin support], $value)
AS_IF([test "x$have_python" = "xyes"], [
  PPRINT_PROP_STRING([Python interpreter path], [$PYTHON])
])
AS_IF([test "x$have_python_dev" = "xyes"], [
  PPRINT_PROP_STRING([python-config path], [$PYTHON_CONFIG])
  PPRINT_PROP_STRING([Python include paths], [$PYTHON_INCLUDE])
  PPRINT_PROP_STRING([Python linker flags], [$PYTHON_LDFLAGS])
])
AS_IF([test "x$enable_python_bindings" = "xyes"], [
  PPRINT_PROP_STRING([SWIG executable], [$SWIG])
  PPRINT_PROP_STRING([SWIG library], [$SWIG_LIB])
])

AS_ECHO
PPRINT_SUBTITLE([Plugins])
PPRINT_PROP_BOOL(['ctf' plugin], 1)
test "x$enable_debug_info" = "xyes" && value=1 || value=0
PPRINT_PROP_BOOL(['lttng-utils' plugin], $value)
PPRINT_PROP_BOOL(['text' plugin], 1)
PPRINT_PROP_BOOL(['utils' plugin], 1)

AS_ECHO
PPRINT_SUBTITLE([Built-in features])
test "x$enable_built_in_plugins" = "xyes" && value=1 || value=0
PPRINT_PROP_BOOL([Built-in plugins], $value)
test "x$enable_built_in_python_plugin_support" = "xyes" && value=1 || value=0
PPRINT_PROP_BOOL([Built-in Python plugin support], $value)

AS_ECHO
PPRINT_SUBTITLE([Documentation])

# man pages build enabled/disabled
m4_pushdef([build_man_pages_msg], [Build and install man pages])

AS_IF([test "x$enable_man_pages" != "xno"], [
  AS_IF([test "x$in_git_repo" = "xyes"], [
    PPRINT_PROP_BOOL([build_man_pages_msg], 1)
  ], [
    AS_IF([test "x$have_asciidoc_xmlto" = "xyes"], [
      PPRINT_PROP_BOOL([build_man_pages_msg], 1)
    ], [
      PPRINT_PROP_STRING([build_man_pages_msg],
        [${PPRINT_COLOR_BLDGRN}yes (already built)],
        $PPRINT_COLOR_SUBTITLE)
    ])
  ])
], [
  PPRINT_PROP_BOOL([build_man_pages_msg], 0)
])

m4_popdef([build_man_pages_msg])

test "x$enable_api_doc" = "xyes" && value=1 || value=0
PPRINT_PROP_BOOL([HTML API documentation], $value)
test "x$enable_python_bindings_doc" = "xyes" && value=1 || value=0
PPRINT_PROP_BOOL([Python bindings documentation], $value)

AS_ECHO
PPRINT_SUBTITLE([Logging])
PPRINT_PROP_STRING([Minimal log level], $BABELTRACE_MINIMAL_LOG_LEVEL)

AS_ECHO
PPRINT_SUBTITLE([Special build modes])
PPRINT_PROP_BOOL([Debug mode], $BABELTRACE_DEBUG_MODE)
PPRINT_PROP_BOOL([Developer mode], $BABELTRACE_DEV_MODE)

report_bindir="`eval eval echo $bindir`"
report_libdir="`eval eval echo $libdir`"
report_sysconfdif="`eval eval echo $sysconfdir`"
report_pluginsdir="`eval eval eval echo $BABELTRACE_PLUGINS_DIR`"
report_pluginprovidersdir="`eval eval eval echo $BABELTRACE_PLUGIN_PROVIDERS_DIR`"

# Print the bindir and libdir this `make install' will install into.
AS_ECHO
PPRINT_SUBTITLE([Install directories])
PPRINT_PROP_STRING([Binaries], [$report_bindir])
PPRINT_PROP_STRING([Libraries], [$report_libdir])
PPRINT_PROP_STRING([Plugins], [$report_pluginsdir])
PPRINT_PROP_STRING([Plugin providers], [$report_pluginprovidersdir])
PPRINT_PROP_STRING([Configuration], [$report_sysconfdif])
