ROOT := $(abspath ../..)
LUA_BIN := $(ROOT)/ports/lua/build/lua

.PHONY: all clean
all: $(LUA_BIN)

$(LUA_BIN):
	@ports/lua/build-lua.sh

clean:
	rm -rf $(ROOT)/ports/lua/build
