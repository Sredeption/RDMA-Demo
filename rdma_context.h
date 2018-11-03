#ifndef RDMA_DEMO_RDMA_CONTEXT_H
#define RDMA_DEMO_RDMA_CONTEXT_H

#include <infiniband/verbs.h>

enum {
    RDMA_RECV_WRID = 1,
    RDMA_SEND_WRID = 2,
};

class rdma_connection;

class rdma_context {
public:
    ibv_device *device;
    ibv_context *context;
    ibv_comp_channel *channel;
    ibv_pd *pd;
    ibv_mr *mr;
    ibv_cq *cq;
    ibv_qp *qp;
    ibv_port_attr port_attr;

    rdma_connection *connection;
    char *buf;
    uint8_t ib_port;
    int size;
    unsigned int send_flags;
    int rx_depth;
    int pending;

    int iters;
    int scnt;
    int rcnt;
    int routs;
    timeval start, end;

    rdma_context(int ib_port, int size, int page_size, int rx_depth);

    ~rdma_context();

    void query_gid(int gid_index, ibv_gid *gid);

    int post_recv(int n);

    int post_send();

    int connect(rdma_connection &connection);


    void prepare();

    void benchmark();

    int process_wc(uint64_t wr_id, ibv_wc_status status);

    void print_statistics();
};


#endif //RDMA_DEMO_RDMA_CONTEXT_H
