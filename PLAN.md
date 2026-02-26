# Plan: Make Luau 100% Lua 5.1 C API Compatible

## Phase 1: Build Static Libraries

### 1a. Build Lua 5.1 as static library
- Run `make linux` inside `lua51/` — this produces `lua51/src/liblua.a`
- Verify the `.a` file exists and contains the expected symbols (`nm lua51/src/liblua.a`)

### 1b. Build Luau as static library
- Run `make` inside `luau/` — this produces multiple `.a` files under `luau/build/debug/`:
  - `libluauvm.a` (the core VM, most important)
  - `libluaucompiler.a` (needed for compiling source to bytecode)
  - `libluauast.a`, `libluaucommon.a` (dependencies of the compiler)
- Verify all `.a` files exist

---

## Phase 2: Create Example App Using Lua 5.1 C API (Defold Subset)

Create `example/main.c` — a single C file that exercises the Lua 5.1 C API functions **actually used by the Defold engine**. Functions not used by Defold will only have stubs in the compat layer, so there is no need to test them thoroughly — just verify they compile.

### From `lua.h` — Core API functions to exercise:

**State manipulation:**
- `lua_close`, `lua_newthread`, `lua_atpanic`

**Stack manipulation:**
- `lua_gettop`, `lua_settop`, `lua_pushvalue`, `lua_remove`, `lua_insert`, `lua_replace`, `lua_checkstack`

**Access functions (stack → C):**
- `lua_isnumber`, `lua_isstring`, `lua_iscfunction`, `lua_isuserdata`, `lua_type`, `lua_typename`
- `lua_rawequal`
- `lua_tonumber`, `lua_tointeger`, `lua_toboolean`, `lua_tolstring`, `lua_objlen`, `lua_tocfunction`, `lua_touserdata`, `lua_tothread`, `lua_topointer`

**Push functions (C → stack):**
- `lua_pushnil`, `lua_pushnumber`, `lua_pushinteger`, `lua_pushlstring`, `lua_pushstring`, `lua_pushvfstring`, `lua_pushfstring`, `lua_pushcclosure`, `lua_pushboolean`, `lua_pushlightuserdata`, `lua_pushthread`

**Get functions (Lua → stack):**
- `lua_gettable`, `lua_getfield`, `lua_rawget`, `lua_rawgeti`, `lua_createtable`, `lua_newuserdata`, `lua_getmetatable`

**Set functions (stack → Lua):**
- `lua_settable`, `lua_setfield`, `lua_rawset`, `lua_rawseti`, `lua_setmetatable`

**Load and call:**
- `lua_call`, `lua_pcall`, `lua_cpcall`

**GC:**
- `lua_gc` (STOP, RESTART, COLLECT, COUNT, COUNTB, STEP, SETPAUSE, SETSTEPMUL)

**Miscellaneous:**
- `lua_error`, `lua_next`, `lua_concat`, `lua_getallocf`

**Macros (use them to ensure they compile):**
- `lua_pop`, `lua_newtable`, `lua_register`, `lua_pushcfunction`, `lua_strlen`, `lua_isfunction`, `lua_istable`, `lua_islightuserdata`, `lua_isnil`, `lua_isboolean`, `lua_isthread`, `lua_isnone`, `lua_isnoneornil`, `lua_pushliteral`, `lua_setglobal`, `lua_getglobal`, `lua_tostring`

**Debug API:**
- `lua_getstack`, `lua_getinfo`, `lua_getupvalue`, `lua_setupvalue`
- `lua_sethook`

### From `lauxlib.h` — Auxiliary library functions:

- `luaL_register`, `luaL_callmeta`, `luaL_typerror`, `luaL_argerror`
- `luaL_checklstring`, `luaL_optlstring`, `luaL_checknumber`, `luaL_optnumber`
- `luaL_checkinteger`, `luaL_optinteger`
- `luaL_checkstack`, `luaL_checktype`, `luaL_checkany`
- `luaL_newmetatable`, `luaL_checkudata`
- `luaL_where`, `luaL_error`
- `luaL_checkoption`
- `luaL_ref`, `luaL_unref`
- `luaL_loadbuffer`, `luaL_loadstring`
- Buffer API: `luaL_buffinit`, `luaL_addlstring`, `luaL_addstring`, `luaL_pushresult`
- Macros: `luaL_argcheck`, `luaL_checkstring`, `luaL_optstring`, `luaL_checkint`, `luaL_optint`, `luaL_checklong`, `luaL_optlong`, `luaL_typename`, `luaL_dostring`, `luaL_getmetatable`, `luaL_addchar`

### From `lualib.h` — Standard libraries:

- `luaopen_base`, `luaopen_table`, `luaopen_os`, `luaopen_string`, `luaopen_math`, `luaopen_debug`
- `luaL_openlibs`

### Stubs (compile-check only, not exercised in tests):

- `lua_newstate`, `lua_xmove`, `lua_equal`, `lua_lessthan`, `lua_getfenv`, `lua_setfenv`
- `lua_load`, `lua_dump`, `lua_yield`, `lua_resume`, `lua_status`, `lua_setallocf`
- `lua_getlocal`, `lua_setlocal`, `lua_gethook`, `lua_gethookmask`, `lua_gethookcount`, `lua_setlevel`
- `luaI_openlib`, `luaL_getmetafield`, `luaL_loadfile`, `luaL_newstate`, `luaL_gsub`, `luaL_findtable`
- `luaL_prepbuffer`, `luaL_addvalue`, `luaL_addsize`, `luaL_dofile`
- `luaopen_io`, `luaopen_package`

### Build with Lua 5.1 static lib
Create a root `Makefile` with targets:
- `lua51-lib` — builds `lua51/src/liblua.a`
- `luau-lib` — builds Luau static libraries
- `example-lua51` — compiles `example/main.c` and links against `lua51/src/liblua.a`
- `example-luau` — compiles `example/main.c` and links against Luau static libs + compat layer
- `clean`

Run `make example-lua51` and verify it works.

---

## Phase 3: Try Building Example with Luau

Run `make example-luau` and collect all compile/link errors. These errors reveal the missing API surface.

---

## Phase 4: Identify Missing APIs

Based on header comparison, the following Lua 5.1 C API functions are **missing or incompatible** in Luau:

### Missing functions (no equivalent in Luau):
| Lua 5.1 Function | Notes | Defold Usage | Action |
|---|---|---|---|
| `lua_atpanic` | Luau uses `lua_Callbacks.panic` instead | Yes (test code) | Real impl |
| `lua_load` | Luau only has `luau_load` (pre-compiled bytecode, different signature) | Not used | **Stub** |
| `lua_dump` | No bytecode dump in Luau | Not used | **Stub** |
| `lua_setallocf` | Luau only has `lua_getallocf`, no setter | Not used | **Stub** |
| `lua_setlevel` | No equivalent | Not used | **Stub** |
| `lua_getstack` | Luau uses level-based debug API, no `lua_Debug` stack walking | Yes (script.cpp — traceback) | Real impl |
| `lua_sethook` | No hook API in Luau (uses callbacks/singlestep) | Yes (script_sys.cpp — debugger) | Real impl |
| `lua_gethook` | No hook API | Not used | **Stub** |
| `lua_gethookmask` | No hook API | Not used | **Stub** |
| `lua_gethookcount` | No hook API | Not used | **Stub** |
| `luaL_loadfile` | No source loading in Luau (needs compile+load) | Not used | **Stub** |
| `luaL_loadbuffer` | No source loading in Luau (needs compile+load) | Yes (script_module.cpp — core) | Real impl |
| `luaL_loadstring` | No source loading in Luau (needs compile+load) | Yes (test code) | Real impl |
| `luaL_ref` | Luau has `lua_ref` with different signature | Yes (heavily: gui, script, hash) | Real impl |
| `luaL_unref` | Luau has `lua_unref` with different signature | Yes (heavily: gui, script, hash) | Real impl |
| `luaI_openlib` | No equivalent | Not used | **Stub** |
| `luaL_gsub` | No equivalent | Not used | **Stub** |
| `luaL_typerror` | Luau has `luaL_typeerrorL` but it's `l_noret`, Lua 5.1 returns int | Yes (gui, gamesys, script, bitop) | Real impl |
| `luaL_prepbuffer` | Luau has `luaL_prepbuffsize` (different signature) | Not used | **Stub** |
| `luaL_addstring` | Is a macro in Luau, is a function in Lua 5.1 | Yes (luasocket/mime.c) | Real impl |
| `luaopen_io` | No io library in Luau | Not used | **Stub** |
| `luaopen_package` | No package/loadlib in Luau | Not used | **Stub** |

### Signature mismatches (exist but differ):
| Function | Lua 5.1 Signature | Luau Signature | Defold Usage | Action |
|---|---|---|---|---|
| `lua_pushcclosure` | `(L, fn, n)` | macro → `lua_pushcclosurek(L, fn, debugname, nup, NULL)` | Yes (luasocket, luacjson) | Real wrapper |
| `lua_pushcfunction` | `(L, f)` macro | `(L, fn, debugname)` macro | (via pushcclosure) | Real wrapper |
| `lua_resume` | `(L, narg)` | `(L, from, narg)` | Not used | **Stub wrapper** |
| `lua_getinfo` | `(L, what, ar)` | `(L, level, what, ar)` | Yes (script.cpp, script_sys.cpp) | Real wrapper |
| `lua_getlocal` | `(L, ar, n)` | `(L, level, n)` | Not used | **Stub wrapper** |
| `lua_setlocal` | `(L, ar, n)` | `(L, level, n)` | Not used | **Stub wrapper** |
| `lua_error` | returns `int` | returns `l_noret` (void, noreturn) | Used everywhere | Real wrapper |
| `lua_gettable` | returns `void` | returns `int` | Used everywhere | Real wrapper |
| `lua_getfield` | returns `void` | returns `int` | Used everywhere | Real wrapper |
| `lua_rawget` | returns `void` | returns `int` | Used everywhere | Real wrapper |
| `lua_rawgeti` | returns `void` | returns `int` | Used everywhere | Real wrapper |
| `luaL_Buffer` | struct with `char buffer[LUAL_BUFFERSIZE]` | `luaL_Strbuf` with different layout | Yes (luasocket: buffer.c, mime.c) | Real adapter |

---

## Phase 4.1: Defold Engine Usage Analysis

Since the target is the Defold game engine, we searched its source code for every missing/incompatible function:
```
ack FUNCTION_NAME /home/linux/defold-engine/engine --ignore-dir=/home/linux/defold-engine/engine/lua
```

Functions **not found** in Defold can be implemented as stubs (correct signature, but return error or no-op). This significantly reduces the amount of work needed.

**Summary:**
- **13 stubs** (not used by Defold): `lua_load`, `lua_dump`, `lua_setallocf`, `lua_setlevel`, `lua_gethook`, `lua_gethookmask`, `lua_gethookcount`, `luaL_loadfile`, `luaI_openlib`, `luaL_gsub`, `luaL_prepbuffer`, `luaopen_io`, `luaopen_package`
- **10 real implementations** needed for missing functions used by Defold
- **~9 real wrappers** needed for signature mismatches used by Defold

---

## Phase 5: Implement Compatibility Layer

Create **external** files (DO NOT modify Luau sources):
- `compat/lua51_compat.h` — header with Lua 5.1 compatible declarations
- `compat/lua51_compat.cpp` — implementation of missing/incompatible functions

### Real implementations (used by Defold):

1. **`lua_atpanic`** — wrap `lua_callbacks(L)->panic`
2. **`lua_getstack`** — implement via Luau's level-based `lua_getinfo`
3. **`lua_sethook`** — implement via Luau's callbacks + singlestep
4. **`luaL_loadbuffer` / `luaL_loadstring`** — compile source to bytecode with `luau_compile`, then `luau_load`
5. **`luaL_ref` / `luaL_unref`** — wrap Luau's `lua_ref` / `lua_unref`
6. **`luaL_typerror`** — wrap `luaL_typeerrorL` (note: noreturn difference, declare as noreturn too)
7. **`luaL_addstring`** — implement as function wrapping `luaL_addlstring`
8. **`lua_pushcclosure` / `lua_pushcfunction` ABI mismatch** — handle via macros/wrappers that add NULL debugname
9. **`lua_getinfo`** — wrapper translating `lua_Debug*` to level-based API
10. **`lua_error` return type** — wrapper or macro (noreturn is compatible for callers)
11. **`lua_gettable` / `lua_getfield` / `lua_rawget` / `lua_rawgeti`** — return type void→int wrappers
12. **`luaL_Buffer` layout** — typedef + adapter macros mapping to `luaL_Strbuf`

### Stubs (not used by Defold — correct signature, return error/no-op):

13. **`lua_load`** — stub returning error (Luau doesn't support source loading via this API)
14. **`lua_dump`** — stub returning error (Luau doesn't support bytecode dumping)
15. **`lua_setallocf`** — no-op (Luau doesn't support changing allocator after creation)
16. **`lua_setlevel`** — no-op (Lua 5.1 itself says "hack")
17. **`lua_gethook` / `lua_gethookmask` / `lua_gethookcount`** — stubs returning NULL/0
18. **`luaL_loadfile`** — stub returning error
19. **`luaI_openlib`** — stub (no-op)
20. **`luaL_gsub`** — stub returning NULL
21. **`luaL_prepbuffer`** — stub wrapping `luaL_prepbuffsize(B, LUAL_BUFFERSIZE)`
22. **`luaopen_io` / `luaopen_package`** — stubs that register empty tables
23. **`lua_resume`** — stub wrapper adding NULL `from` parameter
24. **`lua_getlocal` / `lua_setlocal`** — stub wrappers returning NULL/0

---

## Phase 6: Build Example with Luau + Compat Layer

- Build compat layer as static library: `make compat-lib` (compiles `compat/lua51_compat.cpp` → `compat/libcompat.a`)
- Build example using only compat include path: `make example-luau` (`-Icompat/include`, links `libcompat.a` + Luau libs)
- The example's `main.c` includes only `lua.h`, `lauxlib.h`, `lualib.h` — no `#ifdef`, no compat-specific includes
- Run and verify all tests pass identically to the Lua 5.1 version

---

## Architecture: Header and Library Separation

**Key principle:** `example/main.c` includes ONLY standard Lua 5.1 header names (`lua.h`, `lauxlib.h`, `lualib.h`) without any `#ifdef` or compat-specific includes. The build system selects which headers to use via `-I` paths:

- **Lua 5.1 build:** `-Ilua51/src` — uses original Lua 5.1 headers
- **Luau build:** `-Icompat/include` — uses compat headers that present Lua 5.1 API surface on top of Luau

The compat layer is built as a **static library** (`libcompat.a`) that is linked together with Luau libraries. The compat headers (`compat/include/lua.h`, etc.) include Luau's real headers internally and add macros/declarations for Lua 5.1 compatibility. The compat `.cpp` implementation is compiled separately against Luau headers only.

## File Structure (Final)

```
ext_luau/
├── PLAN.md
├── Makefile                  # Root build script
├── lua51/                    # Lua 5.1 sources (untouched)
├── luau/                     # Luau sources (untouched)
├── compat/
│   ├── include/
│   │   ├── lua.h             # Lua 5.1 API compat header (wraps Luau's lua.h)
│   │   ├── lauxlib.h         # Lua 5.1 auxlib compat header (wraps Luau's lualib.h)
│   │   └── lualib.h          # Lua 5.1 standard libs compat header
│   ├── lua51_compat.cpp      # Compat function implementations
│   └── libcompat.a           # Built static library
└── example/
    └── main.c                # Test app using ONLY standard Lua 5.1 headers
```
