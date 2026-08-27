# BlockOS BX11 integration

This tree contains the BlockOS-side foundation for running Window Maker without porting Window Maker itself.

Layers:

1. `server/` — BlockOS X11-style window server/resource model.
2. `libX11/` — Xlib ABI/API compatibility entry points.
3. `libxcb/` — minimal XCB ABI/API entry points.
4. `ipc/` — framed client/server transport and in-kernel-safe ring channel.
5. `input/` — keyboard/mouse event translation.
6. `render/` — framebuffer surface, rectangles, pixels, copy operations.

The code is deliberately isolated from the freestanding EFI kernel until the hosted/userspace process and IPC runtime are ready. This keeps the existing kernel build working while providing a concrete subsystem to wire into BlockOS userspace.

`make -C ports/bx11 host-test` builds a hosted ABI smoke test. `make -C ports/bx11 clean` removes it.

This is an X11 compatibility subsystem, not a claim of full X.org protocol/extension completeness. Window Maker still needs the remaining API/extension coverage exercised by its build and runtime tests.
