#include <stdlib.h>

#include <doca_argp.h>
#include <doca_log.h>

#include "doca_error.h"
#include "doca_rdma.h"
#include "rdma_common_doca.h"
#include "common_doca.h"

DOCA_LOG_REGISTER(HOST_EXPORT_MAIN::MAIN);

struct host_resources
               {
    struct rdma_config *cfg;
    struct doca_dev *doca_device;  /* DOCA device */
    struct doca_pe *pe;            /* DOCA progress engine */
    struct doca_mmap *mmap;        /* DOCA memory map */

};

doca_error_t allocate_dma_copy_resources(struct host_resources *resources, struct rdma_config *cfg)
{
    doca_error_t result;

    resources->cfg = cfg;
    /* Open DOCA device */
    result = open_doca_device_with_ibdev_str(cfg->device_name, doca_rdma_cap_task_receive_is_supported, &(resources->doca_device));
    if (result != DOCA_SUCCESS)
    {
        DOCA_LOG_ERR("Failed to open DOCA device: %s", doca_error_get_descr(result));
        return result;
    }





	return result;
}
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

    /* Start sample */
    result = rdma_multi_conn_receive(&cfg);
    if (result != DOCA_SUCCESS)
    {
        DOCA_LOG_ERR("rdma_multi_conn_receive() failed: %s", doca_error_get_descr(result));
        goto argp_cleanup;
    }

    exit_status = EXIT_SUCCESS;

argp_cleanup:
    doca_argp_destroy();
sample_exit:
    if (exit_status == EXIT_SUCCESS)
        DOCA_LOG_INFO("Sample finished successfully");
    else
        DOCA_LOG_INFO("Sample finished with errors");
    return exit_status;
}
