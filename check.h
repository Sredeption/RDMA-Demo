#ifndef RDMA_DEMO_CHECK_H
#define RDMA_DEMO_CHECK_H

// if x is NON-ZERO, error is printed
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <cstdio>
#include <execinfo.h>
#include <unistd.h>

#define CHECK_NZ(x, y) do { if ((x)) die(y); } while (0)

// if x is ZERO, error is printed
#define CHECK_Z(x, y) do { if (!(x)) die(y); } while (0)

// if x is NEGATIVE, error is printed
#define CHECK_N(x, y) do { if ((x)<0) die(y); } while (0)

static int die(const char *reason) {
    void *array[10];
    int size;
    size = backtrace(array, 10);
    fprintf(stderr, "Err: %s - %s\n ", strerror(errno), reason);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    exit(EXIT_FAILURE);
}

#endif //RDMA_DEMO_CHECK_H
