CC = gcc
CXX = g++
CFLAGS = -Wall -Wno-unused -g
CFLAGS_RELEASE = $(CFLAGS) -O2 -DNDEBUG

LUA51_DIR = lua51
LUA51_SRC = $(LUA51_DIR)/src
LUA51_LIB = $(LUA51_SRC)/liblua.a

LUA55_DIR = lua55
LUA55_LIB = $(LUA55_DIR)/liblua.a
LUA55_CLI = $(LUA55_DIR)/lua

LUAU_DIR = luau
LUAU_BUILD = $(LUAU_DIR)/build/release
LUAU_VM_LIB = $(LUAU_BUILD)/libluauvm.a
LUAU_COMPILER_LIB = $(LUAU_BUILD)/libluaucompiler.a
LUAU_AST_LIB = $(LUAU_BUILD)/libluauast.a
LUAU_COMMON_LIB = $(LUAU_BUILD)/libluaucommon.a
LUAU_LIBS = $(LUAU_VM_LIB) $(LUAU_COMPILER_LIB) $(LUAU_AST_LIB) $(LUAU_COMMON_LIB)
LUAU_RUNTIME_LIBS = $(LUAU_VM_LIB) $(LUAU_COMMON_LIB)

LUAU_COMPILE = $(LUAU_DIR)/build/release/luau-compile

COMPAT_DIR = compat
COMPAT_LIB = $(COMPAT_DIR)/libcompat.a
COMPAT_RUNTIME_LIB = $(COMPAT_DIR)/libcompat_runtime.a

.PHONY: all lua51-lib lua55-lib lua55 luau-lib compat-lib compat-runtime-lib \
        example-lua51 example-luau example-luau-runtime precompile clean

all: example-lua51 example-luau precompile example-luau-runtime

lua51-lib:
	$(MAKE) -C $(LUA51_SRC) a MYCFLAGS="-DLUA_USE_LINUX"

lua55-lib:
	$(MAKE) -C $(LUA55_DIR) a MYCFLAGS="-DLUA_USE_LINUX"

lua55: lua55-lib
	$(MAKE) -C $(LUA55_DIR) lua

luau-lib:
	$(MAKE) -C $(LUAU_DIR) config=release

compat-lib: luau-lib
	$(CXX) $(CFLAGS_RELEASE) -c \
		-I$(LUAU_DIR)/VM/include \
		-I$(LUAU_DIR)/Compiler/include \
		-I$(LUAU_DIR)/Common/include \
		$(COMPAT_DIR)/luau_bridge.cpp \
		-o $(COMPAT_DIR)/luau_bridge.o
	$(CXX) $(CFLAGS_RELEASE) -c \
		-I$(LUA51_SRC) \
		$(COMPAT_DIR)/lua51_compat.cpp \
		-o $(COMPAT_DIR)/lua51_compat.o
	ar rcs $(COMPAT_LIB) $(COMPAT_DIR)/luau_bridge.o $(COMPAT_DIR)/lua51_compat.o

example-lua51: lua51-lib
	$(CC) $(CFLAGS) -I$(LUA51_SRC) example/main.c $(LUA51_LIB) -lm -ldl -o example/test_lua51

example-luau: compat-lib
	$(CXX) $(CFLAGS_RELEASE) -I$(LUA51_SRC) example/main.c $(COMPAT_LIB) $(LUAU_LIBS) -lm -lpthread -o example/test_luau

# Runtime-only compat (no compiler/AST)
compat-runtime-lib: luau-lib
	$(CXX) $(CFLAGS_RELEASE) -DLUAU_RUNTIME_ONLY -c \
		-I$(LUAU_DIR)/VM/include \
		-I$(LUAU_DIR)/Common/include \
		$(COMPAT_DIR)/luau_bridge.cpp \
		-o $(COMPAT_DIR)/luau_bridge_rt.o
	$(CXX) $(CFLAGS_RELEASE) -DLUAU_RUNTIME_ONLY -c \
		-I$(LUA51_SRC) \
		$(COMPAT_DIR)/lua51_compat.cpp \
		-o $(COMPAT_DIR)/lua51_compat_rt.o
	ar rcs $(COMPAT_RUNTIME_LIB) $(COMPAT_DIR)/luau_bridge_rt.o $(COMPAT_DIR)/lua51_compat_rt.o

# Precompile all .lua to .luac bytecode
precompile: luau-lib
	@echo "Precompiling Lua files to bytecode..."
	@find example/tests -name '*.lua' | while read f; do \
		$(LUAU_COMPILE) --binary -O2 "$$f" > "$${f}c" && \
		echo "  $$f -> $${f}c"; \
	done
	@find example/shims -name '*.lua' | while read f; do \
		$(LUAU_COMPILE) --binary -O2 "$$f" > "$${f}c" && \
		echo "  $$f -> $${f}c"; \
	done

example-luau-runtime: compat-runtime-lib precompile
	$(CXX) $(CFLAGS_RELEASE) -DLUAU_RUNTIME_ONLY -I$(LUA51_SRC) example/main.c \
		$(COMPAT_RUNTIME_LIB) $(LUAU_RUNTIME_LIBS) -lm -lpthread -o example/test_luau_runtime

clean:
	$(MAKE) -C $(LUA51_DIR) clean
	$(MAKE) -C $(LUA55_DIR) clean
	$(MAKE) -C $(LUAU_DIR) clean
	rm -f $(COMPAT_DIR)/*.o $(COMPAT_LIB) $(COMPAT_RUNTIME_LIB)
	rm -f example/test_lua51 example/test_lua51 example/test_luau example/test_luau_runtime
	find example/tests example/shims -name '*.luac' -delete 2>/dev/null || true
