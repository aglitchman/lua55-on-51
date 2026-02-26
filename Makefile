CC = gcc
CXX = g++
CFLAGS = -Wall -Wno-unused -g

LUA51_DIR = lua51
LUA51_SRC = $(LUA51_DIR)/src
LUA51_LIB = $(LUA51_SRC)/liblua.a

LUAU_DIR = luau
LUAU_BUILD = $(LUAU_DIR)/build/debug
LUAU_VM_LIB = $(LUAU_BUILD)/libluauvm.a
LUAU_COMPILER_LIB = $(LUAU_BUILD)/libluaucompiler.a
LUAU_AST_LIB = $(LUAU_BUILD)/libluauast.a
LUAU_COMMON_LIB = $(LUAU_BUILD)/libluaucommon.a

LUAU_INCLUDES = -I$(LUAU_DIR)/VM/include -I$(LUAU_DIR)/Compiler/include -I$(LUAU_DIR)/Common/include
LUAU_LIBS = $(LUAU_VM_LIB) $(LUAU_COMPILER_LIB) $(LUAU_AST_LIB) $(LUAU_COMMON_LIB)

.PHONY: all lua51-lib luau-lib example-lua51 example-luau clean

all: example-lua51

lua51-lib:
	$(MAKE) -C $(LUA51_SRC) a MYCFLAGS="-DLUA_USE_LINUX"

luau-lib:
	$(MAKE) -C $(LUAU_DIR)

example-lua51: lua51-lib
	$(CC) $(CFLAGS) -I$(LUA51_SRC) example/main.c $(LUA51_LIB) -lm -ldl -o example/test_lua51

example-luau: luau-lib
	$(CXX) $(CFLAGS) $(LUAU_INCLUDES) -Icompat example/main.c compat/lua51_compat.cpp $(LUAU_LIBS) -lm -lpthread -o example/test_luau

clean:
	$(MAKE) -C $(LUA51_DIR) clean
	$(MAKE) -C $(LUAU_DIR) clean
	rm -f example/test_lua51 example/test_luau
