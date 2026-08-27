CXX ?= g++
CPPFLAGS += -Iinclude -I.
CXXFLAGS += -std=c++17 -O2 -Wall -Wextra -Werror=return-type

CORE_SRC := \
    ipc/transport.cpp \
    render/framebuffer.cpp \
    input/input.cpp \
    server/server.cpp \
    server/protocol.cpp \
    libX11/display.cpp \
    libX11/extensions.cpp \
    libX11/xft.cpp \
    libxcb/xcb.cpp

HOST_OBJ := $(CORE_SRC:.cpp=.host.o)
TEST_OBJ := tests/abi_smoke.host.o
OUT := build/bx11-host-test

.PHONY: all host-test install-sysroot clean
all: host-test


build:
	mkdir -p build

%.host.o: %.cpp | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

tests/abi_smoke.host.o: tests/abi_smoke.cpp | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

host-test: $(HOST_OBJ) $(TEST_OBJ)
	$(CXX) $(HOST_OBJ) $(TEST_OBJ) -o $(OUT)
	$(OUT)

install-sysroot: host-test
	sh install-sysroot.sh

clean:
	rm -rf build $(HOST_OBJ) $(TEST_OBJ)
