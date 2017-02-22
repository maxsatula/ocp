#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([ocp],[0.2],[Max Satula <maksym.satula@gmail.com>])
AM_INIT_AUTOMAKE
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/ocp.h])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
# native compiler goes first, otherwise may not compile in AIX
AC_PROG_CC([cc gcc])
AC_PROG_INSTALL
AC_PROG_RANLIB
AM_PROG_CC_C_O

AC_SYS_LARGEFILE

# Checks for libraries.
AX_LIB_ORACLE_OCI
if test "x${HAVE_ORACLE_OCI}" = xno; then
	AC_MSG_ERROR([----------------------------------------------
  This software requires Oracle installation
    1. Have you set \$ORACLE_HOME?
    2. Check if Oracle Home 64/32-bit is OK
    3. Check whether native compiler (not GCC)
       is used for non-Linux platforms
----------------------------------------------)])
fi
AC_SUBST([ORACLE_OCI_CFLAGS])
AC_SUBST([ORACLE_OCI_LDFLAGS])

#AC_SEARCH_LIBS([OCIEnvCreate], [clntsh])
AC_SEARCH_LIBS([poptGetContext], [popt])
AC_SEARCH_LIBS([deflateInit2_], [z])
AC_SEARCH_LIBS([clock_gettime], [rt])

# Checks for header files.
##AC_CHECK_HEADERS([popt.h zlib.h])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

AC_CONFIG_FILES([Makefile
	progressmeter/Makefile
	yesno/Makefile
	src/Makefile])
AC_OUTPUT
