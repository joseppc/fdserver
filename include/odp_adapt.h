#ifndef ODP_ADAPT_H
#define ODP_ADAPT_H

#include <stdio.h>

#define ODP_ERR(...) \
	fprintf(stderr, __VA_ARGS__)

#define ODP_DBG(...) \
	fprintf(stderr, __VA_ARGS__)

extern const char * const fdserver_path;

#endif
