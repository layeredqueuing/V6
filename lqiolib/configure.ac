#                                               -*- Autoconf -*-
# $Id: configure.ac 13832 2020-09-18 12:58:04Z greg $
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.58)
AC_INIT(liblqio, 0.4.0, martinmroz@gmail.com)
AC_CONFIG_AUX_DIR([../config])
AC_CONFIG_MACRO_DIR([../config])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_LIBTOOL_WIN32_DLL
AC_PROG_LIBTOOL
AM_PROG_LEX
AC_PROG_YACC
AC_PROG_CXX
AC_PROG_CC
AC_LANG(C++)

# Checks for header files.
AC_FUNC_ALLOCA
AC_HEADER_STDC
AC_CHECK_HEADERS([sys/mman.h])
AC_CHECK_HEADERS([float.h inttypes.h pwd.h malloc.h stddef.h stdlib.h string.h strings.h unistd.h values.h time.h])
AC_CHECK_HEADERS([sys/param.h sys/resource.h sys/time.h sys/times.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STAT
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE
AC_C_BIGENDIAN

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_STAT
AC_FUNC_STRTOD
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([ctime getopt_long getsubopt pow rint strcasecmp strchr strdup strerror strrchr strftime])
AC_CHECK_FUNCS([geteuid getpwuid getrusage gettimeofday uname mmap])

# Check for libexpat
AC_CHECK_HEADERS([expat.h])
AC_CHECK_LIB([expat], [XML_ParserCreateNS])

AM_CONDITIONAL([EXPAT], [test "x${ac_cv_lib_expat_XML_ParserCreateNS}" = "xyes"] )

# Produce the desired output files
AC_CONFIG_FILES([Makefile src/Makefile src/headers/lqio/Makefile])
AC_OUTPUT
