Virtio Net — Milestone 3 Started

This commit adds a more complete virtio-net driver that allocates RX/TX virtqueues,
submits receive buffers, and provides basic send/receive APIs using DMA-allocated
buffers and the virtqueue_ops helpers.

What is implemented
- Allocation of RX and TX virtqueue memory via dma::alloc and virtqueue::alloc_virtqueue
- virtqueue_ops view and ring initialization for RX/TX
- Submission of RX buffers for device-written frames (1536 bytes each)
- send_packet: allocates a DMA buffer, copies frame data, submits to TX queue and notifies device (best-effort)
- receive_packet: polls RX used ring, returns packet length and copies into provided buffer, then resubmits the buffer

Limitations and TODOs
- Notify offsets and legacy queue programming are best-effort and may need adjustment for certain devices; this is targeted at QEMU virtio devices.
- The driver currently does not wait for TX completion or reclaim TX buffers — a proper free/reclaim mechanism will be added.
- lwIP integration via lwip_adapter is ready to call virtio_net::receive_packet/transmit in a loop.

How to test (QEMU)
- Start QEMU with a virtio-net device backed by user-mode NAT:
  qemu-system-x86_64 -bios /usr/share/ovmf/OVMF_CODE.fd -drive file=disk.img,format=raw,id=drive0 -device virtio-blk-pci,drive=drive0 -netdev user,id=net0 -device virtio-net-pci,netdev=net0 -m 1024

Next steps
- Implement TX completion and buffer recycling
- Integrate lwIP fully (netif adapter, netif->input) and run DHCP/ping tests
- Improve feature negotiation and modern (virtio 1.0) device support

