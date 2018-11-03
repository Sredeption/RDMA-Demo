#ifndef RDMA_DEMO_RDMA_IDENTIFIER_H
#define RDMA_DEMO_RDMA_IDENTIFIER_H


#include <cstdint>
#include <infiniband/verbs.h>
#include "rdma_context.h"

class rdma_context;

class rdma_identifier {
public:
    uint16_t lid;
    uint32_t qpn;
    uint32_t psn;
    unsigned rkey;
    unsigned long long vaddr;
    int gid_index;
    ibv_gid gid;

    rdma_identifier();

    explicit rdma_identifier(rdma_context &context);

    void print(char *name);
};


#endif //RDMA_DEMO_RDMA_IDENTIFIER_H
