cmake_minimum_required(VERSION 3.5)
project(RDMA_Demo)

set(CMAKE_CXX_STANDARD 14)

add_executable(rc_send rc_send.cpp rdma_context.cpp rdma_context.h check.h rdma_identifier.cpp rdma_identifier.h rdma_connection.cpp rdma_connection.h)

target_link_libraries(rc_send ibverbs)