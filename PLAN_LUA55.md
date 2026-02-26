# Plan: Make Lua 5.5.0 a Lua 5.1 API Drop-in Replacement

## Principle

**Key principle:** `example/main.c` includes ONLY standard Lua 5.1 header names (`lua.h`, `lauxlib.h`, `lualib.h`) without any `#ifdef` or compat-specific includes. Both builds use the same headers (`-Ilua51/src`). The only difference is which libraries are linked.

- **Lua 5.1 build:** `-Ilua51/src`, link `liblua.a` from lua51
- **Lua 5.5 build:** `-Ilua51/src`, link `liblua.a` from lua55

---

## Phase 1: Build Static Library

### 1a. Build Lua 5.1 as static library (already done)
- Run `make lua51-lib` — produces `lua51/src/liblua.a`

### 1b. Build Lua 5.5 as static library
- Run `make lua55-lib` — produces `lua55/liblua.a`
- Need to add lua55 build target to root Makefile
- Verify `.a` file exists and contains expected symbols

---

## Phase 2: Identify API Differences

Lua 5.5 introduces many new features and API changes compared to Lua 5.1. Need to identify:
1. New functions in Lua 5.5 not in Lua 5.1
2. Removed functions from Lua 5.1
3. Changed signatures

### Key Differences (preliminary)

| Category | Lua 5.1 | Lua 5.5 | Action |
|----------|---------|---------|--------|
| Version | 5.1 | 5.5 | Compat macro |
| Type constants | 0-8 | Different (includes VECTOR,etc) | Compat macro |
| lua_open | `lua_open()` | Removed | Macro to `luaL_newstate()` |
| luaL_register | Present | Removed | Compat wrapper |
| luaL_Buffer | Different layout | Different | Adapter |
| lua_pushcclosure | `(L, f, n)` | Added extra parameter | Wrapper |
| lua_resume | `(L, narg)` | `(L, from, narg)` | Wrapper |
| lua_load | Different signature | Added chunkname param | Wrapper |
| lua_upvalueindex | Same | Same | - |
| Debug API | Level-based | Different | Check |

---

## Phase 3: Create Compat Layer

### Architecture (same as Luau compat)

```
compat/
├── lua55_bridge.cpp   # Compiled with Lua 5.5 headers (if needed)
├── lua55_compat.cpp   # Compiled with Lua 5.1 headers
└── libcompat55.a      # Built compat library
```

### Functions needing compat wrappers

#### Macros in lua55 that differ from lua51:
- `lua_pop`, `lua_newtable`, `lua_register`, `lua_pushcfunction`
- `lua_strlen`, `lua_isfunction`, `lua_istable`, etc.

#### Changed functions:
1. **lua_pushcclosure** — signature change (extra upvalue info parameter)
2. **lua_pushcfunction** — signature change (extra name parameter)
3. **lua_resume** — signature change (added `from` thread parameter)
4. **lua_load** — signature change (added chunkname parameter)
5. **lua_getinfo** — signature/behavior change

#### Removed functions needing stubs:
- `luaL_register` — removed in Lua 5.5, use `luaL_newlib` instead
- `luaI_openlib` — removed
- `luaopen_io` — may have different implementation

---

## Phase 4: Update Root Makefile

Add new targets:
```makefile
LUA55_DIR = lua55
LUA55_LIB = $(LUA55_DIR)/liblua.a

lua55-lib:
	$(MAKE) -C $(LUA55_DIR) a MYCFLAGS="-DLUA_USE_LINUX"

compat55-lib: lua55-lib
	# Build compat library for lua55

example-lua55: lua51-lib
	$(CC) $(CFLAGS) -I$(LUA51_SRC) example/main.c $(LUA51_LIB) -lm -ldl -o example/test_lua51

example-lua55: lua55-lib compat55-lib
	$(CC) $(CFLAGS) -I$(LUA51_SRC) example/main.c $(COMPAT55_LIB) $(LUA55_LIB) -lm -ldl -o example/test_lua55
```

---

## Phase 5: Build and Test

1. Build lua55 static library
2. Build compat55 library
3. Build example with lua55
4. Compare output with lua51 version
5. Iterate on compat layer until matching behavior

---

## Phase 6: File Structure (Final)

```
ext_luau/
├── PLAN.md           # Original Luau compat plan
├── PLAN_LUA55.md     # This plan
├── AGENTS.md
├── Makefile          # Updated with lua55 targets
├── lua51/            # Lua 5.1 sources
├── lua55/            # Lua 5.5 sources
├── luau/             # Luau sources
├── compat/
│   ├── luau_bridge.cpp      # Luau compat
│   ├── lua51_compat.cpp     # Lua 5.1 compat for Luau
│   ├── lua55_compat.cpp     # Lua 5.5 compat for 5.1 API
│   ├── libcompat.a          # Luau compat lib
│   └── libcompat55.a        # Lua 5.5 compat lib
└── example/
    └── main.c        # Test app using ONLY Lua 5.1 headers
```
