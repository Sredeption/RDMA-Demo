#include "rdma_connection.h"

#include "check.h"
#include <netdb.h>
#include <stdio.h>

rdma_connection::rdma_connection(rdma_context &context) :
        local(new rdma_identifier(context)), remote(nullptr) {
}

rdma_connection::~rdma_connection() {
    delete local;
    delete remote;
}


void rdma_connection::listen(int port) {
    addrinfo *res;
    addrinfo hints = {
            .ai_flags       = AI_PASSIVE,
            .ai_family      = AF_UNSPEC,
            .ai_socktype    = SOCK_STREAM
    };

    is_server = true;

    char *service;
    int listen_fd;
    int n, conn_fd;

    CHECK_N(asprintf(&service, "%d", port),
            "Error writing port-number to port-string");

    CHECK_N(n = getaddrinfo(nullptr, service, &hints, &res),
            "getaddrinfo threw error");

    CHECK_N(listen_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol),
            "Could not create server socket");

    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &n, sizeof n);

    CHECK_N(bind(listen_fd, res->ai_addr, res->ai_addrlen),
            "Could not bind addr to socket");

    ::listen(listen_fd, 1);

    CHECK_N(conn_fd = accept(listen_fd, nullptr, 0),
            "server accept failed");

    freeaddrinfo(res);

    socket_fd = conn_fd;
}

void rdma_connection::connect(char *server_name, int port) {
    struct addrinfo *res, *t;
    struct addrinfo hints{};

    is_server = false;

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    char *service;
    int conn_fd = -1;

    CHECK_N(asprintf(&service, "%d", port),
            "Error writing port-number to port-string");

    CHECK_N(getaddrinfo(server_name, service, &hints, &res),
            "getaddrinfo threw error");

    for (t = res; t; t = t->ai_next) {
        CHECK_N(conn_fd = socket(t->ai_family, t->ai_socktype, t->ai_protocol),
                "Could not create client socket");

        CHECK_N(::connect(conn_fd, t->ai_addr, t->ai_addrlen),
                "Could not connect to server");

        printf("connect successfully\n");
    }

    freeaddrinfo(res);

    socket_fd = conn_fd;
}

int rdma_connection::exchange() {
    size_t identifier_size = sizeof(rdma_identifier);

    if (write(socket_fd, local, identifier_size) != identifier_size) {
        perror("Could not send connection_details to peer");
        return -1;
    }

    remote = new rdma_identifier();

    if (read(socket_fd, remote, identifier_size) != identifier_size) {
        perror("Could not receive connection_details to peer");
        return -1;
    }

    return 0;
}

void rdma_connection::print() {
    local->print(const_cast<char *>("Local "));
    remote->print(const_cast<char *>("Remote"));
}
