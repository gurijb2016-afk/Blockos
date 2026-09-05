
PROJECT      := $(CURDIR)

ARCH         := x86_64
EFI_INCL     ?= /usr/include/efi
EFI_INCL_X86 ?= /usr/include/efi/x86_64
EFI_LIB      ?= /usr/lib
EFI_CRT      ?= /usr/lib
EFI_LDS      ?= /usr/lib/elf_x86_64_efi.lds

CXX          ?= x86_64-elf-g++
CC           ?= x86_64-elf-gcc
AS           ?= x86_64-elf-as
LD           ?= x86_64-elf-ld
AR           ?= x86_64-elf-ar
OBJCOPY      ?= x86_64-elf-objcopy

# ------------------------------------------------------------
# BlockOS userspace libc
# ------------------------------------------------------------

USERLIBC ?=

ifeq ($(strip $(USERLIBC)),)
ifneq ($(wildcard $(PROJECT)/userspace/libc),)
USERLIBC := $(PROJECT)/userspace/libc
else ifneq ($(wildcard $(PROJECT)/userspace/sysroot),)
USERLIBC := $(PROJECT)/userspace/sysroot
else ifneq ($(wildcard $(PROJECT)/libc),)
USERLIBC := $(PROJECT)/libc
else ifneq ($(wildcard $(PROJECT)/sysroot),)
USERLIBC := $(PROJECT)/sysroot
else ifneq ($(wildcard $(PROJECT)/userspace),)
USERLIBC := $(PROJECT)/userspace
endif
endif

# ------------------------------------------------------------
# Compiler flags
# ------------------------------------------------------------

COMMON_CFLAGS := \
	-ffreestanding \
	-fno-stack-protector \
	-fno-pic \
	-fno-pie \
	-fno-exceptions \
	-fno-rtti \
	-mno-red-zone \
	-mno-sse \
	-mno-sse2 \
	-mno-mmx \
	-mno-avx \
	-m64 \
	-Wall \
	-Wextra \
	-I$(PROJECT)

COMMON_CXXFLAGS := \
	$(COMMON_CFLAGS) \
	-std=c++20

COMMON_ASFLAGS := \
	--64

KERNEL_LDFLAGS := \
	-T $(EFI_LDS) \
	-shared \
	-Bsymbolic \
	-nostdlib

EFI_CPPFLAGS := \
	-I$(EFI_INCL) \
	-I$(EFI_INCL_X86)

# ------------------------------------------------------------
# Directories
# ------------------------------------------------------------

BUILD_DIR    := build
OBJ_DIR      := $(BUILD_DIR)/obj
EFI_DIR      := $(BUILD_DIR)/efi
ISO_DIR      := $(BUILD_DIR)/iso

KERNEL_DIRS  := \
	arch \
	kernel \
	fs \
	drivers \
	memory \
	device \
	lib

# ------------------------------------------------------------
# Source discovery
# ------------------------------------------------------------

CPP_SOURCES := $(shell find arch kernel fs drivers memory device lib \
	-type f \( -name '*.cpp' -o -name '*.cc' \) 2>/dev/null)

C_SOURCES := $(shell find . \
	-path './build' -prune -o \
	-path './ports/lua' -prune -o \
	-path './userspace' -prune -o \
	-type f -name '*.c' -print 2>/dev/null)

ASM_SOURCES := $(shell find arch \
	-type f \( -name '*.S' -o -name '*.s' \) 2>/dev/null)

# Lua embedded ELF source is intentionally included by fs/*.cpp
# through fs/lua_elf.cpp.

CPP_OBJECTS := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(CPP_SOURCES))
C_OBJECTS   := $(patsubst %.c,$(OBJ_DIR)/%.o,$(C_SOURCES))
ASM_OBJECTS := $(patsubst %.S,$(OBJ_DIR)/%.o,$(filter %.S,$(ASM_SOURCES))) \
               $(patsubst %.s,$(OBJ_DIR)/%.o,$(filter %.s,$(ASM_SOURCES)))

OBJECTS := $(CPP_OBJECTS) $(C_OBJECTS) $(ASM_OBJECTS)

# ------------------------------------------------------------
# Lua
# ------------------------------------------------------------

LUA_DIR      := ports/lua
LUA_BUILD    := $(LUA_DIR)/build
LUA_ELF      := $(LUA_BUILD)/lua
LUA_SCRIPT   := $(LUA_DIR)/build-lua.sh

# ------------------------------------------------------------
# EFI output
# ------------------------------------------------------------

EFI_TARGET := $(EFI_DIR)/BOOTX64.EFI

# ------------------------------------------------------------
# Link objects
# ------------------------------------------------------------

KERNEL_OBJECTS := $(OBJECTS)

# ------------------------------------------------------------
# Default target
# ------------------------------------------------------------

.PHONY: all
all: $(EFI_TARGET)

# ------------------------------------------------------------
# Directories
# ------------------------------------------------------------

$(BUILD_DIR):
	mkdir -p $@

$(OBJ_DIR):
	mkdir -p $@

$(EFI_DIR):
	mkdir -p $@

$(ISO_DIR):
	mkdir -p $@

# ------------------------------------------------------------
# Lua build
# ------------------------------------------------------------

.PHONY: lua

lua: $(LUA_ELF)

$(LUA_ELF): $(LUA_SCRIPT)
	@echo "[LUA] building real Lua userspace binary"
	@if [ -z "$(USERLIBC)" ]; then \
		echo ""; \
		echo "ERROR: BlockOS userspace libc sysroot was not found."; \
		echo "Set it explicitly with:"; \
		echo "  make USERLIBC=/path/to/blockos/libc"; \
		echo ""; \
		exit 2; \
	fi
	@if [ ! -d "$(USERLIBC)" ]; then \
		echo "ERROR: USERLIBC does not exist: $(USERLIBC)"; \
		exit 2; \
	fi
	@USERLIBC="$(USERLIBC)" \
	CC="$(CC)" \
	AR="$(AR)" \
	RANLIB="$(AR) -s" \
	$(LUA_SCRIPT)

# ------------------------------------------------------------
# Embed Lua into RAMFS
# ------------------------------------------------------------

fs/lua_elf.cpp: $(LUA_ELF)

$(OBJ_DIR)/fs/lua_elf.o: $(LUA_ELF)

# ------------------------------------------------------------
# Generic C++ compilation
# ------------------------------------------------------------

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@echo "[CXX] $<"
	$(CXX) $(COMMON_CXXFLAGS) $(EFI_CPPFLAGS) -c "$<" -o "$@"

# ------------------------------------------------------------
# Generic C compilation
# ------------------------------------------------------------

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "[CC ] $<"
	$(CC) $(COMMON_CFLAGS) $(EFI_CPPFLAGS) -c "$<" -o "$@"

# ------------------------------------------------------------
# Assembly compilation
# ------------------------------------------------------------

$(OBJ_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	@echo "[AS ] $<"
	$(CC) $(COMMON_CFLAGS) -c "$<" -o "$@"

$(OBJ_DIR)/%.o: %.s
	@mkdir -p $(dir $@)
	@echo "[AS ] $<"
	$(AS) $(COMMON_ASFLAGS) "$<" -o "$@"

# ------------------------------------------------------------
# UEFI link
# ------------------------------------------------------------

$(EFI_TARGET): lua $(OBJECTS) | $(EFI_DIR)
	@echo "[LD ] BlockOS UEFI kernel"
	$(CXX) \
		$(KERNEL_LDFLAGS) \
		-o "$@" \
		$(KERNEL_OBJECTS) \
		-L$(EFI_LIB) \
		-L$(EFI_CRT) \
		-lefi \
		-lgnuefi

	@echo "[OK ] $(EFI_TARGET)"

# ------------------------------------------------------------
# Copy EFI to standard location
# ------------------------------------------------------------

.PHONY: efi

efi: $(EFI_TARGET)
	@mkdir -p EFI/BOOT
	cp -f "$(EFI_TARGET)" EFI/BOOT/BOOTX64.EFI
	@echo "[EFI] EFI/BOOT/BOOTX64.EFI ready"

# ------------------------------------------------------------
# ISO
# ------------------------------------------------------------

ISO_IMAGE := $(BUILD_DIR)/blockos.iso

.PHONY: iso

iso: $(EFI_TARGET)
	@echo "[ISO] creating BlockOS ISO"
	rm -rf "$(ISO_DIR)"
	mkdir -p "$(ISO_DIR)/EFI/BOOT"
	cp -f "$(EFI_TARGET)" "$(ISO_DIR)/EFI/BOOT/BOOTX64.EFI"

	grub-mkrescue \
		-o "$(ISO_IMAGE)" \
		"$(ISO_DIR)"

	@echo "[OK ] $(ISO_IMAGE)"

# ------------------------------------------------------------
# Ring3 test
# ------------------------------------------------------------

RING3_TEST := userspace/tests/ring3_test

.PHONY: ring3-test

ring3-test:
	@if [ -x userspace/tests/build-ring3-test.sh ]; then \
		echo "[RING3] building userspace test"; \
		userspace/tests/build-ring3-test.sh; \
	else \
		echo "[WARN] userspace/tests/build-ring3-test.sh not found"; \
	fi

# ------------------------------------------------------------
# QEMU
# ------------------------------------------------------------

OVMF_CODE ?= /usr/share/OVMF/OVMF_CODE_4M.fd
QEMU      ?= qemu-system-x86_64

.PHONY: run

run: iso
	@echo "[QEMU] starting BlockOS"
	$(QEMU) \
		-machine q35 \
		-m 512M \
		-bios "$(OVMF_CODE)" \
		-drive format=raw,file="$(ISO_IMAGE)"

# ------------------------------------------------------------
# Clean
# ------------------------------------------------------------

.PHONY: clean

clean:
	rm -rf "$(BUILD_DIR)"
	rm -f "$(LUA_ELF)"
	find . -type f \( -name '*.o' -o -name '*.EFI' \) -delete

# ------------------------------------------------------------
# Distclean
# ------------------------------------------------------------

.PHONY: distclean

distclean: clean
	rm -rf EFI/BOOT
	rm -rf "$(LUA_BUILD)"

# ------------------------------------------------------------
# Information
# ------------------------------------------------------------

.PHONY: info

info:
	@echo "=============================================="
	@echo " BlockOS Build Information"
	@echo "=============================================="
	@echo "Project   : $(PROJECT)"
	@echo "Compiler  : $(CXX)"
	@echo "C compiler: $(CC)"
	@echo "Lua       : $(LUA_ELF)"
	@echo "USERLIBC  : $(USERLIBC)"
	@echo "EFI INCL  : $(EFI_INCL)"
	@echo "EFI X86   : $(EFI_INCL_X86)"
	@echo "EFI LIB   : $(EFI_LIB)"
	@echo "=============================================="

# ------------------------------------------------------------
# Help
# ------------------------------------------------------------

.PHONY: help

help:
	@echo ""
	@echo "BlockOS build targets:"
	@echo ""
	@echo "  make              Build BlockOS UEFI binary"
	@echo "  make lua          Build real Lua userspace binary"
	@echo "  make efi          Copy BOOTX64.EFI to EFI/BOOT"
	@echo "  make iso          Build blockos.iso"
	@echo "  make run          Build ISO and start QEMU"
	@echo "  make ring3-test   Build Ring3 test program"
	@echo "  make info         Show build configuration"
	@echo "  make clean        Remove build files"
	@echo "  make distclean    Remove all generated files"
	@echo ""
	@echo "Manual libc path:"
	@echo "  make USERLIBC=/path/to/blockos/libc"
	@echo ""

# ------------------------------------------------------------
# Dependency rules
# ------------------------------------------------------------

.PHONY: FORCE
FORCE:
