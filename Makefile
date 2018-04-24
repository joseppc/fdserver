all: fdserver


fdserver: src/fdserver.c src/fdserver_common.c
	gcc -I include -I src/include $^ -o $@

clean:
	rm -f fdserver
