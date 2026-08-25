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
	-MMD \
	-MP



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
	libc/src

S_SRC_DIRS := \
	drivers \
	examples \
	fs \
	kernel

# ============================================================
# SOURCE FILES
# ============================================================

# Sources excluded from the kernel build.
#
# Hosted C++ (libstdc++, libsodium) written against interfaces the kernel does
# not provide; must be ported before they can compile -ffreestanding -nostdlib:
#   fs/EROFS.cpp, fs/tmpfs.cpp, kernel/login.cpp
#
# Depends on blockos_terminal_write/_hex, which nothing defines yet. Currently
# orphaned - nothing calls into it - so it is parked until there is a console
# or serial backend to print through:
#   kernel/panic.cpp
EXCLUDED_SRC := \
	fs/EROFS.cpp \
	fs/tmpfs.cpp \
	kernel/login.cpp \
	kernel/panic.cpp
	
SRC := $(filter-out $(EXCLUDED_SRC), \
	$(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.cpp)))

SRC += kernel/cmd/cmd_ata.cpp
SRC += kernel/cmd/cmd_forth.cpp
SRC += kernel/cmd/cmd_ls.cpp
SRC += kernel/cmd/command_registry.cpp
SRC += kernel/cmd/cmd_clear.cpp
SRC += kernel/cmd/cmd_help.cpp

S_SRC := $(foreach dir,$(S_SRC_DIRS),$(wildcard $(dir)/*.S))

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
# DEFAULT
# ============================================================

all: $(EFI_OUT)

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
# WEAKENED GNU-EFI LIB
# Weaken gnu-efi's memcpy and memset so our libc wins when linking kernel.so.
# ============================================================

EFI_LIB_WEAK := $(BUILD_DIR)/libefi-weak.a

$(EFI_LIB_WEAK): $(EFI_LIB)
	@mkdir -p $(BUILD_DIR)
	@echo "[OBJCOPY] weakening memcpy/memset in $(notdir $(EFI_LIB))"
	cp $< $@
	$(OBJCOPY) --weaken-symbol=memcpy --weaken-symbol=memset $@

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
		--target=efi-app-x86_64 \
		$(SO_OUT) \
		$(EFI_OUT)

	@echo ""
	@echo "[OK] $@"

# ============================================================
# CHECK GNU-EFI
# ============================================================

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

menuconfig:
	$(PYTHON) scripts/menuconfig.py

# ============================================================
# CLEAN
# ============================================================

clean:
	rm -f $(OBJ)
	rm -f $(DEP)
	rm -rf $(BUILD_DIR)

# ============================================================
# REBUILD
# ============================================================

rebuild:
	$(MAKE) clean
	$(MAKE) all

# ============================================================
# RUN
# ============================================================

run: all
	sh build_and_run.sh

# ============================================================
# PHONY
# ============================================================

.PHONY: all clean rebuild menuconfig check-efi run

# ============================================================
# HEADER DEPENDENCIES
# Must come after all rules
# ============================================================

-include $(DEP)
