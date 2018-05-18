COMMON_SRC=../src/fdserver_lib.c
INCLUDES=-I ../include -I ../src/include

all: share_pipe_reader

clean:
	rm -f share_pipe_reader


share_pipe_reader: $(COMMON_SRC) share_pipe_reader.c
	gcc $(INCLUDES) -o $@ $^
