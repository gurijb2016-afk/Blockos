/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef __NSM_TYPE3_H__
#define __NSM_TYPE3_H__

/* ------------------------ Includes --------------------------------------- */
/* ------------------------ Macros ----------------------------------------- */

#pragma pack(1)
/**
 * \defgroup TYPE3_CMDS Command Code Encoding
 * @{
 */

//! @ref TYPE3_GET_MAXIMUM_CUSTOMER_BOOST_CLOCK
#define NSM_TYPE3_CMD_GET_MAXIMUM_CUSTOMER_BOOST_CLOCK           0x40

//! @ref TYPE3_SET_MAXIMUM_CUSTOMER_BOOST_CLOCK
#define NSM_TYPE3_CMD_SET_MAXIMUM_CUSTOMER_BOOST_CLOCK           0x41

//! @ref TYPE3_GET_VIOLATION_DURATION
#define NSM_TYPE3_CMD_GET_VIOLATION_DURATION                     0x45

//! @ref NSM_TYPE3_GET_CURRENT_UTIL
#define NSM_TYPE3_CMD_GET_CURRENT_UTILIZATION                    0x47

//! @ref TYPE3_GET_MIG_MODE
#define NSM_TYPE3_CMD_GET_MIG_MODE                               0x4D

//! @ref TYPE3_SET_MIG_MODE
#define NSM_TYPE3_CMD_SET_MIG_MODE                               0x4E

//! @ref TYPE3_GET_ECC_MODE
#define NSM_TYPE3_CMD_GET_ECC_MODE                               0x4F

//! @ref TYPE3_SET_ECC_MODE
#define NSM_TYPE3_CMD_SET_ECC_MODE                               0x7C

//! @ref TYPE3_GET_MEMORY_CAPACITY_UTILIZATION
#define NSM_TYPE3_CMD_GET_MEMORY_CAPACITY_UTILIZATION            0xAD

/** @} TYPE3_CMDS */

/** \defgroup TYPE3_GET_MAXIMUM_CUSTOMER_BOOST_CLOCK \
 *            Get Maximum Customer Boost Clock
 * @{
 * This command retrieves the current maximum customer boost clock.
 */

/**
 * \brief Get Maximum Customer Boost Clock response
 */
struct nsm_type3_get_maximum_customer_boost_clock_response
{
    /*!
     * Current device Max Customer Boost Clock value in MHz.
     */
    NvU32 current_limit_mhz;
};

/** @} TYPE3_GET_MAXIMUM_CUSTOMER_BOOST_CLOCK */

/** \defgroup TYPE3_SET_MAXIMUM_CUSTOMER_BOOST_CLOCK \
 *            Set Maximum Customer Boost Clock
 * @{
 * This command sets the maximum customer boost clock.
 */

/**
 * \brief Set Maximum Customer Boost Clock request
 */
struct nsm_type3_set_maximum_customer_boost_clock_request
{
    /*!
     * Maximum customer boost clock value to enforce in MHz.
     * The value must be within the range of supported clocks.
     */
    NvU32 requested_limit_mhz;
};

/** @} TYPE3_SET_MAXIMUM_CUSTOMER_BOOST_CLOCK */

/** \defgroup TYPE3_GET_VIOLATION_DURATION Get Violation Duration
 * @{
 * A violation is any event that forces the device to run below clocks required
 * for optimum performance. This API returns the duration that a violation
 * event was asserted. These can be of the following kinds:
 * Hardware: Any event that results in hardware induced clock capping.
 * Software: Any event resulting in software induced clock capping. Software
 *           violations can be further classified into:
 *           Power violation: Asserted when total device power exceeds the
 *                            current power limit.
 *           Thermal violation: Asserted when the device or memory temperature
 *                              exceeds the maximum operating temperature.
 *           Global violation: Asserted for any other condition that causes
 *                             current clocks to be lowered.
 */

/** \defgroup TYPE3_GET_VIOLATION_DURATION_TYPES Counter Types
 * @{
 */
#define NSM_TYPE3_GET_VIOLATION_DURATION_TYPE_HW_VIOLATION                    0
#define NSM_TYPE3_GET_VIOLATION_DURATION_TYPE_SW_GLOBAL                       1
#define NSM_TYPE3_GET_VIOLATION_DURATION_TYPE_SW_POWER                        2
#define NSM_TYPE3_GET_VIOLATION_DURATION_TYPE_SW_THERMAL                      3

#define NSM_TYPE3_GET_VIOLATION_DURATION_NUM_TYPES                            8
/** @} TYPE3_GET_VIOLATION_DURATION_TYPES */

/**
 * \brief Get Violation Duration response
 */
struct nsm_type3_get_violation_duration_response
{
    /*!
     * Mask of the supported violation counters.
     * @ref TYPE3_GET_VIOLATION_DURATION_TYPES for more info.
     */
    NvU64 supported;

    //! Duration of the violation in ns for the counters.
    NvU64 counters[NSM_TYPE3_GET_VIOLATION_DURATION_NUM_TYPES];
};

/** @} TYPE3_GET_VIOLATION_DURATION */

/**
 * \defgroup NSM_TYPE3_GET_CURRENT_UTIL \
 *           Get Current Utilization
 * @{
 *
 * @brief Get Current Utilization
 *
 * This command retrieves the current GPU and memory bandwidth utilization in
 * percent.
 *
 * @note The request for this command does not contain any payload.
 */
struct nsm_type3_get_current_utilization_response {
    //! GPU utilization in percent.
    NvU32 gpu_util_percent;

    //! Memory utilization in percent.
    NvU32 memory_util_percent;
};

/** \defgroup TYPE3_GET_MIG_MODE Get MIG Mode
 * @{
 * This command retrieves the current MIG mode setting.
 */

/** \defgroup NSM_TYPE3_GET_MIG_MODE_FLAGS Supported Flags
 * @{
 */
#define NSM_TYPE3_GET_MIG_MODE_PRESENT_MODE                         0:0
#define NSM_TYPE3_GET_MIG_MODE_PRESENT_MODE_OFF                       0
#define NSM_TYPE3_GET_MIG_MODE_PRESENT_MODE_ON                        1

#define NSM_TYPE3_GET_MIG_MODE_PENDING_MODE                         1:1
#define NSM_TYPE3_GET_MIG_MODE_PENDING_MODE_OFF                       0
#define NSM_TYPE3_GET_MIG_MODE_PENDING_MODE_ON                        1

#define NSM_TYPE3_GET_MIG_MODE_RESERVED                             7:2
/** @} NSM_TYPE3_GET_MIG_MODE_FLAGS */

/**
 * \brief Get MIG Mode response
 */
struct nsm_type3_get_mig_mode_response
{
    /*!
     * @ref NSM_TYPE3_GET_MIG_MODE_FLAGS for more info.
     */
    NvU8 flags;
};
/** @} TYPE3_GET_MIG_MODE */

/** \defgroup TYPE3_SET_MIG_MODE Set MIG Mode
 * @{
 * This command sets the desired MIG mode for the GPU.
 */

/** \defgroup NSM_TYPE3_SET_MIG_MODE_REQUESTED_MODES Supported Modes
 * @{
 */
#define NSM_TYPE3_SET_MIG_MODE_REQUESTED_MODE_OFF                     0
#define NSM_TYPE3_SET_MIG_MODE_REQUESTED_MODE_ON                      1
/** @} NSM_TYPE3_SET_MIG_MODE_REQUESTED_MODES */

/**
 * \brief Set MIG Mode request
 */
struct nsm_type3_set_mig_mode_request
{
    /*!
     * @ref NSM_TYPE3_SET_MIG_MODE_REQUESTED_MODES for more info.
     */
    NvU8 requested_mode;
};

/** @} TYPE3_SET_MIG_MODE */

/** \defgroup TYPE3_GET_ECC_MODE Get ECC Mode
 * @{
 * This command retrieves the current and pending ECC Modes.
 */

/** \defgroup NSM_TYPE3_GET_ECC_MODE_FLAGS Supported Flags
 * @{
 */
#define NSM_TYPE3_GET_ECC_MODE_PRESENT_MODE                         0:0
#define NSM_TYPE3_GET_ECC_MODE_PRESENT_MODE_OFF                       0
#define NSM_TYPE3_GET_ECC_MODE_PRESENT_MODE_ON                        1

#define NSM_TYPE3_GET_ECC_MODE_PENDING_MODE                         1:1
#define NSM_TYPE3_GET_ECC_MODE_PENDING_MODE_OFF                       0
#define NSM_TYPE3_GET_ECC_MODE_PENDING_MODE_ON                        1

#define NSM_TYPE3_GET_ECC_MODE_RESERVED                             7:2
/** @} NSM_TYPE3_GET_ECC_MODE_FLAGS */

/**
 * \brief Get ECC Mode response
 */
struct nsm_type3_get_ecc_mode_response
{
    /*!
     * @ref NSM_TYPE3_GET_ECC_MODE_FLAGS for more info.
     */
    NvU8 flags;
};
/** @} TYPE3_GET_ECC_MODE */

/** \defgroup TYPE3_SET_ECC_MODE Set ECC Mode
 * @{
 * This sets the desired ECC Mode for the GPU. The setting takes effect after
 * GPU reset.
 */

/** \defgroup NSM_TYPE3_SET_ECC_MODE_REQUESTED_MODES Supported Modes
 * @{
 */
#define NSM_TYPE3_SET_ECC_MODE_REQUESTED_MODE_OFF                     0
#define NSM_TYPE3_SET_ECC_MODE_REQUESTED_MODE_ON                      1
/** @} NSM_TYPE3_SET_ECC_MODE_REQUESTED_MODES */

/**
 * \brief Set ECC Mode request
 */
struct nsm_type3_set_ecc_mode_request
{
    /*!
     * @ref NSM_TYPE3_SET_ECC_MODE_REQUESTED_MODES for more info.
     */
    NvU8 requested_mode;
};

/** @} TYPE3_SET_ECC_MODE */

/** \defgroup TYPE3_GET_MEMORY_CAPACITY_UTILIZATION Get Memory Capacity Utilization
 * @{
 * This command retrieves the memory capacity utilization for the packet.
 */

/**
 * \brief Get Memory Capacity Utilization response
 */
struct nsm_type3_get_memory_capacity_utilization_response
{
    /*!
     * Device memory (in MiB) reserved for system use (driver or firmware).
     */
    NvU32 reserved_memory_mib;

    /*!
     * Allocated device memory (in MiB). Note that the driver/GPU always
     * sets aside a small amount of memory for book-keeping.
     */
    NvU32 used_memory_mib;
};

#pragma pack()

#endif /* __NSM_TYPE3_H__ */
