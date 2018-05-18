FDSERVER_INCLUDES = -I$(srcdir)/include \
                    -I$(top_srcdir)/include

AM_CPPFLAGS = $(FDSERVER_INCLUDES) \
              -W -Wall -Werror -Wstrict-prototypes -Wmissing-prototypes \
              -Wmissing-declarations -Wold-style-definition -Wpointer-arith \
              -Wcast-align -Wnested-externs -Wcast-qual -Wformat-nonliteral \
              -Wformat-security -Wundef -Wwrite-strings -Wformat-truncation=0 \
              -Wformat-overflow=0

lib_LTLIBRARIES = libfdserver.la
libfdserver_la_SOURCES = fdserver_lib.c
include_HEADERS = $(top_srcdir)/include/fdserver.h

bin_PROGRAMS = fdserver
fdserver_SOURCES = fdserver.c
