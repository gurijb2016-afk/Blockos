# BlockOS GNU-EFI C++ Build System

EFI_INCL = /usr/include/efi
EFI_INCL_X86 = /usr/include/efi/x86_64
GNU_EFI_LIBDIR = /usr/lib
GNU_EFI_LDS = $(GNU_EFI_LIBDIR)/gnu-efi/elf_x86_64_efi.lds

CXX = g++
LD = ld
OBJCOPY = objcopy
PYTHON = python3

CXXFLAGS = \
	-fno-exceptions \
	-fno-rtti \
	-fshort-wchar \
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
	-Iexamples

LDFLAGS = \
	-nostdlib \
	-znocombreloc \
	-T $(GNU_EFI_LDS)


# Forrás könyvtárak
SRC_DIRS = drivers examples fs kernel
S_SRC_DIRS = drivers examples fs kernel

# Összes cpp fájl
SRC = $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.cpp))
# Összes assembly (.S) fájl
S_SRC = $(foreach dir,$(S_SRC_DIRS),$(wildcard $(dir)/*.S))

# Objektum fájlok
OBJ = $(SRC:.cpp=.o) $(S_SRC:.S=.o)

BUILD_DIR = build

SO_OUT = $(BUILD_DIR)/kernel.so
EFI_OUT = $(BUILD_DIR)/BOOTX64.EFI


all: $(EFI_OUT)


# C++ fordítás
%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Assembly fordítás
%.o: %.S
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@


# ELF shared kernel
$(SO_OUT): $(OBJ)
	mkdir -p $(BUILD_DIR)
	$(LD) $(LDFLAGS) \
		-shared \
		-Bsymbolic \
		-L$(GNU_EFI_LIBDIR) \
		$(OBJ) \
		-o $@


# EFI image
$(EFI_OUT): $(SO_OUT)
	$(OBJCOPY) \
		-j .text \
		-j .sdata \
		-j .data \
		-j .dynamic \
		-j .dynsym \
		--target=efi-app-x86_64 \
		$(SO_OUT) \
		$(EFI_OUT)


# Menü konfiguráció
menuconfig:
	$(PYTHON) scripts/menuconfig.py


clean:
	rm -rf $(OBJ)
	rm -rf $(BUILD_DIR)


.PHONY: all clean menuconfig
