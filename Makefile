all: fdserver

AM_CPP_FLAGS = -O2  -W -Wall -Werror -Wstrict-prototypes -Wmissing-prototypes \
	       -Wmissing-declarations -Wold-style-definition -Wpointer-arith \
	       -Wcast-align -Wnested-externs -Wcast-qual -Wformat-nonliteral \
	       -Wformat-security -Wundef -Wwrite-strings -Wformat-truncation=0 \
	       -Wformat-overflow=0

INCLUDES = -I include \
	   -I src/include

fdserver: src/fdserver.c
	gcc $(AM_CPP_FLAGS) $(INCLUDES) $^ -o $@

clean:
	rm -f fdserver
	make -C examples clean
