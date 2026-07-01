# Simple Makefile for building a GNU-EFI based EFI application

# You may need to adjust these include and library paths depending on your distribution.
EFI_INCL = /usr/include/efi
EFI_INCL_X86 = /usr/include/efi/x86_64
GNU_EFI_LIBDIR = /usr/lib
GNU_EFI_LDS = $(GNU_EFI_LIBDIR)/gnu-efi/elf_x86_64_efi.lds

CXX = g++
LD = ld
OBJCOPY = objcopy

CXXFLAGS = -fno-exceptions -fno-rtti -fshort-wchar -DEFI_FUNCTION_WRAPPER -I$(EFI_INCL) -I$(EFI_INCL_X86) -ffreestanding -O2 -Wall -Wextra
LDFLAGS = -nostdlib -znocombreloc -T $(GNU_EFI_LDS)

SRC = src/kernel.cpp src/fb.cpp
OBJ = $(SRC:.cpp=.o)

BUILD_DIR = build
EFI_OUT = $(BUILD_DIR)/BOOTX64.EFI
SO_OUT = $(BUILD_DIR)/kernel.so

all: $(EFI_OUT)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(SO_OUT): $(OBJ)
	$(LD) $(LDFLAGS) -shared -Bsymbolic -L$(GNU_EFI_LIBDIR) $(OBJ) -o $(SO_OUT)

$(EFI_OUT): $(SO_OUT)
	mkdir -p $(BUILD_DIR)
	$(OBJCOPY) -j .text -j .sdata -j .data -j .dynamic -j .dynsym --target=efi-app-x86_64 $(SO_OUT) $(EFI_OUT)

clean:
	rm -rf $(OBJ) $(BUILD_DIR)

.PHONY: all clean
