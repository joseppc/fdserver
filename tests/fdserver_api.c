#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include <fdserver.h>

#define WELL_KNOWN_MESSAGE (int)(0xbadcafe)

#define KEY_READER 0
#define KEY_WRITER 1

fdserver_context_e context = FD_SRV_CTX_ISHM;

struct Test {
	int (*run_test)(void);
	const char *name;
};

static int create_context(void)
{
	return fdserver_new_context();
}

static int request_missing_fd(void)
{
	int fd;

	fd = fdserver_lookup_fd(context, KEY_READER);
	if (fd != -1) {
		close(fd);
		return 1;
	}

	return 0;
}

static int register_fds(void)
{
	int fd[2];
	int ret;

	if (pipe(fd) == -1)
		return 1;

	ret = fdserver_register_fd(context, KEY_READER, fd[0]);
	if (ret == -1)
		goto close_exit;

	ret = fdserver_register_fd(context, KEY_WRITER, fd[1]);
	if (ret == -1)
		goto close_exit;

	return 0;

close_exit:
	close(fd[0]);
	close(fd[1]);

	return ret;
}

static int lookup_writer(void)
{
	int fd;
	int msg = WELL_KNOWN_MESSAGE;

	fd = fdserver_lookup_fd(context, KEY_WRITER);
	if (fd == -1)
		return 1;

	write(fd, &msg, sizeof(msg));
	/* assume success */
	close(fd);
	return 0;
}

static int lookup_reader(void)
{
	int fd;
	int msg;

	fd = fdserver_lookup_fd(context, KEY_READER);
	if (fd == -1)
		return 1;

	read(fd, &msg, sizeof(msg));
	close(fd);
	if (msg != WELL_KNOWN_MESSAGE)
		return 1;

	return 0;
}

static int deregister_fds(void)
{
	int retval = 0;

	if (fdserver_deregister_fd(context, KEY_READER) == -1)
		retval++;
	if (fdserver_deregister_fd(context, KEY_WRITER) == -1)
		retval++;

	return retval;
}

static int terminate_server(void)
{
	if (fdserver_terminate(context) == -1)
		return 1;
	return 0;
}

static int terminate_unexisting_server(void)
{
	if (fdserver_terminate(context) != -1)
		return 1;
	return 0;
}

struct Test tests_suite[] = {
	{ create_context, "Create context" },
	{ request_missing_fd, "Request missing fd" },
	{ register_fds, "Register two file descriptors" },
	{ lookup_writer, "Lookup writer fd" },
	{ lookup_reader, "Lookup reader fd" },
	{ deregister_fds, "Deregistering file descriptors" },
	{ request_missing_fd, "Request missing fd" },
	{ terminate_server, "Terminate server" },
	{ terminate_unexisting_server, "Terminate unexisting server" },
	{ NULL, NULL }
};

int main(int argc, char *argv[])
{
	struct Test *test;
	int errors = 0;

	test = &tests_suite[0];
	while (test->run_test != NULL) {
		int ret = test->run_test();
		if (ret == 0) {
			printf("PASS: ");
		} else {
			errors++;
			printf("FAIL: ");
		}
		printf("%s\n", test->name);
		test++;
	}

	return errors;
}
