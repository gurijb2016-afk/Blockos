Virtio enhancements: multi-sector blk read/write and flush; VFS adapter extended to multi-sector and flush operations.

What's included:
- virtio_blk_full: read_sectors, write_sectors, flush
- vfs_blk_adapter: wrappers for multi-sector operations and flush

Notes:
- Multi-sector operations are implemented as per-sector sequential requests for simplicity. For performance, batching multiple sectors into single descriptor chains should be implemented later.
- Flush sends BLK_T_FLUSH request (type 4) with no data.
