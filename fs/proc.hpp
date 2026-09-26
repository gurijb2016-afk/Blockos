#pragma once

#include <stddef.h>
#include <stdint.h>

namespace blockos
{
namespace proc
{

/*
 * Read a virtual procfs node.
 *
 * Examples:
 *   "meminfo"
 *   "version"
 *   "1/status"
 *   "1/stat"
 *   "1/cmdline"
 *   "1/maps"
 *   "self/status"
 */
size_t read(
    const char* path,
    char* buffer,
    size_t max_size);

/*
 * Returns true when a virtual procfs node or directory exists.
 */
bool exists(
    const char* path);

/*
 * Root-level /proc directory entries.
 */
size_t count();

const char* name_at(
    size_t index);

/*
 * Process directory helpers.
 */
size_t process_count();

uint64_t process_pid_at(
    size_t index);

/*
 * Returns a process-directory name into buffer:
 *
 *   "1"
 *   "2"
 *   "15"
 */
size_t process_name_at(
    size_t index,
    char* buffer,
    size_t max_size);

/*
 * procfs lifecycle.
 */
void init();

/*
 * Small self-test.
 */
bool test();

/*
 * Compatibility wrapper used by older code.
 */
void init_proc_fs();

} // namespace proc
} // namespace blockos
