AC_INIT([fdserver], [0.1])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

AC_PROG_CC
AM_PROG_AR

#Use libtool
LT_INIT([])
AC_PROG_LIBTOOL
AM_PROG_LIBTOOL

AC_HEADER_RESOLV
AC_CHECK_HEADERS([errno.h inttypes.h signal.h stdint.h stdio.h stdlib.h \
		  string.h sys/mman.h sys/prctl.h sys/random.h sys/socket.h \
		  sys/stat.h sys/types.h sys/un.h sys/wait.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_INT32_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T

AC_CONFIG_FILES([
		 Makefile
		 src/Makefile
		 tests/Makefile
		 ])

AC_OUTPUT
