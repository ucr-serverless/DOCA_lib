/*
 * Copyright (c) 2022-2023 NVIDIA CORPORATION AND AFFILIATES.  All rights reserved.
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

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "sock_utils.h"
#include <doca_buf.h>
#include <doca_buf_inventory.h>
#include <doca_ctx.h>
#include <doca_dev.h>
#include <doca_dma.h>
#include <doca_error.h>
#include <doca_log.h>
#include <doca_mmap.h>
#include <doca_pe.h>
#include <doca_rdma.h>
#include <errno.h>
#include <sys/epoll.h>

#include "common_doca.h"

DOCA_LOG_REGISTER(COMMON);

void wait_for_enter(void)
{
    int enter = 0;

    /* Wait for enter */
    while (enter != '\r' && enter != '\n')
        enter = getchar();
}
void check_dev_cap(const struct doca_devinfo *devinfo)
{
    doca_error_t result;

    uint8_t ret;
    result = doca_mmap_cap_is_create_from_export_pci_supported(devinfo, &ret);
    DOCA_LOG_INFO("start check");
    if (result != DOCA_SUCCESS)
    {
        DOCA_LOG_ERR("mmap query fail");
    }
    if (ret == 1)
    {
        DOCA_LOG_INFO("device support create mmap");
    }
    result = doca_rdma_cap_task_receive_is_supported(devinfo);
    if (result != DOCA_SUCCESS)
    {
        DOCA_LOG_ERR("rdma_receive not supportted");
    }
    else
    {
        DOCA_LOG_INFO("rdma receive supportted");
    }
    result = doca_rdma_cap_task_send_is_supported(devinfo);
    if (result != DOCA_SUCCESS)
    {
        DOCA_LOG_ERR("rdma send not supportted");
    }
    else
    {
        DOCA_LOG_INFO("rdma send supportted");
    }
    result = doca_dma_cap_task_memcpy_is_supported(devinfo);
    if (result != DOCA_SUCCESS)
    {
        DOCA_LOG_ERR("dma memcpy is not supportted");
    }
    else
    {
        DOCA_LOG_ERR("dma memcpy supportted");
    }
    uint8_t ip_addr[DOCA_DEVINFO_IPV4_ADDR_SIZE] = {0};
    result = doca_devinfo_get_ipv4_addr(devinfo, ip_addr, DOCA_DEVINFO_IPV4_ADDR_SIZE);
    if (result != DOCA_SUCCESS)
    {
        DOCA_LOG_ERR("ipv4 addr is not found");
    }
    else
    {
        DOCA_LOG_INFO("IPv4 Address: %u.%u.%u.%u\n", ip_addr[0], ip_addr[1], ip_addr[2], ip_addr[3]);
    }

    uint8_t mac_addr[DOCA_DEVINFO_MAC_ADDR_SIZE];
    result = doca_devinfo_get_mac_addr(devinfo, mac_addr, DOCA_DEVINFO_MAC_ADDR_SIZE);
    if (result != DOCA_SUCCESS)
    {
        DOCA_LOG_ERR("mac addr is not found");
    }
    else
    {
        DOCA_LOG_INFO("MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n", mac_addr[0], mac_addr[1], mac_addr[2],
                      mac_addr[3], mac_addr[4], mac_addr[5]);
    }
    DOCA_LOG_INFO("end check");
}
/*
 * Allocate memory and populate it into the memory map
 *
 * @mmap [in]: DOCA memory map
 * @buffer_len [in]: Allocated buffer length
 * @access_flags [in]: The access permissions of the mmap
 * @buffer [out]: Allocated buffer
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
doca_error_t create_doca_mmap_from_buf(struct doca_mmap **mmap, size_t buffer_len, uint32_t access_flags,
                                       struct doca_dev *dev, char **buffer)
{
    doca_error_t result;

    result = doca_mmap_create(mmap);
    if (result != DOCA_SUCCESS)
    {
        DOCA_LOG_ERR("Failed to create mmap for source buffer, error: %s", doca_error_get_descr(result));
        return result;
    }
    result = doca_mmap_set_permissions(*mmap, access_flags);
    if (result != DOCA_SUCCESS)
    {
        DOCA_LOG_ERR("Unable to set access permissions of memory map: %s", doca_error_get_descr(result));
        goto destroy_mmap;
    }
    *buffer = (char *)malloc(buffer_len);
    if (*buffer == NULL)
    {
        DOCA_LOG_ERR("Failed to allocate memory for source buffer");
        goto free_buf;
    }
    DOCA_LOG_INFO("The raw buffer address is %p", buffer);

    result = doca_mmap_set_memrange(*mmap, *buffer, buffer_len);
    if (result != DOCA_SUCCESS)
    {
        DOCA_LOG_ERR("Unable to set memrange of memory map: %s", doca_error_get_descr(result));
        goto free_buf;
        return result;
    }
    result = doca_mmap_add_dev(*mmap, dev);
    if (result != DOCA_SUCCESS)
    {
        DOCA_LOG_ERR("Failed to add device to mmap, error: %s", doca_error_get_descr(result));
        goto free_buf;
    }

    /* Populate local buffer into memory map to allow access from DPU side after exporting */
    result = doca_mmap_start(*mmap);
    if (result != DOCA_SUCCESS)
    {
        DOCA_LOG_ERR("Unable to populate memory map: %s", doca_error_get_descr(result));
        goto free_buf;
    }

    return result;
    doca_error_t tmp_result = result;

free_buf:
    if (*buffer)
    {
        free(*buffer);
    }
destroy_mmap:
    tmp_result = doca_mmap_destroy(*mmap);
    if (tmp_result != DOCA_SUCCESS)
    {
        DOCA_ERROR_PROPAGATE(result, tmp_result);
        DOCA_LOG_ERR("Failed to destroy remote DOCA mmap: %s", doca_error_get_descr(tmp_result));
    }
    return tmp_result;
}

void print_buffer_hex(const void *buffer, size_t length)
{
    const unsigned char *byte_buffer = (const unsigned char *)buffer; // Cast to byte array

    printf("Buffer content (%zu bytes):\n", length);
    for (size_t i = 0; i < length; i++)
    {
        printf("%02x ", byte_buffer[i]); // Print each byte in hex format
        if ((i + 1) % 16 == 0)
        {
            printf("\n"); // New line after every 16 bytes
        }
    }
    printf("\n\n");
}

doca_error_t sock_send_buffer(const void *rdma_conn_descriptor, size_t descriptor_size, int sock_fd)
{
    if (sock_write(sock_fd, &descriptor_size, sizeof(uint32_t)) != sizeof(uint32_t))
    {
        log_error("Error, send descriptor size\n");
        goto error;
    }
    ssize_t write_len = sock_write(sock_fd, rdma_conn_descriptor, descriptor_size);
    log_info("read: %u, descriptor_size: %u", write_len, descriptor_size);
    if (write_len < 0)
    {
        goto error;
    }
    if (write_len != descriptor_size)
    {
        log_error("Error, send descriptor\n");
        goto error;
    }

    return DOCA_SUCCESS;

error:
    log_error("Error, send descriptor");
    return DOCA_ERROR_IO_FAILED;
}

doca_error_t sock_recv_buffer(void *rdma_conn_descriptor, size_t *descriptor_size, size_t descriptor_buf_size,
                              int sock_fd)
{

    if (sock_read(sock_fd, descriptor_size, sizeof(uint32_t)) != sizeof(uint32_t))
    {
        log_error("Error, recv descriptor size\n");
        goto error;
    }
    log_info("receive buffer %d incoming data %d", descriptor_buf_size, *descriptor_size);
    if (descriptor_buf_size < *descriptor_size)
    {
        log_fatal("receive buffer %d is smaller then the incoming data %d", descriptor_buf_size, *descriptor_size);
        goto error;
    }
    ssize_t read_len = sock_read(sock_fd, rdma_conn_descriptor, *descriptor_size);
    if (read_len < 0)
    {
        goto error;
    }
    if (read_len != *descriptor_size)
    {
        log_error("Error, recv descriptor\n");
        goto error;
    }
    return DOCA_SUCCESS;

error:
    log_error("Error, recv descriptor");
    return DOCA_ERROR_IO_FAILED;
}

doca_error_t open_doca_device_with_pci(const char *pci_addr, tasks_check func, struct doca_dev **retval)
{
    struct doca_devinfo **dev_list;
    uint32_t nb_devs;
    uint8_t is_addr_equal = 0;
    int res;
    size_t i;

    /* Set default return value */
    *retval = NULL;

    res = doca_devinfo_create_list(&dev_list, &nb_devs);
    if (res != DOCA_SUCCESS)
    {
        DOCA_LOG_ERR("Failed to load doca devices list: %s", doca_error_get_descr(res));
        return res;
    }

    /* Search */
    for (i = 0; i < nb_devs; i++)
    {
        res = doca_devinfo_is_equal_pci_addr(dev_list[i], pci_addr, &is_addr_equal);
        if (res == DOCA_SUCCESS && is_addr_equal)
        {
            /* If any special capabilities are needed */
            if (func != NULL && func(dev_list[i]) != DOCA_SUCCESS)
                continue;

            /* if device can be opened */
            res = doca_dev_open(dev_list[i], retval);
            if (res == DOCA_SUCCESS)
            {
                doca_devinfo_destroy_list(dev_list);
                return res;
            }
        }
    }

    DOCA_LOG_WARN("Matching device not found");
    res = DOCA_ERROR_NOT_FOUND;

    doca_devinfo_destroy_list(dev_list);
    return res;
}

doca_error_t register_pe_event(struct doca_pe *pe, int ep_fd)
{
    doca_event_handle_t event_handle = doca_event_invalid_handle;
    struct epoll_event events_in = {.events = EPOLLIN, .data.fd = 0};

    DOCA_LOG_INFO("Registering PE event");

    /* doca_event_handle_t is a file descriptor that can be added to an epoll */
    doca_error_t ret = doca_pe_get_notification_handle(pe, &event_handle);
    if (ret != DOCA_SUCCESS)
    {
        DOCA_LOG_ERR("get event handle fail");
    }

    if (epoll_ctl(ep_fd, EPOLL_CTL_ADD, event_handle, &events_in) != 0)
    {
        DOCA_LOG_ERR("Failed to register epoll, error=%d", errno);
        return DOCA_ERROR_OPERATING_SYSTEM;
    }

    return DOCA_SUCCESS;
}

doca_error_t run_for_competion(struct doca_pe *pe, int ep_fd, predicate func, void *func_args)
{
    struct epoll_event ep_event = {0};
    int ret = 0;
    DOCA_LOG_INFO("epoll event loop");
    while (func(func_args))
    {
        EXIT_ON_FAILURE(doca_pe_request_notification(pe));
        ret = epoll_wait(ep_fd, &ep_event, 1, -1);
        if (ret == -1)
        {
            DOCA_LOG_ERR("failed to wait ep event, error = %d", errno);
            return DOCA_ERROR_OPERATING_SYSTEM;
        }
        EXIT_ON_FAILURE(doca_pe_clear_notification(pe, 0));
        while (doca_pe_progress(pe))
        {
        }
    }
    return DOCA_SUCCESS;
}

/*
 * Open DOCA device
 *
 * @device_name [in]: The name of the wanted IB device (could be empty string)
 * @func [in]: Function to check if a given device is capable of executing some task
 * @doca_device [out]: An allocated DOCA device on success and NULL otherwise
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
doca_error_t open_doca_device_with_ibdev_str(const char *device_name, tasks_check func, struct doca_dev **doca_device)
{
    struct doca_devinfo **dev_list;
    uint32_t nb_devs = 0;
    doca_error_t result;
    char ibdev_name[DOCA_DEVINFO_IBDEV_NAME_SIZE] = {0};
    uint32_t i = 0;

    result = doca_devinfo_create_list(&dev_list, &nb_devs);
    if (result != DOCA_SUCCESS)
    {
        DOCA_LOG_ERR("Failed to load DOCA devices list: %s", doca_error_get_descr(result));
        return result;
    }

    /* Search device with same dev name*/
    for (i = 0; i < nb_devs; i++)
    {
        result = doca_devinfo_get_ibdev_name(dev_list[i], ibdev_name, sizeof(ibdev_name));
        if (result != DOCA_SUCCESS ||
            (strlen(device_name) != 0 && strncmp(device_name, ibdev_name, DOCA_DEVINFO_IBDEV_NAME_SIZE) != 0))
            continue;
        /* If any special capabilities are needed */
        if (func != NULL && func(dev_list[i]) != DOCA_SUCCESS)
            continue;
        result = doca_dev_open(dev_list[i], doca_device);
        if (result != DOCA_SUCCESS)
        {
            DOCA_LOG_ERR("Failed to open DOCA device: %s", doca_error_get_descr(result));
            goto out;
        }
        break;
    }

out:
    doca_devinfo_destroy_list(dev_list);

    if (*doca_device == NULL)
    {
        DOCA_LOG_ERR("Couldn't get DOCA device");
        return DOCA_ERROR_NOT_FOUND;
    }

    return result;
}

doca_error_t open_doca_device_with_ibdev_name(const uint8_t *value, size_t val_size, tasks_check func,
                                              struct doca_dev **retval)
{
    struct doca_devinfo **dev_list;
    uint32_t nb_devs;
    char buf[DOCA_DEVINFO_IBDEV_NAME_SIZE] = {};
    char val_copy[DOCA_DEVINFO_IBDEV_NAME_SIZE] = {};
    int res;
    size_t i;

    /* Set default return value */
    *retval = NULL;

    /* Setup */
    if (val_size > DOCA_DEVINFO_IBDEV_NAME_SIZE)
    {
        DOCA_LOG_ERR("Value size too large. Failed to locate device");
        return DOCA_ERROR_INVALID_VALUE;
    }
    memcpy(val_copy, value, val_size);

    res = doca_devinfo_create_list(&dev_list, &nb_devs);
    if (res != DOCA_SUCCESS)
    {
        DOCA_LOG_ERR("Failed to load doca devices list: %s", doca_error_get_descr(res));
        return res;
    }

    /* Search */
    for (i = 0; i < nb_devs; i++)
    {
        res = doca_devinfo_get_ibdev_name(dev_list[i], buf, DOCA_DEVINFO_IBDEV_NAME_SIZE);
        if (res == DOCA_SUCCESS && strncmp(buf, val_copy, val_size) == 0)
        {
            /* If any special capabilities are needed */
            if (func != NULL && func(dev_list[i]) != DOCA_SUCCESS)
                continue;

            /* if device can be opened */
            res = doca_dev_open(dev_list[i], retval);
            if (res == DOCA_SUCCESS)
            {
                doca_devinfo_destroy_list(dev_list);
                return res;
            }
        }
    }

    DOCA_LOG_WARN("Matching device not found");
    res = DOCA_ERROR_NOT_FOUND;

    doca_devinfo_destroy_list(dev_list);
    return res;
}

doca_error_t open_doca_device_with_iface_name(const uint8_t *value, size_t val_size, tasks_check func,
                                              struct doca_dev **retval)
{
    struct doca_devinfo **dev_list;
    uint32_t nb_devs;
    char buf[DOCA_DEVINFO_IFACE_NAME_SIZE] = {};
    char val_copy[DOCA_DEVINFO_IFACE_NAME_SIZE] = {};
    int res;
    size_t i;

    /* Set default return value */
    *retval = NULL;

    /* Setup */
    if (val_size > DOCA_DEVINFO_IFACE_NAME_SIZE)
    {
        DOCA_LOG_ERR("Value size too large. Failed to locate device");
        return DOCA_ERROR_INVALID_VALUE;
    }
    memcpy(val_copy, value, val_size);

    res = doca_devinfo_create_list(&dev_list, &nb_devs);
    if (res != DOCA_SUCCESS)
    {
        DOCA_LOG_ERR("Failed to load doca devices list: %s", doca_error_get_descr(res));
        return res;
    }

    /* Search */
    for (i = 0; i < nb_devs; i++)
    {
        res = doca_devinfo_get_iface_name(dev_list[i], buf, DOCA_DEVINFO_IFACE_NAME_SIZE);
        if (res == DOCA_SUCCESS && strncmp(buf, val_copy, val_size) == 0)
        {
            /* If any special capabilities are needed */
            if (func != NULL && func(dev_list[i]) != DOCA_SUCCESS)
                continue;

            /* if device can be opened */
            res = doca_dev_open(dev_list[i], retval);
            if (res == DOCA_SUCCESS)
            {
                doca_devinfo_destroy_list(dev_list);
                return res;
            }
        }
    }

    DOCA_LOG_WARN("Matching device not found");
    res = DOCA_ERROR_NOT_FOUND;

    doca_devinfo_destroy_list(dev_list);
    return res;
}

doca_error_t open_doca_device_with_capabilities(tasks_check func, struct doca_dev **retval)
{
    struct doca_devinfo **dev_list;
    uint32_t nb_devs;
    doca_error_t result;
    size_t i;

    /* Set default return value */
    *retval = NULL;

    result = doca_devinfo_create_list(&dev_list, &nb_devs);
    if (result != DOCA_SUCCESS)
    {
        DOCA_LOG_ERR("Failed to load doca devices list: %s", doca_error_get_descr(result));
        return result;
    }

    /* Search */
    for (i = 0; i < nb_devs; i++)
    {
        /* If any special capabilities are needed */
        if (func(dev_list[i]) != DOCA_SUCCESS)
            continue;

        /* If device can be opened */
        if (doca_dev_open(dev_list[i], retval) == DOCA_SUCCESS)
        {
            doca_devinfo_destroy_list(dev_list);
            return DOCA_SUCCESS;
        }
    }

    DOCA_LOG_WARN("Matching device not found");
    doca_devinfo_destroy_list(dev_list);
    return DOCA_ERROR_NOT_FOUND;
}

doca_error_t open_doca_device_rep_with_vuid(struct doca_dev *local, enum doca_devinfo_rep_filter filter,
                                            const uint8_t *value, size_t val_size, struct doca_dev_rep **retval)
{
    uint32_t nb_rdevs = 0;
    struct doca_devinfo_rep **rep_dev_list = NULL;
    char val_copy[DOCA_DEVINFO_REP_VUID_SIZE] = {};
    char buf[DOCA_DEVINFO_REP_VUID_SIZE] = {};
    doca_error_t result;
    size_t i;

    /* Set default return value */
    *retval = NULL;

    /* Setup */
    if (val_size > DOCA_DEVINFO_REP_VUID_SIZE)
    {
        DOCA_LOG_ERR("Value size too large. Ignored");
        return DOCA_ERROR_INVALID_VALUE;
    }
    memcpy(val_copy, value, val_size);

    /* Search */
    result = doca_devinfo_rep_create_list(local, filter, &rep_dev_list, &nb_rdevs);
    if (result != DOCA_SUCCESS)
    {
        DOCA_LOG_ERR("Failed to create devinfo representor list. Representor devices are available only on DPU, do not "
                     "run on Host");
        return DOCA_ERROR_INVALID_VALUE;
    }

    for (i = 0; i < nb_rdevs; i++)
    {
        result = doca_devinfo_rep_get_vuid(rep_dev_list[i], buf, DOCA_DEVINFO_REP_VUID_SIZE);
        if (result == DOCA_SUCCESS && strncmp(buf, val_copy, DOCA_DEVINFO_REP_VUID_SIZE) == 0 &&
            doca_dev_rep_open(rep_dev_list[i], retval) == DOCA_SUCCESS)
        {
            doca_devinfo_rep_destroy_list(rep_dev_list);
            return DOCA_SUCCESS;
        }
    }

    DOCA_LOG_WARN("Matching device not found");
    doca_devinfo_rep_destroy_list(rep_dev_list);
    return DOCA_ERROR_NOT_FOUND;
}

doca_error_t open_doca_device_rep_with_pci(struct doca_dev *local, enum doca_devinfo_rep_filter filter,
                                           const char *pci_addr, struct doca_dev_rep **retval)
{
    uint32_t nb_rdevs = 0;
    struct doca_devinfo_rep **rep_dev_list = NULL;
    uint8_t is_addr_equal = 0;
    doca_error_t result;
    size_t i;

    *retval = NULL;

    /* Search */
    result = doca_devinfo_rep_create_list(local, filter, &rep_dev_list, &nb_rdevs);
    if (result != DOCA_SUCCESS)
    {
        DOCA_LOG_ERR("Failed to create devinfo representors list. Representor devices are available only on DPU, do "
                     "not run on Host");
        return DOCA_ERROR_INVALID_VALUE;
    }

    for (i = 0; i < nb_rdevs; i++)
    {
        result = doca_devinfo_rep_is_equal_pci_addr(rep_dev_list[i], pci_addr, &is_addr_equal);
        if (result == DOCA_SUCCESS && is_addr_equal && doca_dev_rep_open(rep_dev_list[i], retval) == DOCA_SUCCESS)
        {
            doca_devinfo_rep_destroy_list(rep_dev_list);
            return DOCA_SUCCESS;
        }
    }

    DOCA_LOG_WARN("Matching device not found");
    doca_devinfo_rep_destroy_list(rep_dev_list);
    return DOCA_ERROR_NOT_FOUND;
}

doca_error_t create_core_objects(struct program_core_objects *state, uint32_t max_bufs)
{
    doca_error_t res;

    res = doca_mmap_create(&state->src_mmap);
    if (res != DOCA_SUCCESS)
    {
        DOCA_LOG_ERR("Unable to create source mmap: %s", doca_error_get_descr(res));
        return res;
    }
    res = doca_mmap_add_dev(state->src_mmap, state->dev);
    if (res != DOCA_SUCCESS)
    {
        DOCA_LOG_ERR("Unable to add device to source mmap: %s", doca_error_get_descr(res));
        goto destroy_src_mmap;
    }

    res = doca_mmap_create(&state->dst_mmap);
    if (res != DOCA_SUCCESS)
    {
        DOCA_LOG_ERR("Unable to create destination mmap: %s", doca_error_get_descr(res));
        goto destroy_src_mmap;
    }
    res = doca_mmap_add_dev(state->dst_mmap, state->dev);
    if (res != DOCA_SUCCESS)
    {
        DOCA_LOG_ERR("Unable to add device to destination mmap: %s", doca_error_get_descr(res));
        goto destroy_dst_mmap;
    }

    if (max_bufs != 0)
    {
        res = doca_buf_inventory_create(max_bufs, &state->buf_inv);
        if (res != DOCA_SUCCESS)
        {
            DOCA_LOG_ERR("Unable to create buffer inventory: %s", doca_error_get_descr(res));
            goto destroy_dst_mmap;
        }

        res = doca_buf_inventory_start(state->buf_inv);
        if (res != DOCA_SUCCESS)
        {
            DOCA_LOG_ERR("Unable to start buffer inventory: %s", doca_error_get_descr(res));
            goto destroy_buf_inv;
        }
    }

    res = doca_pe_create(&state->pe);
    if (res != DOCA_SUCCESS)
    {
        DOCA_LOG_ERR("Unable to create progress engine: %s", doca_error_get_descr(res));
        goto destroy_buf_inv;
    }

    return DOCA_SUCCESS;

destroy_buf_inv:
    if (state->buf_inv != NULL)
    {
        doca_buf_inventory_destroy(state->buf_inv);
        state->buf_inv = NULL;
    }

destroy_dst_mmap:
    doca_mmap_destroy(state->dst_mmap);
    state->dst_mmap = NULL;

destroy_src_mmap:
    doca_mmap_destroy(state->src_mmap);
    state->src_mmap = NULL;

    return res;
}

doca_error_t request_stop_ctx(struct doca_pe *pe, struct doca_ctx *ctx)
{
    doca_error_t tmp_result, result = DOCA_SUCCESS;

    tmp_result = doca_ctx_stop(ctx);
    if (tmp_result == DOCA_ERROR_IN_PROGRESS)
    {
        enum doca_ctx_states ctx_state;

        do
        {
            (void)doca_pe_progress(pe);
            tmp_result = doca_ctx_get_state(ctx, &ctx_state);
            if (tmp_result != DOCA_SUCCESS)
            {
                DOCA_ERROR_PROPAGATE(result, tmp_result);
                DOCA_LOG_ERR("Failed to get state from ctx: %s", doca_error_get_descr(tmp_result));
                break;
            }
        } while (ctx_state != DOCA_CTX_STATE_IDLE);
    }
    else if (tmp_result != DOCA_SUCCESS)
    {
        DOCA_ERROR_PROPAGATE(result, tmp_result);
        DOCA_LOG_ERR("Failed to stop ctx: %s", doca_error_get_descr(tmp_result));
    }

    return result;
}

doca_error_t destroy_core_objects(struct program_core_objects *state)
{
    doca_error_t tmp_result, result = DOCA_SUCCESS;

    if (state->pe != NULL)
    {
        tmp_result = doca_pe_destroy(state->pe);
        if (tmp_result != DOCA_SUCCESS)
        {
            DOCA_ERROR_PROPAGATE(result, tmp_result);
            DOCA_LOG_ERR("Failed to destroy pe: %s", doca_error_get_descr(tmp_result));
        }
        state->pe = NULL;
    }

    if (state->buf_inv != NULL)
    {
        tmp_result = doca_buf_inventory_destroy(state->buf_inv);
        if (tmp_result != DOCA_SUCCESS)
        {
            DOCA_ERROR_PROPAGATE(result, tmp_result);
            DOCA_LOG_ERR("Failed to destroy buf inventory: %s", doca_error_get_descr(tmp_result));
        }
        state->buf_inv = NULL;
    }

    if (state->dst_mmap != NULL)
    {
        tmp_result = doca_mmap_destroy(state->dst_mmap);
        if (tmp_result != DOCA_SUCCESS)
        {
            DOCA_ERROR_PROPAGATE(result, tmp_result);
            DOCA_LOG_ERR("Failed to destroy destination mmap: %s", doca_error_get_descr(tmp_result));
        }
        state->dst_mmap = NULL;
    }

    if (state->src_mmap != NULL)
    {
        tmp_result = doca_mmap_destroy(state->src_mmap);
        if (tmp_result != DOCA_SUCCESS)
        {
            DOCA_ERROR_PROPAGATE(result, tmp_result);
            DOCA_LOG_ERR("Failed to destroy source mmap: %s", doca_error_get_descr(tmp_result));
        }
        state->src_mmap = NULL;
    }

    if (state->dev != NULL)
    {
        tmp_result = doca_dev_close(state->dev);
        if (tmp_result != DOCA_SUCCESS)
        {
            DOCA_ERROR_PROPAGATE(result, tmp_result);
            DOCA_LOG_ERR("Failed to close device: %s", doca_error_get_descr(tmp_result));
        }
        state->dev = NULL;
    }

    return result;
}

char *hex_dump(const void *data, size_t size)
{
    /*
     * <offset>:     <Hex bytes: 1-8>        <Hex bytes: 9-16>         <Ascii>
     * 00000000: 31 32 33 34 35 36 37 38  39 30 61 62 63 64 65 66  1234567890abcdef
     *    8     2         8 * 3          1          8 * 3         1       16       1
     */
    const size_t line_size = 8 + 2 + 8 * 3 + 1 + 8 * 3 + 1 + 16 + 1;
    size_t i, j, r, read_index;
    size_t num_lines, buffer_size;
    char *buffer, *write_head;
    unsigned char cur_char, printable;
    char ascii_line[17];
    const unsigned char *input_buffer;

    /* Allocate a dynamic buffer to hold the full result */
    num_lines = (size + 16 - 1) / 16;
    buffer_size = num_lines * line_size + 1;
    buffer = (char *)malloc(buffer_size);
    if (buffer == NULL)
        return NULL;
    write_head = buffer;
    input_buffer = data;
    read_index = 0;

    for (i = 0; i < num_lines; i++)
    {
        /* Offset */
        snprintf(write_head, buffer_size, "%08lX: ", i * 16);
        write_head += 8 + 2;
        buffer_size -= 8 + 2;
        /* Hex print - 2 chunks of 8 bytes */
        for (r = 0; r < 2; r++)
        {
            for (j = 0; j < 8; j++)
            {
                /* If there is content to print */
                if (read_index < size)
                {
                    cur_char = input_buffer[read_index++];
                    snprintf(write_head, buffer_size, "%02X ", cur_char);
                    /* Printable chars go "as-is" */
                    if (' ' <= cur_char && cur_char <= '~')
                        printable = cur_char;
                    /* Otherwise, use a '.' */
                    else
                        printable = '.';
                    /* Else, just use spaces */
                }
                else
                {
                    snprintf(write_head, buffer_size, "   ");
                    printable = ' ';
                }
                ascii_line[r * 8 + j] = printable;
                write_head += 3;
                buffer_size -= 3;
            }
            /* Spacer between the 2 hex groups */
            snprintf(write_head, buffer_size, " ");
            write_head += 1;
            buffer_size -= 1;
        }
        /* Ascii print */
        ascii_line[16] = '\0';
        snprintf(write_head, buffer_size, "%s\n", ascii_line);
        write_head += 16 + 1;
        buffer_size -= 16 + 1;
    }
    /* No need for the last '\n' */
    write_head[-1] = '\0';
    return buffer;
}

uint64_t align_up_uint64(uint64_t value, uint64_t alignment)
{
    uint64_t remainder = (value % alignment);

    if (remainder == 0)
        return value;

    return value + (alignment - remainder);
}

uint64_t align_down_uint64(uint64_t value, uint64_t alignment)
{
    return value - (value % alignment);
}
