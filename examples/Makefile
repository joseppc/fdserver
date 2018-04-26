COMMON_SRC=../src/fdserver_common.c ../src/fdserver_lib.c
INCLUDES=-I ../include -I ../src/include

all: share_pipe_reader share_pipe_writer

clean:
	rm -f share_pipe_reader share_pipe_writer


share_pipe_reader: $(COMMON_SRC) share_pipe_reader.c
	gcc $(INCLUDES) -o $@ $^

share_pipe_writer: $(COMMON_SRC) share_pipe_writer.c
	gcc $(INCLUDES) -o $@ $^
