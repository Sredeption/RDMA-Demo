#include <malloc.h>
#include <stdexcept>
#include <sys/time.h>
#include "rdma_context.h"
#include "rdma_connection.h"
#include "check.h"

rdma_context::rdma_context(int ib_port, int size, int page_size, int rx_depth) :
        ib_port(static_cast<uint8_t>(ib_port)), size(size), rx_depth(rx_depth), send_flags(IBV_SEND_SIGNALED),
        pending(0), iters(1000), scnt(0), rcnt(0) {
    int access_flags = IBV_ACCESS_LOCAL_WRITE;

    CHECK_Z(buf = static_cast<char *>(memalign(page_size, size)), "Couldn't allocate work buf.");
    memset(buf, 0x7b, size);

    ibv_device **device_list;

    CHECK_Z(device_list = ibv_get_device_list(nullptr),
            "No IB-device available. get_device_list returned NULL");

    CHECK_Z(device = device_list[0],
            "IB-device could not be assigned. Maybe device_list array is empty");

    CHECK_Z(context = ibv_open_device(device),
            "Could not create context, ibv_open_device");

    CHECK_Z(pd = ibv_alloc_pd(context),
            "Could not allocate protection domain, ibv_alloc_pd");

    CHECK_Z(mr = ibv_reg_mr(pd, buf, size, access_flags),
            "Could not allocate mr, ibv_reg_mr. Do you have root access?");

    channel = nullptr;
    CHECK_Z(cq = ibv_create_cq(context, rx_depth + 1, nullptr, channel, 0),
            "Could not create send completion queue, ibv_create_cq");

    CHECK_NZ(ibv_query_port(context, this->ib_port, &port_attr),
             "Couldn't get port info");

    {
        ibv_qp_attr attr{};
        ibv_qp_init_attr init_attr{};
        init_attr.send_cq = cq,
        init_attr.recv_cq = cq,
        init_attr.cap = {
                .max_send_wr    = 1,
                .max_recv_wr    = static_cast<uint32_t>(rx_depth),
                .max_send_sge   = 1,
                .max_recv_sge   = 1,
        },
        init_attr.qp_type = IBV_QPT_RC;

        CHECK_Z(qp = ibv_create_qp(pd, &init_attr),
                "Could not create queue pair, ibv_create_qp");

        ibv_query_qp(qp, &attr, IBV_QP_CAP, &init_attr);
        if (init_attr.cap.max_inline_data >= size) {
            send_flags |= IBV_SEND_INLINE;
        }
    }

    {
        ibv_qp_attr attr{};
        attr.qp_state = IBV_QPS_INIT,
        attr.pkey_index = 0,
        attr.port_num = static_cast<uint8_t>(ib_port),
        attr.qp_access_flags = 0;

        CHECK_NZ(ibv_modify_qp(qp, &attr,
                               IBV_QP_STATE |
                               IBV_QP_PKEY_INDEX |
                               IBV_QP_PORT |
                               IBV_QP_ACCESS_FLAGS),
                 "Could not modify QP to INIT, ibv_modify_qp");
    }
}

rdma_context::~rdma_context() {
    CHECK_NZ(ibv_destroy_qp(qp),
             "Could not destroy queue pair, ibv_destroy_qp");

    CHECK_NZ(ibv_destroy_cq(cq),
             "Could not destroy send completion queue, ibv_destroy_cq");

    if (channel != nullptr) {
        CHECK_NZ(ibv_destroy_comp_channel(channel),
                 "Could not destroy completion channel, ibv_destroy_comp_channel");
    }

    CHECK_NZ(ibv_dereg_mr(mr),
             "Could not de-register memory region, ibv_dereg_mr");

    CHECK_NZ(ibv_dealloc_pd(pd),
             "Could not deallocate protection domain, ibv_dealloc_pd");

    free(buf);
}

int rdma_context::post_recv(int n) {
    struct ibv_sge list = {
            .addr = reinterpret_cast<uint64_t>(buf),
            .length = static_cast<uint32_t>(size),
            .lkey = mr->lkey
    };

    struct ibv_recv_wr wr = {
            .wr_id      = RDMA_RECV_WRID,
            .next       = nullptr,
            .sg_list    = &list,
            .num_sge    = 1,
    };

    struct ibv_recv_wr *bad_wr;
    int i;

    for (i = 0; i < n; ++i)
        if (ibv_post_recv(qp, &wr, &bad_wr))
            break;

    return i;
}

int rdma_context::post_send() {
    struct ibv_sge list = {
            .addr    = reinterpret_cast<uint64_t>(buf),
            .length = static_cast<uint32_t>(size),
            .lkey    = mr->lkey
    };
    struct ibv_send_wr wr{};
    wr.wr_id = RDMA_SEND_WRID;
    wr.sg_list = &list;
    wr.num_sge = 1;
    wr.opcode = IBV_WR_SEND;
    wr.send_flags = send_flags;

    struct ibv_send_wr *bad_wr;

    return ibv_post_send(qp, &wr, &bad_wr);
}

void rdma_context::query_gid(int gid_index, ibv_gid *gid) {
    CHECK_NZ(ibv_query_gid(context, ib_port, gid_index, gid),
             "Could not get gid index, ibv_query_gid");
}

int rdma_context::connect(rdma_connection &connection) {
    this->connection = &connection;
    rdma_identifier *local = connection.local;
    rdma_identifier *remote = connection.remote;
    ibv_qp_attr attr{};
    attr.qp_state = IBV_QPS_RTR,
    attr.path_mtu = IBV_MTU_1024,
    attr.dest_qp_num = remote->qpn,
    attr.rq_psn = remote->psn,
    attr.max_dest_rd_atomic = 1,
    attr.min_rnr_timer = 12,

    attr.ah_attr.is_global = 0,
    attr.ah_attr.dlid = remote->lid,
    attr.ah_attr.sl = 0,
    attr.ah_attr.src_path_bits = 0,
    attr.ah_attr.port_num = ib_port;

    if (remote->gid.global.interface_id) {
        attr.ah_attr.is_global = 1;
        attr.ah_attr.grh.hop_limit = 1;
        attr.ah_attr.grh.dgid = remote->gid;
        attr.ah_attr.grh.sgid_index = static_cast<uint8_t>(local->gid_index);
    }

    if (ibv_modify_qp(qp, &attr,
                      IBV_QP_STATE |
                      IBV_QP_AV |
                      IBV_QP_PATH_MTU |
                      IBV_QP_DEST_QPN |
                      IBV_QP_RQ_PSN |
                      IBV_QP_MAX_DEST_RD_ATOMIC |
                      IBV_QP_MIN_RNR_TIMER)) {
        fprintf(stderr, "Failed to modify QP to RTR\n");
        return 1;
    }

    attr.qp_state = IBV_QPS_RTS;
    attr.timeout = 14;
    attr.retry_cnt = 7;
    attr.rnr_retry = 7;
    attr.sq_psn = local->psn;
    attr.max_rd_atomic = 1;
    if (ibv_modify_qp(qp, &attr,
                      IBV_QP_STATE |
                      IBV_QP_TIMEOUT |
                      IBV_QP_RETRY_CNT |
                      IBV_QP_RNR_RETRY |
                      IBV_QP_SQ_PSN |
                      IBV_QP_MAX_QP_RD_ATOMIC)) {
        fprintf(stderr, "Failed to modify QP to RTS\n");
        return 1;
    }

    return 0;
}

void rdma_context::prepare() {
    pending = RDMA_RECV_WRID;

    routs = post_recv(rx_depth);
    if (routs < rx_depth) {
        fprintf(stderr, "Couldn't post receive (%d)\n", routs);
        return;
    }
}

void rdma_context::benchmark() {
    if (gettimeofday(&start, nullptr)) {
        perror("get time of day");
        return;
    }

    if (!connection->is_server) { //client
        pending |= RDMA_SEND_WRID;
        if (post_send()) {
            fprintf(stderr, "Couldn't post send\n");
            return;
        }
    }

    int ne, i;
    ibv_wc wc[2];

    while (rcnt < iters || scnt < iters) {
        do {
            CHECK_N(ne = ibv_poll_cq(cq, 2, wc),
                    "poll CQ failed");
        } while (ne < 1);

        for (i = 0; i < ne; ++i) {
            if (process_wc(wc[i].wr_id, wc[i].status)) {
                fprintf(stderr, "process WC failed %d\n", ne);
                return;
            }
        }
    }

    if (gettimeofday(&end, nullptr)) {
        perror("get time of day");
        return;
    }
}

int rdma_context::process_wc(uint64_t wr_id, ibv_wc_status status) {
    if (status != IBV_WC_SUCCESS) {
        fprintf(stderr, "Failed status %s (%d) for wr_id %lu\n",
                ibv_wc_status_str(status), status, wr_id);
        return 1;
    }


    switch (wr_id) {
        case RDMA_SEND_WRID:
            ++scnt;
            break;
        case RDMA_RECV_WRID:
            if (--routs <= 1) {
                routs += post_recv(rx_depth - routs);
                if (routs < rx_depth) {
                    fprintf(stderr, "Couldn't post receive (%d)\n", routs);
                    return 1;
                }
            }
            ++rcnt;
            break;
        default:
            fprintf(stderr, "Completion for unknown wr_id %lu\n",
                    wr_id);
            return 1;
    }

    pending &= ~static_cast<int>(wr_id);
    if (scnt < iters && !pending) {
        if (post_send()) {
            fprintf(stderr, "Couldn't post send\n");
            return 1;
        }
        pending = RDMA_RECV_WRID | RDMA_SEND_WRID;
    }
    return 0;
}

void rdma_context::print_statistics() {
    float usec = (end.tv_sec - start.tv_sec) * 1000000 +
                 (end.tv_usec - start.tv_usec);
    long long bytes = (long long) size * iters * 2;

    printf("%lld bytes in %.2f seconds = %.2f Mbit/sec\n",
           bytes, usec / 1000000., bytes * 8. / usec);
    printf("%d iters in %.2f seconds = %.2f usec/iter\n",
           iters, usec / 1000000., usec / iters);
}
