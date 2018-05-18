COMMON_SRC=../src/fdserver_lib.c
INCLUDES=-I ../include -I ../src/include

all: fdserver_api

clean:
	rm -f fdserver_api

fdserver_api: $(COMMON_SRC) fdserver_api.c
	gcc $(INCLUDES) -o $@ $^
