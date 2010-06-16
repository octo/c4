AC_INIT(collection, 4.0.0)
AC_CONFIG_SRCDIR(src/main.c)
AC_CONFIG_HEADERS(src/config.h)
AM_INIT_AUTOMAKE(dist-bzip2)
AC_LANG(C)

AC_PREFIX_DEFAULT("/opt/collection4")

#
# Check for programs/utilities
#
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LEX
AC_PROG_YACC
AM_CONDITIONAL(COMPILER_IS_GCC, test "x$GCC" = "xyes")

#
# Checks for header files.
#
AC_HEADER_STDC
AC_CHECK_HEADERS(stdbool.h sys/types.h sys/socket.h netdb.h)

AC_CHECK_HEADERS(fcgiapp.h fcgi_stdio.h rrd.h, [],
		 [AC_MSG_ERROR(a required header file cannot be found.)])

AC_CHECK_LIB(fcgi, FCGI_Accept, [],
	     [AC_MSG_ERROR(cannot find libfcgi.)])
AC_CHECK_LIB(rrd_th, rrd_graph_v, [],
	     [AC_MSG_ERROR(cannot find librrd_th.)], [-lm])

AC_OUTPUT(Makefile src/Makefile)
