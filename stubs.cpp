#include <stdint.h>
#include <stddef.h>
#include "drivers/network.hpp"
#include "drivers/virtio_common.hpp"
#include "drivers/virtio_common_state.hpp"
#include "drivers/virtio_common_modern.hpp"
#include "drivers/virtqueue.hpp"
#include "drivers/report-problem.hpp"
#include "drivers/virtio_blk_full.hpp"
#include "kernel/scheduler_preempt.hpp"

extern "C" {
void __stack_chk_fail(void) {}
void task_exit() { scheduler_preempt::task_exit(); }
void bb_blit_to_fb(void* fb, const uint8_t* bb) { (void)fb; (void)bb; }
void bb_blit_rect_to_fb(void* fb, const uint8_t* bb, int x, int y, int w, int h) { (void)fb; (void)bb; (void)x; (void)y; (void)w; (void)h; }
int blockos_scheduler_fork() { return -1; }
void* blockos_mmap_allocate(size_t length) { (void)length; return nullptr; }
}

NetworkStack net_engine;

bool virtio_blk_is_ready() { return false; }

bool virtio_blk_submit_request(
    void* header, void* data, uint32_t data_len,
    void* status, bool device_writes_data)
{
    (void)header; (void)data; (void)data_len;
    (void)status; (void)device_writes_data;
    return false;
}

namespace virtio_common {
    bool probe_device(DeviceType type, DeviceHandle* h)
    { (void)type; (void)h; return false; }

    void set_device_status(DeviceHandle* h, uint8_t status)
    { (void)h; (void)status; }

    uint8_t get_device_status(DeviceHandle* h)
    { (void)h; return 0; }
}

namespace virtqueue {
    VirtqDesc* alloc_virtqueue(void* mem, uint32_t qsize, uint32_t* desc_count)
    { (void)mem; (void)qsize; (void)desc_count; return nullptr; }
}

bool program_queue_pfn(
    const virtio_common::DeviceHandle* h, uint16_t queue_sel, void* pfn_mem)
{ (void)h; (void)queue_sel; (void)pfn_mem; return false; }

void ReportProblem(ProblemLevel level, const char* component, const char* message)
{ (void)level; (void)component; (void)message; }

bool virtio_blk_full::read_sector(uint64_t sector, uint8_t* buf)
{ (void)sector; (void)buf; return false; }

bool virtio_blk_full::write_sector(uint64_t sector, const uint8_t* buf)
{ (void)sector; (void)buf; return false; }
