# BlockOS Userspace X11 Stack (X server + 3 X11 applications)
CC      ?= gcc
LIBC_DIR ?= ../libc/build
GCC_INC := $(shell $(CC) -print-file-name=include)
LIBGCC  := $(shell $(CC) -print-libgcc-file-name)

CFLAGS := -std=gnu11 -O2 -Wall -Wextra -Wno-unused-parameter \
          -ffreestanding -fno-stack-protector -fno-pie -no-pie -nostdinc \
          -isystem ../libc/include -isystem $(GCC_INC)

LDFLAGS := -static -nostdlib
LIBS    := $(LIBC_DIR)/libc.a $(LIBGCC)

TARGETS := ../../rootfs/bin/X \
           ../../rootfs/bin/x11_settings \
           ../../rootfs/bin/x11_files \
           ../../rootfs/bin/x11_terminal

.PHONY: all clean

all: $(TARGETS)

../../rootfs/bin/X: xserver.c x11_client.h ../../font8x8.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBC_DIR)/crt1.o xserver.c $(LIBS) -o $@
	@echo "[OK] built $@"

../../rootfs/bin/x11_settings: settings.c x11_client.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBC_DIR)/crt1.o settings.c $(LIBS) -o $@
	@echo "[OK] built $@"

../../rootfs/bin/x11_files: files.c x11_client.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBC_DIR)/crt1.o files.c $(LIBS) -o $@
	@echo "[OK] built $@"

../../rootfs/bin/x11_terminal: terminal.c x11_client.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBC_DIR)/crt1.o terminal.c $(LIBS) -o $@
	@echo "[OK] built $@"

clean:
	rm -f $(TARGETS)
