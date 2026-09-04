#pragma once

#include <cstdint>

class virtio_blk_full {
public:
    bool read_sector(
        uint64_t sector,
        uint8_t* out_buf
    )
    {
        return read_sectors(
            sector,
            1,
            out_buf
        );
    }

    bool write_sector(
        uint64_t sector,
        const uint8_t* in_buf
    )
    {
        return write_sectors(
            sector,
            1,
            in_buf
        );
    }

    bool read_sectors(
        uint64_t sector,
        uint32_t count,
        uint8_t* out_buf
    );

    bool write_sectors(
        uint64_t sector,
        uint32_t count,
        const uint8_t* in_buf
    );
};
