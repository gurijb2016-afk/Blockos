# Driver example

This directory documents where a BlockOS driver example should live. A real driver must use the BlockOS kernel driver/device-registration API rather than pretending that POSIX userspace calls are kernel-driver APIs.

Recommended progression:
1. register a device in the kernel
2. expose a stable file descriptor interface
3. implement read/write/ioctl
4. add interrupt/DMA handling when the hardware needs it
5. add a sample userspace program under `examples/`
