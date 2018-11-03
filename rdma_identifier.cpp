#include <stdlib.h>
#include <cstdio>
#include <infiniband/verbs.h>
#include "rdma_identifier.h"

rdma_identifier::rdma_identifier() = default;

rdma_identifier::rdma_identifier(rdma_context &context) {
    lid = context.port_attr.lid;
    qpn = context.qp->qp_num;
    psn = static_cast<uint32_t>(lrand48() & 0xffffff);
    rkey = context.mr->rkey;

    memset(&gid, 0, sizeof(ibv_gid));
    if (context.port_attr.link_layer == IBV_LINK_LAYER_ETHERNET) {
        gid_index = 0;
        context.query_gid(gid_index, &gid);
    } else {
        gid_index = -1;
    }

}

void rdma_identifier::print(char *name) {
    printf("%s: LID %#04x, QPN %#06x, PSN %#06x RKey %#08x  Subnet %#016lx Interface %016lx\n",
           name, lid, qpn, psn, rkey, gid.global.subnet_prefix, gid.global.interface_id);
}
