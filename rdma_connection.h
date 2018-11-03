#ifndef RDMA_DEMO_RDMA_CONNECTION_H
#define RDMA_DEMO_RDMA_CONNECTION_H


#include "rdma_identifier.h"

class rdma_identifier;

class rdma_context;

class rdma_connection {
public:
    rdma_identifier *local;
    rdma_identifier *remote;

    int listen_fd;
    int socket_fd;
    bool is_server;

    explicit rdma_connection(rdma_context &context);

    ~rdma_connection();

    void listen(int port);

    void connect(char *server_name, int port);

    int exchange();

    void print();
};


#endif //RDMA_DEMO_RDMA_CONNECTION_H
