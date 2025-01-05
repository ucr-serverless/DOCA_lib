/*
 * Copyright (c) 2024 NVIDIA CORPORATION AND AFFILIATES.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright notice, this list of
 *       conditions and the following disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *     * Neither the name of the NVIDIA CORPORATION nor the names of its contributors may be used
 *       to endorse or promote products derived from this software without specific prior written
 *       permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NVIDIA CORPORATION BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TOR (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stdlib.h>

#include <doca_argp.h>
#include <doca_log.h>

#include "common_doca.h"
#include "log.h"
#include "rdma_common_doca.h"
#include "sock_utils.h"

#define DEFAULT_MMAP_EXPT_LISTEN_ADDR "0.0.0.0"
#define DEFAULT_MMAP_EXPT_LISTEN_PORT "10005"
DOCA_LOG_REGISTER(RDMA_EXPORT_HOST_RECEIVE::MAIN);

/* Sample's Logic */
doca_error_t rdma_multi_conn_receive(struct rdma_config *cfg);

/*
 * Sample main function
 *
 * @argc [in]: command line arguments size
 * @argv [in]: array of command line arguments
 * @return: EXIT_SUCCESS on success and EXIT_FAILURE otherwise
 */
int main(int argc, char **argv)
{
    struct rdma_config cfg;
    doca_error_t result;
    struct doca_log_backend *sdk_log;
    int exit_status = EXIT_FAILURE;

    /* Set the default configuration values (Example values) */
    result = set_default_config_value(&cfg);
    if (result != DOCA_SUCCESS)
        goto sample_exit;

    /* No need for send_string in the receiver side */
    cfg.send_string[0] = '\0';

    /* Register a logger backend */
    result = doca_log_backend_create_standard();
    if (result != DOCA_SUCCESS)
        goto sample_exit;

    /* Register a logger backend for internal SDK errors and warnings */
    result = doca_log_backend_create_with_file_sdk(stderr, &sdk_log);
    if (result != DOCA_SUCCESS)
        goto sample_exit;
    result = doca_log_backend_set_sdk_level(sdk_log, DOCA_LOG_LEVEL_WARNING);
    if (result != DOCA_SUCCESS)
        goto sample_exit;

    DOCA_LOG_INFO("Starting the sample");

    /* Initialize argparser */
    result = doca_argp_init("doca_rdma_multi_conn_receive", &cfg);
    if (result != DOCA_SUCCESS)
    {
        DOCA_LOG_ERR("Failed to init ARGP resources: %s", doca_error_get_descr(result));
        goto sample_exit;
    }

    /* Register RDMA common params */
    result = register_rdma_common_params();
    if (result != DOCA_SUCCESS)
    {
        DOCA_LOG_ERR("Failed to register sample parameters: %s", doca_error_get_descr(result));
        goto argp_cleanup;
    }

    /* Register RDMA num_connections param */
    result = register_rdma_num_connections_param();
    if (result != DOCA_SUCCESS)
    {
        DOCA_LOG_ERR("Failed to register num_connections parameter: %s", doca_error_get_descr(result));
        goto argp_cleanup;
    }

    /* Start argparser */
    result = doca_argp_start(argc, argv);
    if (result != DOCA_SUCCESS)
    {
        DOCA_LOG_ERR("Failed to parse sample input: %s", doca_error_get_descr(result));
        goto argp_cleanup;
    }

    char port[MAX_PORT_LEN];

    int_to_port_str(cfg.sock_port, port, MAX_PORT_LEN);

    int fd = sock_create_bind(DEFAULT_MMAP_EXPT_LISTEN_ADDR, DEFAULT_MMAP_EXPT_LISTEN_PORT);
    if (fd < 0)
    {
        log_error("sock fd fail");
        goto server_sock_error;
    }
    log_info("start listen from host");
    struct sockaddr_in peer_addr;
    socklen_t peer_addr_len = sizeof(struct sockaddr_in);
    int ret = listen(fd, 5);
    JUMP_ON_FAILURE_CONDITION((ret < 0), server_sock_error, "listen error");

    cfg.sock_fd = accept(fd, (struct sockaddr *)&peer_addr, &peer_addr_len);
    JUMP_ON_FAILURE_CONDITION((cfg.sock_fd < 0), server_sock_error, "accept error");

    log_info("received connection: %d", cfg.sock_fd);

    cfg.host_descriptor = malloc(MAX_RDMA_DESCRIPTOR_SZ);
    JUMP_ON_FAILURE_CONDITION((cfg.host_descriptor == NULL), host_descriptor_free,
                              "allocate mem for host descriptor faile");

    result = sock_recv_buffer(cfg.host_descriptor, &cfg.host_descriptor_size, MAX_RDMA_DESCRIPTOR_SZ, cfg.sock_fd);
    JUMP_ON_DOCA_ERROR(result, client_sock_error);

    print_buffer_hex(cfg.host_descriptor, cfg.host_descriptor_size);

    /* Start sample */
    result = rdma_multi_conn_receive(&cfg);
    if (result != DOCA_SUCCESS)
    {
        DOCA_LOG_ERR("rdma_multi_conn_receive() failed: %s", doca_error_get_descr(result));
        goto argp_cleanup;
    }

    exit_status = EXIT_SUCCESS;
host_descriptor_free:
    if (cfg.host_descriptor)
    {
        free(cfg.host_descriptor);
    }
client_sock_error:
    close(cfg.sock_fd);
server_sock_error:
    close(fd);
argp_cleanup:
    doca_argp_destroy();
sample_exit:
    if (exit_status == EXIT_SUCCESS)
        DOCA_LOG_INFO("Sample finished successfully");
    else
        DOCA_LOG_INFO("Sample finished with errors");
    return exit_status;
}
