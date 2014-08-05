#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.66])
AC_INIT([peakmem], [1.0.0], [slavinp@cs.man.ac.uk])
AM_INIT_AUTOMAKE([peakmem], [1.0.0])
AC_CONFIG_SRCDIR([src/peakmem.h])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([limits.h stdlib.h string.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_CHECK_DECLS([sys_siglist])

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_CHECK_FUNCS([gettimeofday strchr strrchr])

AC_OUTPUT([Makefile src/Makefile])
