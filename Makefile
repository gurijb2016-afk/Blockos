
# ============================================================
# BlockOS GNU-EFI C++ Build System
# x86_64 UEFI
# ============================================================

EFI_INCL       := /usr/include/efi
EFI_INCL_X86   := /usr/include/efi/x86_64

GNU_EFI_LIBDIR := /usr/lib
GNU_EFI_LDS    := $(GNU_EFI_LIBDIR)/elf_x86_64_efi.lds

EFI_CRT        := $(GNU_EFI_LIBDIR)/crt0-efi-x86_64.o
EFI_LIB        := $(GNU_EFI_LIBDIR)/libefi.a
GNU_EFI_LIB    := $(GNU_EFI_LIBDIR)/libgnuefi.a

CXX      := g++
LD       := ld
OBJCOPY  := objcopy
PYTHON   := python3

# ============================================================
# C++ FLAGS
# ============================================================

CXXFLAGS := \
	-fno-exceptions \
	-fno-rtti \
	-fshort-wchar \
	-fPIC \
	-fno-stack-protector \
	-fno-stack-check \
	-mno-red-zone \
	-DEFI_FUNCTION_WRAPPER \
	-I. \
	-I$(EFI_INCL) \
	-I$(EFI_INCL_X86) \
	-ffreestanding \
	-O2 \
	-Wall \
	-Wextra \
	-Iarch/86_64x \
	-Ikernel \
	-Idrivers \
	-Ifs \
	-Iexamples \
	-Ilibc/include \
	-fvisibility=hidden \
	-fno-strict-overflow \
	-fno-delete-null-pointer-checks \
	-MMD \
	-MP \
	-Iinclude

# ============================================================
# ASSEMBLY FLAGS
# ============================================================

ASFLAGS := \
	-I. \
	-I$(EFI_INCL) \
	-I$(EFI_INCL_X86) \
	-ffreestanding \
	-MMD \
	-MP

# ============================================================
# LINKER FLAGS
# ============================================================

LDFLAGS := \
	-nostdlib \
	-znocombreloc \
	-T$(GNU_EFI_LDS) \
	-shared \
	-Bsymbolic

# ============================================================
# SOURCE DIRECTORIES
# ============================================================

SRC_DIRS := \
	drivers \
	examples \
	fs \
	kernel \
	libc/src \
	hal

S_SRC_DIRS := \
	drivers \
	examples \
	fs \
	kernel

# ============================================================
# EXCLUDED SOURCES
# ============================================================

EXCLUDED_SRC := \
	fs/EROFS.cpp \
	fs/tmpfs.cpp \
	kernel/login.cpp \
	kernel/panic.cpp \
	fs/lua_elf.cpp

# ============================================================
# C++ SOURCES
# ============================================================

SRC := $(filter-out $(EXCLUDED_SRC), \
	$(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.cpp)))

SRC += kernel/cmd/cmd_ata.cpp
SRC += kernel/cmd/cmd_forth.cpp

# ============================================================
# ASSEMBLY SOURCES
# ============================================================

S_SRC := $(foreach dir,$(S_SRC_DIRS),$(wildcard $(dir)/*.S))

S_SRC += arch/86_64x/irq_stubs.S
S_SRC += arch/86_64x/user_entry.S
S_SRC += arch/86_64x/syscall_entry.S

# ============================================================
# OBJECT FILES
# ============================================================

OBJ := \
	$(SRC:.cpp=.o) \
	$(S_SRC:.S=.o)

# ============================================================
# DEPENDENCY FILES
# ============================================================

DEP := $(OBJ:.o=.d)

# ============================================================
# BUILD OUTPUT
# ============================================================

BUILD_DIR := build

SO_OUT  := $(BUILD_DIR)/kernel.so
EFI_OUT := $(BUILD_DIR)/BOOTX64.EFI

# ============================================================
# LUA
# ============================================================

LUA_DIR    := ports/lua
LUA_SCRIPT := $(LUA_DIR)/build-lua.sh
LUA_ELF    := $(LUA_DIR)/build/lua

# ============================================================
# DEFAULT
# ============================================================

all: $(EFI_OUT)

# ============================================================
# FULL STACK
# ============================================================

.PHONY: full-stack

full-stack: host-all windowmaker install-windowmaker-rootfs rootfs-windowmaker-check $(EFI_OUT)

# ============================================================
# C++ COMPILATION
# ============================================================

%.o: %.cpp
	@mkdir -p $(dir $@)
	@echo "[CXX] $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ============================================================
# ASSEMBLY COMPILATION
# ============================================================

%.o: %.S
	@mkdir -p $(dir $@)
	@echo "[ASM] $<"
	$(CXX) $(ASFLAGS) -c $< -o $@

# ============================================================
# WEAKEN GNU-EFI memcpy / memset
# ============================================================

EFI_LIB_WEAK := $(BUILD_DIR)/libefi-weak.a

$(EFI_LIB_WEAK): $(EFI_LIB)
	@mkdir -p $(BUILD_DIR)
	@echo "[OBJCOPY] weakening memcpy/memset in $(notdir $(EFI_LIB))"
	cp $< $@
	$(OBJCOPY) \
		--weaken-symbol=memcpy \
		--weaken-symbol=memset \
		$@

# ============================================================
# LINK KERNEL.SO
# ============================================================

$(SO_OUT): $(OBJ) $(EFI_LIB_WEAK)
	@mkdir -p $(BUILD_DIR)

	@echo ""
	@echo "=============================================="
	@echo " Linking BlockOS kernel.so"
	@echo "=============================================="

	$(LD) \
		$(LDFLAGS) \
		-L$(GNU_EFI_LIBDIR) \
		$(EFI_CRT) \
		$(OBJ) \
		$(GNU_EFI_LIB) \
		$(EFI_LIB_WEAK) \
		-o $@

	@echo ""
	@echo "[OK] $@"

# ============================================================
# CREATE BOOTX64.EFI
# ============================================================

$(EFI_OUT): $(SO_OUT)
	@mkdir -p $(BUILD_DIR)

	@echo ""
	@echo "=============================================="
	@echo " Creating BOOTX64.EFI"
	@echo "=============================================="

	$(OBJCOPY) \
		-j .text \
		-j .plt \
		-j .init_array \
		-j .rodata \
		-j .ramfs \
		-j .dynstr \
		-j .sdata \
		-j .data \
		-j .dynamic \
		-j .dynsym \
		-j .rel \
		-j .rela \
		-j .rel.* \
		-j .rela.* \
		-j .reloc \
		--subsystem=10 \
		--target=efi-app-x86_64 \
		$(SO_OUT) \
		$(EFI_OUT)

	@echo ""
	@echo "[OK] $@"

# ============================================================
# LUA - OPTIONAL
# ============================================================

.PHONY: lua

lua:
	@set -eu; \
	if [ -z "$$USERLIBC" ]; then \
		echo "[ERROR] USERLIBC is not set."; \
		echo "[ERROR] Lua build skipped."; \
		exit 1; \
	fi; \
	if [ ! -d "$$USERLIBC" ]; then \
		echo "[ERROR] USERLIBC does not exist: $$USERLIBC"; \
		exit 1; \
	fi; \
	if [ ! -f "$(LUA_SCRIPT)" ]; then \
		echo "[ERROR] Missing $(LUA_SCRIPT)"; \
		exit 1; \
	fi; \
	chmod +x "$(LUA_SCRIPT)"; \
	USERLIBC="$$USERLIBC" "$(LUA_SCRIPT)"

# ============================================================
# CHECK GNU-EFI
# ============================================================

.PHONY: check-efi

check-efi:
	@echo "Checking GNU-EFI..."

	@test -f "$(GNU_EFI_LDS)" \
		&& echo "[OK] Linker script: $(GNU_EFI_LDS)" \
		|| echo "[ERROR] Missing: $(GNU_EFI_LDS)"

	@test -f "$(EFI_CRT)" \
		&& echo "[OK] CRT: $(EFI_CRT)" \
		|| echo "[ERROR] Missing: $(EFI_CRT)"

	@test -f "$(EFI_LIB)" \
		&& echo "[OK] libefi: $(EFI_LIB)" \
		|| echo "[ERROR] Missing: $(EFI_LIB)"

	@test -f "$(GNU_EFI_LIB)" \
		&& echo "[OK] libgnuefi: $(GNU_EFI_LIB)" \
		|| echo "[ERROR] Missing: $(GNU_EFI_LIB)"

# ============================================================
# MENUCONFIG
# ============================================================

.PHONY: menuconfig

menuconfig:
	$(PYTHON) scripts/menuconfig.py

# ============================================================
# CLEAN
# ============================================================

.PHONY: clean

clean:
	rm -f $(OBJ)
	rm -f $(DEP)
	rm -rf $(BUILD_DIR)

# ============================================================
# REBUILD
# ============================================================

.PHONY: rebuild

rebuild:
	$(MAKE) clean
	$(MAKE) all

# ============================================================
# RUN
# ============================================================

.PHONY: run

run: all
	sh build_and_run.sh

# ============================================================
# LINUX REPLACEMENT CHECK
# ============================================================

.PHONY: linux-replacement-check release-gate toolchain-bootstrap

linux-replacement-check:
	@mkdir -p build/linux-replacement
	$(CXX) -std=c++17 -Wall -Wextra -Werror -I. \
		kernel/compat/linux_compat_api.cpp \
		drivers/net/e1000.cpp \
		drivers/block/nvme.cpp \
		tests/linux_replacement_host.cpp \
		-o build/linux-replacement-test
	./build/linux-replacement-test
	./scripts/release/release_gate.sh

release-gate: platform-check server-profile-check linux-replacement-check
	@echo "[OK] BlockOS Linux-replacement release gate passed."

toolchain-bootstrap:
	./scripts/toolchain/build-blockos-toolchain.sh

# ============================================================
# HOST TESTS
# ============================================================

.PHONY: host-all browser-host windowmaker-runtime-check \
	install-windowmaker-rootfs windowmaker windowmaker-clean \
	bx11 bx11-api-test network-host-smoke browser-source-check \
	tls-backend-check linux20-plus-check rootfs-windowmaker-check \
	usermode-foundation-check userspace-runtime-check \
	bx11-install windowmaker-bx11 platform-check verify-platform \
	server-profile server-profile-check

host-all: bx11 network-host-smoke browser-host browser-source-check \
	tls-backend-check linux20-plus-check usermode-foundation-check

browser-host:
	$(MAKE) -C ports/browser -j$$(nproc)

windowmaker-runtime-check:
	bash scripts/windowmaker_runtime_check.sh

install-windowmaker-rootfs:
	@set -eu; \
	if [ ! -x build/windowmaker-rootfs/usr/bin/wmaker ]; then \
		echo "[ERROR] build/windowmaker-rootfs/usr/bin/wmaker is missing; build Window Maker first."; \
		exit 1; \
	fi; \
	mkdir -p build/sysroot/usr/bin build/sysroot/usr/lib build/sysroot/usr/share; \
	cp -a build/windowmaker-rootfs/usr/bin/wmaker build/sysroot/usr/bin/; \
	if [ -d build/windowmaker-rootfs/usr/lib ]; then \
		cp -a build/windowmaker-rootfs/usr/lib/. build/sysroot/usr/lib/; \
	fi; \
	if [ -d build/windowmaker-rootfs/usr/share ]; then \
		cp -a build/windowmaker-rootfs/usr/share/. build/sysroot/usr/share/; \
	fi; \
	echo "[OK] Window Maker installed into BlockOS sysroot"

windowmaker:
	BLOCKOS_SYSROOT=$(CURDIR)/build/sysroot JOBS=$$(nproc) ./windowmaker.sh

windowmaker-clean:
	sh ports/windowmaker/scripts/windowmaker-clean.sh

bx11:
	$(MAKE) -C ports/bx11 host-test

bx11-api-test:
	cc -std=c11 \
		-Iuserland/include \
		ports/windowmaker/tests/x11-api-compile.c \
		userland/libx11/xlib_stub.cpp \
		-lstdc++ \
		-o build/x11-api-test

network-host-smoke:
	@mkdir -p build/network-host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -I. \
		kernel/network/tcp/tcp_segment.cpp \
		kernel/network/dns/dns_wire.cpp \
		kernel/network/tls/tls_record.cpp \
		tests/network_host_smoke.cpp \
		-o build/network-host-smoke
	./build/network-host-smoke

browser-source-check:
	test -f apps/blockbrowser/browser.hpp
	test -f apps/blockbrowser/browser.cpp
	test -f ports/browser/Makefile
	@echo "[OK] BlockOS browser source + HTTP/URL parser present"

tls-backend-check:
	test -f ports/tls/mbedtls_adapter.hpp
	test -f ports/tls/mbedtls_adapter.cpp
	test -f ports/tls/README.md
	@echo "[OK] TLS backend integration point present"

linux20-plus-check:
	@echo "[CHECK] Linux 2.0-era core capability coverage"
	test -f kernel/process.cpp
	test -f kernel/scheduler.cpp
	test -f kernel/elf_loader.cpp
	test -f kernel/syscall/syscall_dispatcher.cpp
	test -f kernel/network/tcp/tcp_socket.cpp
	test -f kernel/network/udp/udp_socket.cpp
	test -f kernel/network/ipv4/ipv4_packet.cpp
	test -f fs/ext2.cpp
	test -f fs/ext4.cpp
	test -f drivers/virtio_net_driver.cpp
	test -f drivers/virtio_blk.cpp
	test -f arch/86_64x/user_entry.S
	test -f kernel/io_uring.cpp
	test -f kernel/rcu.cpp
	test -f kernel/livepatch/livepatch.cpp
	test -f ports/bx11/libX11/display.cpp
	test -x windowmaker.sh
	@echo "[OK] BlockOS core capability source audit passed."

rootfs-windowmaker-check:
	bash -n scripts/generate_rootfs.sh
	bash -n ports/windowmaker/scripts/windowmakerdowloader+builder.sh
	@test -f kernel/init/services/gui.service
	@grep -q '^ExecStart=/usr/bin/wmaker$$' kernel/init/services/gui.service
	@grep -q '/etc/services/gui.service' fs/files.cpp
	@grep -q 'vfs_init_from_ramfs();' kernel/kernel.cpp
	@grep -q 'init_system.boot("/etc/services/")' kernel/kernel.cpp
	@echo "BlockOS rootfs + Window Maker autostart integration checks passed."

usermode-foundation-check:
	@test -f kernel/usermode/user_mode.hpp
	@test -f arch/86_64x/user_mode_gdt.cpp
	@test -f arch/86_64x/user_entry.S
	@grep -q 'USER_CODE_SELECTOR = 0x1B' kernel/usermode/user_mode.hpp
	@grep -q 'iretq' arch/86_64x/user_entry.S
	@echo "BlockOS user-mode/Ring3 foundation checks passed."

userspace-runtime-check:
	@echo "[CHECK] userspace runtime layers"
	bash scripts/check_autostart.sh
	bash scripts/windowmaker_runtime_check.sh
	@test -f arch/86_64x/usermode.cpp
	test -f arch/86_64x/user_entry.S
	test -f kernel/syscall/syscall_entry.S
	test -f kernel/syscall/syscall_entry.cpp
	test -f userland/runtime/syscall_runtime.cpp
	@grep -q 'map_user_zero_pages' arch/86_64x/paging.cpp arch/86_64x/paging.hpp
	@grep -q 'blockos_syscall_dispatch_entry' kernel/syscall/syscall_entry.S
	@echo "[OK] userspace runtime checks passed"

bx11-install:
	$(MAKE) -C ports/bx11 clean
	$(MAKE) -C ports/bx11 -j$$(nproc)
	BLOCKOS_SYSROOT=$(CURDIR)/build/sysroot ports/bx11/install-sysroot.sh

windowmaker-bx11: bx11-install
	BLOCKOS_SYSROOT=$(CURDIR)/build/sysroot ./windowmaker.sh

platform-check:
	@echo "[CHECK] BlockOS portable feature layer"
	@$(MAKE) --no-print-directory -C . verify-platform

verify-platform:
	sh scripts/verify_platform_features.sh

server-profile:
	@./scripts/server/build_server_profile.sh
	@./scripts/server/validate_server_profile.sh

server-profile-check: server-profile

# ============================================================
# HEADER DEPENDENCIES
# ============================================================

-include $(DEP)
