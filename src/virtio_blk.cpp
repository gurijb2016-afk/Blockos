#include "virtio_blk.hpp"
#include <efi.h>
#include <efilib.h>

// Minimal virtio-blk stub: discovery is not implemented here. This file provides the API the kernel
// will call for persistence once the driver is implemented. Currently the functions return false.

bool virtio_blk::init() {
    Print(L"virtio-blk: init() stub called (not implemented)\n");
    return false;
}

bool virtio_blk::read_sector(uint64_t sector, uint8_t* out_buf) {
    (void)sector; (void)out_buf; return false;
}

bool virtio_blk::write_sector(uint64_t sector, const uint8_t* in_buf) {
    (void)sector; (void)in_buf; return false;
}
