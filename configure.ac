#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.66])
AC_CONFIG_AUX_DIR([aconfig])
AC_INIT([peakmem], [1.0.0], [slavinp@cs.man.ac.uk])
AM_INIT_AUTOMAKE([peakmem], [1.0.0])
AC_CONFIG_SRCDIR([src/peakmem.h])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([limits.h stdlib.h string.h sys/time.h sys/wait.h])
AC_CHECK_HEADERS([unistd.h sys/ptrace.h signal.h getopt.h sys/types.h])
AC_CHECK_HEADERS([poll.h sys/poll.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T
AC_TYPE_SIZE_T
# sys_siglist is extern.
AC_CHECK_DECLS([sys_siglist, optarg, optind, strsignal])
AC_CHECK_DECLS([waitpid],,,[#include <sys/wait.h>])
AC_CHECK_DECLS([nanosleep],,,[#define _POSIX_C_SOURCE 199309L
#include <time.h>])
# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_CHECK_FUNCS([gettimeofday strchr strrchr])
AC_CONFIG_FILES([Makefile src/Makefile man/Makefile])

AC_OUTPUT
