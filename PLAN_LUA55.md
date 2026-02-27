# Plan: Make Lua 5.5.0 a Lua 5.1 API Drop-in Replacement

## Key Insight

**Lua 5.5 uses `lua55_` prefix for all exports** — types, functions, and structures all have `lua55_` prefix in the headers (e.g., `lua55_State`, `lua55_pushnumber`, `lua55L_Buffer`). This means:

1. No naming conflicts between lua51 and lua55 APIs in the same compilation unit
2. `lua55_compat.cpp` can safely include lua55 headers with `-Ilua55`
3. The compat layer must implement **all lua51 API** by wrapping/redirecting to lua55_* functions

---

## Principle

**`compat_tests/main.c` includes ONLY standard Lua 5.1 header names** (`lua.h`, `lauxlib.h`, `lualib.h`) without any `#ifdef` or compat-specific includes. Both builds use the same headers (`-Ilua51/src`). The only difference is which libraries are linked.

- **Lua 5.1 build:** `-Ilua51/src`, link `liblua.a` from lua51
- **Lua 5.5 build:** `-Ilua51/src`, link `libcompat55.a` + `lua55/liblua.a`

---

## Phase 1: Build Static Library

### 1a. Build Lua 5.1 as static library (already done)
- Run `make lua51-lib` — produces `lua51/src/liblua.a`

### 1b. Build Lua 5.5 as static library
- Run `make lua55-lib` — produces `lua55/liblua.a`
- Verify `.a` file exists and contains expected symbols

---

## Phase 2: Identify API Differences

### Types needing lua51-compatible definitions in compat layer

| Lua 5.1 Type | Lua 5.5 Equivalent | Notes |
|--------------|-------------------|-------|
| `lua_State` | `lua55_State` | Same struct, different name |
| `lua_Debug` | `lua55_Debug` | Different struct layout |
| `lua_Number` | `lua55_Number` | Same (double by default) |
| `lua_Integer` | `lua55_Integer` | Same (int64_t by default) |
| `lua_CFunction` | `lua55_CFunction` | Same signature |
| `lua_KFunction` | `lua55_KFunction` | Same signature |
| `lua_Reader` | `lua55_Reader` | Same signature |
| `lua_Writer` | `lua55_Writer` | Same signature |
| `lua_Alloc` | `lua55_Alloc` | Same signature |
| `luaL_Buffer` | `lua55L_Buffer` | Different struct layout |
| `luaL_Reg` | `lua55L_Reg` | Same struct layout |

### Constants differing between versions

| Category | Lua 5.1 | Lua 5.5 | Action |
|----------|---------|---------|--------|
| Version | 5.1 | 5.5 | N/A (we want 5.1 behavior) |
| Type constants | 0-8 | Same | Same values |
| `lua_upvalueindex` | Same | Same | Use lua55 version |
| `LUA_GLOBALSINDEX` | -10002 | Removed | Need compat macro |
| `LUA_ENVIRONINDEX` | -10001 | Removed | Need compat macro |
| `LUA_REGISTRYINDEX` | -10000 | Different value | Need compat macro |

### Functions with different signatures

| Lua 5.1 | Lua 5.5 | Wrapper Required |
|---------|---------|------------------|
| `lua_newstate(f, ud)` | `lua55_newstate(f, ud, seed)` | Pass default seed |
| `lua_pushcclosure(L, fn, n)` | `lua55_pushcclosure(L, fn, n)` | Same signature |
| `lua_resume(L, narg)` | `lua55_resume(L, from, narg, nres)` | Pass NULL for `from` |
| `lua_load(L, reader, dt, chunkname)` | `lua55_load(L, reader, dt, chunkname, mode)` | Pass NULL for `mode` |
| `lua_dump(L, writer, data)` | `lua55_dump(L, writer, data, strip)` | Pass 0 for `strip` |
| `lua_gc(L, what, data)` | `lua55_gc(L, what, ...)` | Different varargs |
| `lua_getinfo(L, what, ar)` | `lua55_getinfo(L, what, ar)` | Different `lua_Debug` struct |
| `lua_rawgeti(L, idx, n)` | `lua55_rawgeti(L, idx, n)` | Same signature |
| `lua_rawseti(L, idx, n)` | `lua55_rawseti(L, idx, n)` | Same signature |
| `lua_newuserdata(sz)` | `lua55_newuserdatauv(L, sz, nuvalue)` | Pass 1 for nuvalue |
| `lua_getfenv(idx)` | **Removed** | Need wrapper |
| `lua_setfenv(idx)` | **Removed** | Need wrapper |

### Functions removed in Lua 5.5 (need compat implementations)

| Function | Replacement in 5.5 | Notes |
|----------|-------------------|-------|
| `lua_open()` | `lua55L_newstate()` | Macro in 5.1 |
| `luaL_register(L, libname, l)` | `lua55L_newlib()` | Different approach |
| `luaI_openlib` | `lua55L_newlib()` | Deprecated |
| `lua_getlocal` | Same | Same |
| `lua_setlocal` | Same | Same |

### Functions that are macros in 5.1 but real functions in 5.5

- `lua_pop(L, n)` — macro in 5.1, `lua55_settop()` in 5.5
- `lua_newtable(L)` — macro in 5.1, `lua55_createtable()` in 5.5
- `lua_register(L, n, f)` — macro in 5.1
- `lua_pushcfunction(L, f)` — macro in 5.1
- `lua_strlen(L, i)` — macro in 5.1, use `lua55_objlen()`
- `lua_isfunction`, `lua_istable`, etc. — macros in 5.1
- `lua_getglobal(L, s)` — macro in 5.1
- `lua_setglobal(L, s)` — macro in 5.1

### Debug API differences

`lua_Debug` struct in 5.1:
```c
struct lua_Debug {
  int event;
  const char *name;
  const char *namewhat;
  const char *what;
  const char *source;
  int currentline;
  int nups;
  int linedefined;
  int lastlinedefined;
  char short_src[LUA_IDSIZE];
  int i_ci;  /* private */
};
```

`lua55_Debug` struct in 5.5:
```c
struct lua55_Debug {
  int event;
  const char *name;
  const char *namewhat;
  const char *what;
  const char *source;
  size_t srclen;
  int currentline;
  int linedefined;
  int lastlinedefined;
  unsigned char nups;
  unsigned char nparams;
  char isvararg;
  unsigned char extraargs;
  char istailcall;
  int ftransfer;
  int ntransfer;
  char short_src[LUA_IDSIZE];
  struct CallInfo *i_ci;  /* private, different type */
};
```

**Impact:** Need wrapper/adapter for `lua_getinfo()` that handles field mapping.

### Buffer API differences

`luaL_Buffer` in 5.1:
```c
typedef struct luaL_Buffer {
  char *p;
  int lvl;
  lua_State *L;
  char buffer[LUAL_BUFFERSIZE];
} luaL_Buffer;
```

`lua55L_Buffer` in 5.5:
```c
typedef struct lua55L_Buffer {
  char *b;
  size_t size;
  size_t n;
  lua55_State *L;
  union {
    LUAI_MAXALIGN;
    char b[LUAL_BUFFERSIZE];
  } init;
} lua55L_Buffer;
```

---

## Phase 3: Create Compat Layer

### Architecture

```
compat/
├── lua55_compat.cpp   # Compiled with lua55 headers (-Ilua55)
└── libcompat55.a      # Built compat library
```

### Build Command

```bash
# Compile lua55_compat.cpp with lua55 headers
g++ -c -Ilua55 -Ilua51/src -DLUA_COMPAT compat/lua55_compat.cpp -o compat/lua55_compat.o
ar rcs compat/libcompat55.a compat/lua55_compat.o
```

### Implementation Details

#### 1. Type aliases (compatible with lua51)
```cpp
// In lua55_compat.cpp
typedef lua55_State lua_State;
typedef lua55_Debug lua_Debug;
typedef lua55_Number lua_Number;
typedef lua55_Integer lua_Integer;
typedef lua55_Unsigned lua_Unsigned;
typedef lua55_CFunction lua_CFunction;
typedef lua55_KFunction lua_KFunction;
typedef lua55_Reader lua_Reader;
typedef lua55_Writer lua_Writer;
typedef lua55_Alloc lua_Alloc;
typedef lua55_WarnFunction lua_WarnFunction;
typedef lua55_Hook lua_Hook;

// lauxlib types
typedef lua55L_Buffer luaL_Buffer;
typedef lua55L_Reg luaL_Reg;
typedef lua55L_Stream luaL_Stream;
```

#### 2. Constants
```cpp
#define LUA_GLOBALSINDEX (-10002)
#define LUA_ENVIRONINDEX (-10001)
#define lua_upvalueindex(i) lua55_upvalueindex(i)
```

#### 3. Functions to implement/wrap

**State management:**
- `lua_newstate` → call `lua55_newstate(f, ud, 0)`
- `lua_close` → call `lua55_close`
- `lua_newthread` → call `lua55_newthread`

**Stack operations:**
- `lua_gettop` → `lua55_gettop`
- `lua_settop` → `lua55_settop`
- `lua_pushvalue` → `lua55_pushvalue`
- `lua_rotate` → `lua55_rotate` (new in 5.2, available in 5.5)
- `lua_copy` → `lua55_copy` (new in 5.2, available in 5.5)
- `lua_checkstack` → `lua55_checkstack`
- `lua_xmove` → `lua55_xmove`

**Push functions:**
- `lua_pushnil` → `lua55_pushnil`
- `lua_pushnumber` → `lua55_pushnumber`
- `lua_pushinteger` → `lua55_pushinteger`
- `lua_pushlstring` → `lua55_pushlstring`
- `lua_pushstring` → `lua55_pushstring`
- `lua_pushvfstring` → `lua55_pushvfstring`
- `lua_pushfstring` → `lua55_pushfstring`
- `lua_pushcclosure` → `lua55_pushcclosure`
- `lua_pushboolean` → `lua55_pushboolean`
- `lua_pushlightuserdata` → `lua55_pushlightuserdata`
- `lua_pushthread` → `lua55_pushthread`

**Access functions:**
- `lua_isnumber` → `lua55_isnumber`
- `lua_isstring` → `lua55_isstring`
- `lua_iscfunction` → `lua55_iscfunction`
- `lua_isinteger` → `lua55_isinteger`
- `lua_isuserdata` → `lua55_isuserdata`
- `lua_type` → `lua55_type`
- `lua_typename` → `lua55_typename`
- `lua_equal` → `lua55_compare(L, idx1, idx2, LUA_OPEQ)`
- `lua_rawequal` → `lua55_rawequal`
- `lua_lessthan` → `lua55_compare(L, idx1, idx2, LUA_OPLT)`
- `lua_tonumberx` → `lua55_tonumberx`
- `lua_tointegerx` → `lua55_tointegerx`
- `lua_toboolean` → `lua55_toboolean`
- `lua_tolstring` → `lua55_tolstring`
- `lua_objlen` → `(size_t)lua55_rawlen`
- `lua_rawlen` → `lua55_rawlen`
- `lua_tocfunction` → `lua55_tocfunction`
- `lua_touserdata` → `lua55_touserdata`
- `lua_tothread` → `lua55_tothread`
- `lua_topointer` → `lua55_topointer`

**Get functions:**
- `lua_gettable` → `lua55_gettable`
- `lua_getfield` → `lua55_getfield`
- `lua_rawget` → `lua55_rawget`
- `lua_rawgeti` → `lua55_rawgeti`
- `lua_createtable` → `lua55_createtable`
- `lua_newuserdata` → `lua55_newuserdatauv(L, sz, 1)`
- `lua_getmetatable` → `lua55_getmetatable`
- `lua_getfenv` → **stub needed** (removed in 5.5)

**Set functions:**
- `lua_settable` → `lua55_settable`
- `lua_setfield` → `lua55_setfield`
- `lua_rawset` → `lua55_rawset`
- `lua_rawseti` → `lua55_rawseti`
- `lua_setmetatable` → `lua55_setmetatable`
- `lua_setfenv` → **stub needed** (removed in 5.5)

**Call functions:**
- `lua_call` → `lua55_call` (macro)
- `lua_pcall` → `lua55_pcall` (macro)
- `lua_cpcall` → wrap with `lua55_pcallk`
- `lua_load` → `lua55_load` with mode=NULL
- `lua_dump` → `lua55_dump` with strip=0

**Coroutine functions:**
- `lua_yield` → `lua55_yield` (macro)
- `lua_resume` → `lua55_resume(L, NULL, narg, &nres)`
- `lua_status` → `lua55_status`
- `lua_isyieldable` → `lua55_isyieldable`

**GC functions:**
- `lua_gc` → wrap varargs to `lua55_gc`

**Misc functions:**
- `lua_error` → `lua55_error`
- `lua_next` → `lua55_next`
- `lua_concat` → `lua55_concat`
- `lua_len` → `lua55_len`
- `lua_getallocf` → `lua55_getallocf`
- `lua_setallocf` → `lua55_setallocf`

**Debug API:**
- `lua_getstack` → `lua55_getstack`
- `lua_getinfo` → `lua55_getinfo` (different Debug struct!)
- `lua_getlocal` → `lua55_getlocal`
- `lua_setlocal` → `lua55_setlocal`
- `lua_getupvalue` → `lua55_getupvalue`
- `lua_setupvalue` → `lua55_setupvalue`
- `lua_sethook` → `lua55_sethook`
- `lua_gethook` → `lua55_gethook`
- `lua_gethookmask` → `lua55_gethookmask`
- `lua_gethookcount` → `lua55_gethookcount`

**lauxlib functions:**
- `luaL_newstate` → `lua55L_newstate`
- `luaL_register` → implement using `lua55_createtable` + `lua55L_setfuncs`
- `luaL_checkversion_` → `lua55L_checkversion_`
- `luaL_getmetafield` → `lua55L_getmetafield`
- `luaL_callmeta` → `lua55L_callmeta`
- `luaL_typename` → `lua55_typename`
- `luaL_error` → `lua55L_error`
- `luaL_checkinteger` → `lua55L_checkinteger`
- `luaL_optinteger` → `lua55L_optinteger`
- `luaL_checknumber` → `lua55L_checknumber`
- `luaL_optnumber` → `lua55L_optnumber`
- `luaL_checklstring` → `lua55L_checklstring`
- `luaL_optlstring` → `lua55L_optlstring`
- `luaL_checkstack` → `lua55L_checkstack`
- `luaL_checktype` → `lua55L_checktype`
- `luaL_checkany` → `lua55L_checkany`
- `luaL_where` → `lua55L_where`
- `luaL_ref` → `lua55L_ref`
- `luaL_unref` → `lua55L_unref`
- `luaL_newmetatable` → `lua55L_newmetatable`
- `luaL_setmetatable` → `lua55L_setmetatable`
- `luaL_testudata` → `lua55L_testudata`
- `luaL_checkudata` → `lua55L_checkudata`
- `luaL_checkoption` → `lua55L_checkoption`
- `luaL_loadfilex` → `lua55L_loadfilex`
- `luaL_loadfile` → `lua55L_loadfile` (macro)
- `luaL_loadbufferx` → `lua55L_loadbufferx`
- `luaL_loadbuffer` → `lua55L_loadbuffer` (macro)
- `luaL_loadstring` → `lua55L_loadstring`
- `luaL_gsub` → `lua55L_gsub`
- `luaL_addgsub` → `lua55L_addgsub`
- `luaL_setfuncs` → `lua55L_setfuncs`
- `luaL_getsubtable` → `lua55L_getsubtable`
- `luaL_traceback` → `lua55L_traceback`
- `luaL_requiref` → `lua55L_requiref`

**Buffer functions:**
- `luaL_buffinit` → `lua55L_buffinit` (different struct layout!)
- `luaL_prepbuffsize` → `lua55L_prepbuffsize`
- `luaL_addlstring` → `lua55L_addlstring`
- `luaL_addstring` → `lua55L_addstring`
- `luaL_addvalue` → `lua55L_addvalue`
- `luaL_pushresult` → `lua55L_pushresult`
- `luaL_pushresultsize` → `lua55L_pushresultsize`
- `luaL_buffinitsize` → `lua55L_buffinitsize`

**Library open functions:**
- `luaL_openlibs` → implement or wrap `lua55L_requiref` calls
- Individual `luaopen_*` functions — redirect to lua55 versions

---

## Phase 4: Update Root Makefile

Add new targets:
```makefile
LUA55_DIR = lua55
LUA55_LIB = $(LUA55_DIR)/liblua.a

lua55-lib:
	$(MAKE) -C $(LUA55_DIR) a MYCFLAGS="-DLUA_USE_LINUX"

compat55-lib: lua55-lib
	g++ -c -I$(LUA55_DIR) -I$(LUA51_SRC) compat/lua55_compat.cpp -o compat/lua55_compat.o
	ar rcs compat/libcompat55.a compat/lua55_compat.o

compat-test-lua55: lua55-lib compat55-lib
	$(CC) $(CFLAGS) -I$(LUA51_SRC) compat_tests/main.c $(COMPAT55_LIB) $(LUA55_LIB) -lm -ldl -o compat_tests/test_lua55
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
│   └── src/          # Headers and source
├── lua55/            # Lua 5.5 sources (prefixed)
│   ├── lua.h         # lua55_State, lua55_* functions
│   ├── lauxlib.h     # lua55L_* functions
│   ├── lualib.h
│   └── liblua.a     # Built static library
├── luau/             # Luau sources
├── compat/
│   ├── luau_bridge.cpp      # Luau compat
│   ├── lua51_compat.cpp     # Lua 5.1 compat for Luau
│   ├── lua55_compat.cpp     # Lua 5.5 compat for 5.1 API
│   ├── libcompat.a          # Luau compat lib
│   └── libcompat55.a        # Lua 5.5 compat lib
└── compat_tests/
    └── main.c        # Test app using ONLY Lua 5.1 headers
```

---

## Implementation Checklist

- [ ] Build lua55 static library
- [ ] Create/update `lua55_compat.cpp` with all wrappers
- [ ] Build compat55 library
- [ ] Test with example code
- [ ] Fix any API mismatches
- [ ] Verify test_lua51 and test_lua55 produce same output
