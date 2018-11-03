#include <unistd.h>
#include <cstdio>
#include "rdma_context.h"
#include "rdma_connection.h"
#include "check.h"

int main(int argc, char *argv[]) {
    int ib_port = 1;
    int port = 12345;
    int size = 4096 * 10;
    int page_size = static_cast<int>(sysconf(_SC_PAGESIZE));
    int rx_depth = 500;

    srand48(getpid() * time(nullptr));
    rdma_context context(ib_port, size, page_size, rx_depth);


    rdma_connection connection = rdma_connection(context);

    if (argc == 2) { //client
        connection.connect(argv[1], port);
    } else { //server
        connection.listen(port);
    }

    context.prepare();

    CHECK_NZ(connection.exchange(),
             "Could not exchange connection");

    connection.print();

    CHECK_NZ(context.connect(connection),
             "Couldn't connect to remote QP");

    context.benchmark();

    context.print_statistics();
}

