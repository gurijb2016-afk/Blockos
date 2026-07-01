Updated README: next steps for virtio drivers and persistence

- See VIRTIO_ROADMAP.md for the plan to implement virtio-pci, virtio-input, and virtio-blk.
- Current runtime: PS/2 input (mouse/keyboard) is used by default; virtio_input is a stub and virtio_blk is a stub.

To test virtio devices in QEMU when drivers are implemented:
- virtio-blk example: -drive file=some.img,format=raw,id=drive0 -device virtio-blk-pci,drive=drive0
- virtio-input example: -device virtio-input-pci

