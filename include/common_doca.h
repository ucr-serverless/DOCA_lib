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

#ifndef COMMON_H_
#define COMMON_H_

#include <bits/time.h>
#include <doca_buf.h>
#include <doca_dev.h>
#include <doca_error.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#define NS_PER_SEC 1E9  /* Nano-seconds per second */
#define NS_PER_MSEC 1E6 /* Nano-seconds per millisecond */
#define NS_PER_USEC 1E3
#define USEC_PER_SEC 1E6
#define MSEC_PER_SEC 1E3
#define MAX_USER_ARG_SIZE (256)              /* Maximum size of user input argument */
#define MAX_ARG_SIZE (MAX_USER_ARG_SIZE + 1) /* Maximum size of input argument */
#define MAX_TXT_SIZE (4096 + 1)              /* Maximum size of input text */

#ifdef CLOCK_MONOTONIC_RAW /* Defined in glibc bits/time.h */
#define CLOCK_TYPE_ID CLOCK_MONOTONIC_RAW
#else
#define CLOCK_TYPE_ID CLOCK_MONOTONIC
#endif
#define EXIT_ON_FAILURE(_expression_)                                                                                  \
    {                                                                                                                  \
        doca_error_t _status_ = _expression_;                                                                          \
                                                                                                                       \
        if (_status_ != DOCA_SUCCESS)                                                                                  \
        {                                                                                                              \
            DOCA_LOG_ERR("%s failed with status %s", __func__, doca_error_get_descr(_status_));                        \
            return _status_;                                                                                           \
        }                                                                                                              \
    }
#define LOG_ON_FAILURE(_result)                                                                                        \
    {                                                                                                                  \
        if (_result != DOCA_SUCCESS)                                                                                   \
        {                                                                                                              \
            DOCA_LOG_ERR("%s failed with status %s", __func__, doca_error_get_descr(_result));                         \
        }                                                                                                              \
    }
// evaluate the expression and jump to label is the result is not DOCA_SUCCESS
#define EVAL_JUMP_ON_DOCA_ERROR(_expression_, _label)                                                                  \
    {                                                                                                                  \
        doca_error_t _status_ = _expression_;                                                                          \
                                                                                                                       \
        if (_status_ != DOCA_SUCCESS)                                                                                  \
        {                                                                                                              \
            DOCA_LOG_ERR("%s: %s failed with status %s", __func__, #_expression_, doca_error_get_descr(_status_));     \
            goto _label;                                                                                               \
        }                                                                                                              \
    }
// if the result is not DOCA_SUCCESS then jump to the _label
#define JUMP_ON_DOCA_ERROR(_result, _label)                                                                            \
    {                                                                                                                  \
                                                                                                                       \
        if (_result != DOCA_SUCCESS)                                                                                   \
        {                                                                                                              \
            DOCA_LOG_ERR("%s failed with status %s", __func__, doca_error_get_descr(_result));                         \
            goto _label;                                                                                               \
        }                                                                                                              \
    }
// if the status is false, then jump to label and log with custom string
#define JUMP_ON_FAILURE_CONDITION(_status_, _label, _string)                                                           \
    {                                                                                                                  \
                                                                                                                       \
        if (_status_)                                                                                                  \
        {                                                                                                              \
            DOCA_LOG_ERR("%s: %s failed with status %s", __func__, #_status_, _string);                                \
            goto _label;                                                                                               \
        }                                                                                                              \
    }

#ifdef __cplusplus
extern "C"
{
#endif
    /* Function to check if a given device is capable of executing some task */
    typedef doca_error_t (*tasks_check)(const struct doca_devinfo *);

    typedef bool (*predicate)(void *);

    /* DOCA core objects used by the samples / applications */
    struct program_core_objects
    {
        struct doca_dev *dev;               /* doca device */
        struct doca_mmap *src_mmap;         /* doca mmap for source buffer */
        struct doca_mmap *dst_mmap;         /* doca mmap for destination buffer */
        struct doca_buf_inventory *buf_inv; /* doca buffer inventory */
        struct doca_ctx *ctx;               /* doca context */
        struct doca_pe *pe;                 /* doca progress engine */
    };

    void wait_for_enter(void);
    /*
     * check and print the device capabilities
     *
     * @devinfo [in]: DOCA devinfo
     */
    void check_dev_cap(const struct doca_devinfo *devinfo);
    /*
     * Allocate memory and populate it into the memory map
     *
     * @mmap [in]: DOCA memory map
     * @buffer_len [in]: Allocated buffer length
     * @access_flags [in]: The access permissions of the mmap
     * @dev [in]: The device to bind to the mmap
     * @buffer [out]: Allocated buffer and user needs to free it
     * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
     */
    doca_error_t create_doca_mmap_from_buf(struct doca_mmap **mmap, size_t buffer_len, uint32_t access_flags,
                                           struct doca_dev *dev, char **buffer);

    void print_buffer_hex(const void *buffer, size_t length);

    doca_error_t sock_recv_ptr(uint64_t *ptr, int sock_fd);
    doca_error_t sock_recv_range(uint64_t *range, int sock_fd);
    doca_error_t sock_recv_buffer(void *rdma_conn_descriptor, uint32_t *descriptor_size, uint32_t descriptor_buf_size,
                                  int sock_fd);
    doca_error_t sock_send_ptr(uint64_t ptr, int sock_fd);
    doca_error_t sock_send_range(uint64_t range, int sock_fd);
    doca_error_t sock_send_buffer(const void *rdma_conn_descriptor, uint32_t descriptor_size, int sock_fd);
    /*
     * register the pe fd to the ep_fd
     *
     * @pe [in]: PCI address
     * @ep_fd [in]: pointer to a function that checks if the device have some task capabilities (Ignored if set to NULL)
     * @return: DOCA_SUCCESS on success
     */
    doca_error_t register_pe_event(struct doca_pe *pe, int ep_fd);

    /*
     * wait on the epoll fd and process the pe when the predicate returns true
     *
     * @pe [in]: the doca_pe
     * @ep_fd [in]: the epoll fd
     * @predicate [in]: the function pointer of the predicate
     * @func_args [in]: the arguments pointer
     * @return: DOCA_SUCCESS on success
     */
    doca_error_t run_for_competion(struct doca_pe *pe, int ep_fd, predicate func, void *func_args);
    /*
     * Open a DOCA device according to a given PCI address
     *
     * @pci_addr [in]: PCI address
     * @func [in]: pointer to a function that checks if the device have some task capabilities (Ignored if set to
     * NULL)
     * @retval [out]: pointer to doca_dev struct, NULL if not found
     * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
     */
    doca_error_t open_doca_device_with_pci(const char *pci_addr, tasks_check func, struct doca_dev **retval);

    /*
     * Open a DOCA device according to a given IB device name char*
     *
     * @device_name [in]: IB device name
     * @func [in]: pointer to a function that checks if the device have some task capabilities (Ignored if set to
     * NULL)
     * @retval [out]: pointer to doca_dev struct, NULL if not found
     * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
     */
    doca_error_t open_doca_device_with_ibdev_str(const char *device_name, tasks_check func,
                                                 struct doca_dev **doca_device);

    /*
     * Open a DOCA device according to a given IB device name
     *
     * @value [in]: IB device name
     * @val_size [in]: input length, in bytes
     * @func [in]: pointer to a function that checks if the device have some task capabilities (Ignored if set to
     * NULL)
     * @retval [out]: pointer to doca_dev struct, NULL if not found
     * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
     */
    doca_error_t open_doca_device_with_ibdev_name(const uint8_t *value, size_t val_size, tasks_check func,
                                                  struct doca_dev **retval);

    /*
     * Open a DOCA device according to a given interface name
     *
     * @value [in]: interface name
     * @val_size [in]: input length, in bytes
     * @func [in]: pointer to a function that checks if the device have some task capabilities (Ignored if set to
     * NULL)
     * @retval [out]: pointer to doca_dev struct, NULL if not found
     * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
     */
    doca_error_t open_doca_device_with_iface_name(const uint8_t *value, size_t val_size, tasks_check func,
                                                  struct doca_dev **retval);

    /*
     * Open a DOCA device with a custom set of capabilities
     *
     * @func [in]: pointer to a function that checks if the device have some task capabilities
     * @retval [out]: pointer to doca_dev struct, NULL if not found
     * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
     */
    doca_error_t open_doca_device_with_capabilities(tasks_check func, struct doca_dev **retval);

    /*
     * Open a DOCA device representor according to a given VUID string
     *
     * @local [in]: queries representors of the given local doca device
     * @filter [in]: bitflags filter to narrow the representors in the search
     * @value [in]: IB device name
     * @val_size [in]: input length, in bytes
     * @retval [out]: pointer to doca_dev_rep struct, NULL if not found
     * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
     */
    doca_error_t open_doca_device_rep_with_vuid(struct doca_dev *local, enum doca_devinfo_rep_filter filter,
                                                const uint8_t *value, size_t val_size, struct doca_dev_rep **retval);

    /*
     * Open a DOCA device according to a given PCI address
     *
     * @local [in]: queries representors of the given local doca device
     * @filter [in]: bitflags filter to narrow the representors in the search
     * @pci_addr [in]: PCI address
     * @retval [out]: pointer to doca_dev_rep struct, NULL if not found
     * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
     */
    doca_error_t open_doca_device_rep_with_pci(struct doca_dev *local, enum doca_devinfo_rep_filter filter,
                                               const char *pci_addr, struct doca_dev_rep **retval);

    /*
     * Initialize a series of DOCA Core objects needed for the program's execution
     *
     * @state [in]: struct containing the set of initialized DOCA Core objects
     * @max_bufs [in]: maximum number of buffers for DOCA Inventory
     * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
     */
    doca_error_t create_core_objects(struct program_core_objects *state, uint32_t max_bufs);

    /*
     * Request to stop context
     *
     * @pe [in]: DOCA progress engine
     * @ctx [in]: DOCA context added to the progress engine
     * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
     */
    doca_error_t request_stop_ctx(struct doca_pe *pe, struct doca_ctx *ctx);

    /*
     * Cleanup the series of DOCA Core objects created by create_core_objects
     *
     * @state [in]: struct containing the set of initialized DOCA Core objects
     * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
     */
    doca_error_t destroy_core_objects(struct program_core_objects *state);

    /*
     * Create a string Hex dump representation of the given input buffer
     *
     * @data [in]: Pointer to the input buffer
     * @size [in]: Number of bytes to be analyzed
     * @return: pointer to the string representation, or NULL if an error was encountered
     */
    char *hex_dump(const void *data, size_t size);

    /**
     * This method aligns a uint64 value up
     *
     * @value [in]: value to align up
     * @alignment [in]: alignment value
     * @return: aligned value
     */
    uint64_t align_up_uint64(uint64_t value, uint64_t alignment);

    /**
     * This method aligns a uint64 value down
     *
     * @value [in]: value to align down
     * @alignment [in]: alignment value
     * @return: aligned value
     */
    uint64_t align_down_uint64(uint64_t value, uint64_t alignment);

    double calculate_timediff_ms(struct timespec *end, struct timespec *start);
    double calculate_timediff_usec(struct timespec *end, struct timespec *start);
    double calculate_timediff_nsec(struct timespec *end, struct timespec *start);
    doca_error_t init_inventory(struct doca_buf_inventory **inv, uint64_t num);
    doca_error_t destroy_inventory(struct doca_buf_inventory *inv);
    doca_error_t get_buf_from_inv_and_reset_data_len(struct doca_buf_inventory *inv, struct doca_mmap *mmap,
                                                     char *data_start, size_t len, struct doca_buf **buf);
    doca_error_t set_buf_to_len(struct doca_buf *buf);
    doca_error_t safe_buf_decounter(struct doca_buf *buf);

    size_t print_doca_buf_len(struct doca_buf *buf);
#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
