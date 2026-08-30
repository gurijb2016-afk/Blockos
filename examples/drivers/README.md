# BlockOS driver examples

These are BlockOS-oriented examples corresponding to the role that Linux
kernel `samples/` programs play: small examples showing one subsystem or API.
They are not Linux drivers and do not use Linux's `struct device`, `pci_driver`,
`net_device`, or module APIs.

For a real BlockOS driver, use the interfaces already present in this tree:

1. discover hardware through `PciSubsystem`/PCI helpers;
2. register it with `DeviceManager` where appropriate;
3. use the existing DMA allocator for DMA-capable buffers;
4. use the existing VirtIO/PS2/IRQ infrastructure instead of duplicating it;
5. expose a stable BlockOS userspace interface when one exists.

The small examples in the parent directories are intentionally split by
subsystem so they can be copied into a driver or service implementation.
