#pragma once

namespace virtio_net {
    // Force a full reclaim attempt: scan used ring and recycle any remaining buffers.
    void force_reclaim_all();
}
