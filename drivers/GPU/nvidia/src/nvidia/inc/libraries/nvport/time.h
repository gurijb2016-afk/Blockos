/*
 * SPDX-FileCopyrightText: Copyright (c) 2016-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

/**
 * @file
 * @brief Time module public interface
 */

#ifndef _NVPORT_H_
#error "This file cannot be included directly. Include nvport.h instead."
#endif

#ifndef _NVPORT_TIME_H_
#define _NVPORT_TIME_H_

/**
 * @defgroup NVPORT_TIME Time module
 *
 * @brief This module contains basic time management functions
 *
 * @note It is not possible to provide the same precision on all platforms.
 * @ref portTimeGetNanoseconds does not necessarily have better resolution than
 * @ref portTimeGetMicroseconds, though it will never have a worse one.
 * Practically looking, all platforms offer a microsecond resolution. Above that
 * it starts to vary too much. Thus, when using these functions, use the ones
 * that work in units you need.
 *
 * @{
 */

/**
 * @name Core Functions
 * @{
 */

/**
 * @brief a simple timer/stopwatch type.
 *
 * This structure is meant to be opaque, but should freely be allocated on the
 * stack. Example use:
 *
 * ~~~{.c}
 * PORT_TIMER timer = {0}; // Initialize it to zero.
 * portTimeTimerStart(&timer); // Start counting.
 * doStuff();
 * portTimeTimerStop(&timer);  // Stop counting.
 * portTimeTimerStart(&timer); // Resume counting.
 * portTimeTimerReset(&timer); // Reset count to 0. Can be used instead of init.
 * NvU64 ms = portTimeTimerGetMilliseconds(&timer); // Get elapsed milliseconds.
 * ~~~
 *
 */
typedef struct PORT_TIMER PORT_TIMER;

/**
 * @brief Start the given timer.
 */
PORT_INLINE void portTimeTimerStart(PORT_TIMER *pTimer);
/**
 * @brief Stop (pause) the given timer.
 */
PORT_INLINE void portTimeTimerStop(PORT_TIMER *pTimer);
/**
 * @brief Reset the given timer to zero
 */
PORT_INLINE void portTimeTimerReset(PORT_TIMER *pTimer);
/**
 * @brief Query the given timer, returning the number of milliseconds elapsed
 */
PORT_INLINE NvU64 portTimeTimerGetMilliseconds(PORT_TIMER *pTimer);
/**
 * @brief Query the given timer, returning the number of microseconds elapsed
 */
PORT_INLINE NvU64 portTimeTimerGetMicroseconds(PORT_TIMER *pTimer);
/**
 * @brief Query the given timer, returning the number of nanoseconds elapsed
 */
PORT_INLINE NvU64 portTimeTimerGetNanoseconds(PORT_TIMER *pTimer);


/**
 * @brief Get the number of seconds elapsed since the Unix epoch.
 */
NvU32 portTimeGetSeconds(void);
/**
 * @brief Get the number of milliseconds elapsed since the Unix epoch.
 */
NvU64 portTimeGetMilliseconds(void);
/**
 * @brief Get the number of microseconds elapsed since the Unix epoch.
 */
NvU64 portTimeGetMicroseconds(void);
/**
 * @brief Get the number of nanoseconds elapsed since the Unix epoch.
 *
 * This function may have very poor precision, and should not be used for
 * precise time measurements. Use @ref portTimeGetUptimeNanoseconds
 *
 * Nanoseconds since Unix epoch will overflow the 64bit uint sometime in 2554.
 */
NvU64 portTimeGetNanoseconds(void);

/**
 * @brief Get the number of microseconds elapsed since the system boot.
 */
NvU64 portTimeGetUptimeMicroseconds(void);
/**
 * @brief Get the number of nanoseconds elapsed since the system boot.
 *
 * System uptime usually has better precision than time from Unix epoch.
 * For truly precise measurements, use
 * @ref portTimeGetUptimeNanosecondsHighPrecision
 */
NvU64 portTimeGetUptimeNanoseconds(void);
/**
 * @brief Query the performance counter to get a high precision uptime in ns.
 *
 * This timer offers the highest precision measurements available on the given
 * platform, but may be expensive to call. Use only when required.
 *
 * @warning It is not guaranteed that this counter actually has 1ns resolution.
 * It is just the most precise counter available on the current platform, that
 * returns the value in nanoseconds.
 */
NvU64 portTimeGetUptimeNanosecondsHighPrecision(void);
/**
 * @brief Get the resolution (in nanoseconds) of the platform's monotonic uptime
 *        tick source.
 *
 * This returns the number of nanoseconds between subsequent "ticks" of the
 * underlying monotonic uptime source used by this platform/build.
 * This resolution is generally applicable to portTimeGetUptimeNanoseconds(),
 * but not necessarily to portTimeGetUptimeNanosecondsHighPrecision().
 */
NvU64 portTimeGetUptimeResolution(void);


/**
 * @brief Structure representing time broken down into individual fields.
 *
 * @note The "second" field never has a value of 60. In case a time during a
 * leap second needs to be represented, the value of "second" will be 0.
 * However, after the leap second, it will go back to showing the correct value,
 * and will not permanently be late.
 *
 */
typedef struct PORT_WALLTIME
{
    NvU16 year;          // 0-65535 AD
    NvU16 month;         // 1-12
    NvU16 day;           // 1-31
    NvU16 hour;          // 0-23
    NvU16 minute;        // 0-59
    NvU16 second;        // 0-59
    NvU16 millisecond;   // 0-999
} PORT_WALLTIME;

/**
 * @brief Get the current system's local wall time.
 *
 * The time is returned as a PORT_WALLTIME struct, and all values are adjusted
 * for the current timezone.
 *
 * @note If the system time is manually modified, this will return the new
 * values - modifying the time between two calls may yield unexpected results.
 */
PORT_WALLTIME portTimeGetLocalWallTime(void);

/**
 * @brief Convert a Unix epoch timestamp (in ms) to PORT_WALLTIME struct.
 *
 * The result is adjusted for system's timezone (Unix epoch is always UTC)
 */
PORT_WALLTIME portTimeConvertToWallTime(NvU64 msInUnixEpoch);

/**
 * @brief Convert a PORT_WALLTIME struct to milliseconds since Unix epoch.
 *
 * The struct is assumed to be in the current system's timezone.
 *
 * @par Checked builds only:
 * - Will assert if walltime can not be represented in Unix time (e.g. before
 * the start of the epoch).
 */
NvU64 portTimeConvertToUnixMs(PORT_WALLTIME walltime);
/// @} End core functions

/**
 * @name Extended Functions
 * @{
 */

/// @} End extended functions

#include "nvport/inline/time_timer.h"

#endif // _NVPORT_TIME_H_
/// @}
