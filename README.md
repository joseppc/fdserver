fdserver
========

fdserver is used by programs to register file descriptors so they can
be shared with other processes in the system. The file descriptors are
sent to fdserver through ancilliary data to a UNIX domain socket, and 
can be requested in a similar fashion. See fdserver.h for the list of
functions available to applications, and the examples directory.

The initial implementation of fdserver is found in [ODP's code],
see [odp\_fdserver.c].

[ODP's code]: http://www.opendataplane.org/
[odp\_fdserver.c]: https://github.com/Linaro/odp/blob/tigermoth_lts/platform/linux-generic/odp_fdserver.c

Building
========

```
autoreconf --install
./configure
make
```

Optionally `make check`
