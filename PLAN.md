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

## Phase 2: Create Example App Using ALL Lua 5.1 C API

Create `example/main.c` — a single C file that exercises **every** public Lua 5.1 C API function and macro. Organized by header:

### From `lua.h` — Core API functions to exercise:

**State manipulation:**
- `lua_newstate`, `lua_close`, `lua_newthread`, `lua_atpanic`

**Stack manipulation:**
- `lua_gettop`, `lua_settop`, `lua_pushvalue`, `lua_remove`, `lua_insert`, `lua_replace`, `lua_checkstack`, `lua_xmove`

**Access functions (stack → C):**
- `lua_isnumber`, `lua_isstring`, `lua_iscfunction`, `lua_isuserdata`, `lua_type`, `lua_typename`
- `lua_equal`, `lua_rawequal`, `lua_lessthan`
- `lua_tonumber`, `lua_tointeger`, `lua_toboolean`, `lua_tolstring`, `lua_objlen`, `lua_tocfunction`, `lua_touserdata`, `lua_tothread`, `lua_topointer`

**Push functions (C → stack):**
- `lua_pushnil`, `lua_pushnumber`, `lua_pushinteger`, `lua_pushlstring`, `lua_pushstring`, `lua_pushvfstring`, `lua_pushfstring`, `lua_pushcclosure`, `lua_pushboolean`, `lua_pushlightuserdata`, `lua_pushthread`

**Get functions (Lua → stack):**
- `lua_gettable`, `lua_getfield`, `lua_rawget`, `lua_rawgeti`, `lua_createtable`, `lua_newuserdata`, `lua_getmetatable`, `lua_getfenv`

**Set functions (stack → Lua):**
- `lua_settable`, `lua_setfield`, `lua_rawset`, `lua_rawseti`, `lua_setmetatable`, `lua_setfenv`

**Load and call:**
- `lua_call`, `lua_pcall`, `lua_cpcall`, `lua_load`, `lua_dump`

**Coroutines:**
- `lua_yield`, `lua_resume`, `lua_status`

**GC:**
- `lua_gc` (with all options: STOP, RESTART, COLLECT, COUNT, COUNTB, STEP, SETPAUSE, SETSTEPMUL)

**Miscellaneous:**
- `lua_error`, `lua_next`, `lua_concat`, `lua_getallocf`, `lua_setallocf`

**Macros (use them to ensure they compile):**
- `lua_pop`, `lua_newtable`, `lua_register`, `lua_pushcfunction`, `lua_strlen`, `lua_isfunction`, `lua_istable`, `lua_islightuserdata`, `lua_isnil`, `lua_isboolean`, `lua_isthread`, `lua_isnone`, `lua_isnoneornil`, `lua_pushliteral`, `lua_setglobal`, `lua_getglobal`, `lua_tostring`

**Debug API:**
- `lua_getstack`, `lua_getinfo`, `lua_getlocal`, `lua_setlocal`, `lua_getupvalue`, `lua_setupvalue`
- `lua_sethook`, `lua_gethook`, `lua_gethookmask`, `lua_gethookcount`
- `lua_setlevel`

### From `lauxlib.h` — Auxiliary library functions:

- `luaI_openlib`, `luaL_register`, `luaL_getmetafield`, `luaL_callmeta`, `luaL_typerror`, `luaL_argerror`
- `luaL_checklstring`, `luaL_optlstring`, `luaL_checknumber`, `luaL_optnumber`
- `luaL_checkinteger`, `luaL_optinteger`
- `luaL_checkstack`, `luaL_checktype`, `luaL_checkany`
- `luaL_newmetatable`, `luaL_checkudata`
- `luaL_where`, `luaL_error`
- `luaL_checkoption`
- `luaL_ref`, `luaL_unref`
- `luaL_loadfile`, `luaL_loadbuffer`, `luaL_loadstring`
- `luaL_newstate`
- `luaL_gsub`, `luaL_findtable`
- Buffer API: `luaL_buffinit`, `luaL_prepbuffer`, `luaL_addlstring`, `luaL_addstring`, `luaL_addvalue`, `luaL_pushresult`
- Macros: `luaL_argcheck`, `luaL_checkstring`, `luaL_optstring`, `luaL_checkint`, `luaL_optint`, `luaL_checklong`, `luaL_optlong`, `luaL_typename`, `luaL_dofile`, `luaL_dostring`, `luaL_getmetatable`, `luaL_addchar`, `luaL_addsize`

### From `lualib.h` — Standard libraries:

- `luaopen_base`, `luaopen_table`, `luaopen_io`, `luaopen_os`, `luaopen_string`, `luaopen_math`, `luaopen_debug`, `luaopen_package`
- `luaL_openlibs`

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
| Lua 5.1 Function | Notes |
|---|---|
| `lua_atpanic` | Luau uses `lua_Callbacks.panic` instead |
| `lua_load` | Luau only has `luau_load` (pre-compiled bytecode, different signature) |
| `lua_dump` | No bytecode dump in Luau |
| `lua_setallocf` | Luau only has `lua_getallocf`, no setter |
| `lua_setlevel` | No equivalent |
| `lua_getstack` | Luau uses level-based debug API, no `lua_Debug` stack walking |
| `lua_sethook` | No hook API in Luau (uses callbacks/singlestep) |
| `lua_gethook` | No hook API |
| `lua_gethookmask` | No hook API |
| `lua_gethookcount` | No hook API |
| `luaL_loadfile` | No source loading in Luau (needs compile+load) |
| `luaL_loadbuffer` | No source loading in Luau (needs compile+load) |
| `luaL_loadstring` | No source loading in Luau (needs compile+load) |
| `luaL_ref` | Luau has `lua_ref` with different signature |
| `luaL_unref` | Luau has `lua_unref` with different signature |
| `luaI_openlib` | No equivalent |
| `luaL_gsub` | No equivalent |
| `luaL_typerror` | Luau has `luaL_typeerrorL` but it's `l_noret`, Lua 5.1 returns int |
| `luaL_prepbuffer` | Luau has `luaL_prepbuffsize` (different signature) |
| `luaL_addstring` | Is a macro in Luau, is a function in Lua 5.1 |
| `luaopen_io` | No io library in Luau |
| `luaopen_package` | No package/loadlib in Luau |

### Signature mismatches (exist but differ):
| Function | Lua 5.1 Signature | Luau Signature |
|---|---|---|
| `lua_pushcclosure` | `(L, fn, n)` | macro → `lua_pushcclosurek(L, fn, debugname, nup, NULL)` |
| `lua_pushcfunction` | `(L, f)` macro | `(L, fn, debugname)` macro |
| `lua_resume` | `(L, narg)` | `(L, from, narg)` |
| `lua_getinfo` | `(L, what, ar)` | `(L, level, what, ar)` |
| `lua_getlocal` | `(L, ar, n)` | `(L, level, n)` |
| `lua_setlocal` | `(L, ar, n)` | `(L, level, n)` |
| `lua_error` | returns `int` | returns `l_noret` (void, noreturn) |
| `lua_gettable` | returns `void` | returns `int` |
| `lua_getfield` | returns `void` | returns `int` |
| `lua_rawget` | returns `void` | returns `int` |
| `lua_rawgeti` | returns `void` | returns `int` |
| `luaL_Buffer` | struct with `char buffer[LUAL_BUFFERSIZE]` | `luaL_Strbuf` with different layout |

---

## Phase 5: Implement Compatibility Layer

Create **external** files (DO NOT modify Luau sources):
- `compat/lua51_compat.h` — header with Lua 5.1 compatible declarations
- `compat/lua51_compat.cpp` — implementation of missing/incompatible functions

Implementation strategy for each missing function:

1. **`lua_atpanic`** — wrap `lua_callbacks(L)->panic`
2. **`lua_load`** — compile source with `luau_compile`, then `luau_load` bytecode (requires linking Luau Compiler)
3. **`lua_dump`** — stub that returns an error (Luau doesn't support bytecode dumping from C API)
4. **`lua_setallocf`** — stub or no-op (Luau doesn't support changing allocator after creation)
5. **`lua_setlevel`** — no-op (Lua 5.1 itself says "hack")
6. **`lua_getstack`** — implement via Luau's level-based `lua_getinfo`
7. **`lua_sethook` / `lua_gethook` / `lua_gethookmask` / `lua_gethookcount`** — implement via Luau's callbacks + singlestep
8. **`luaL_loadbuffer` / `luaL_loadstring` / `luaL_loadfile`** — compile source to bytecode, then `luau_load`
9. **`luaL_ref` / `luaL_unref`** — wrap Luau's `lua_ref` / `lua_unref`
10. **`luaI_openlib`** — implement using `luaL_register` + upvalue logic
11. **`luaL_gsub`** — reimplement (string substitution utility)
12. **`luaL_typerror`** — wrap `luaL_typeerrorL` (note: noreturn difference, declare as noreturn too)
13. **`luaL_prepbuffer`** — wrap `luaL_prepbuffsize(B, LUAL_BUFFERSIZE)`
14. **`luaL_addstring`** — implement as function wrapping `luaL_addlstring`
15. **`luaopen_io` / `luaopen_package`** — stubs that register empty tables (or omit from test)
16. **`lua_pushcclosure` / `lua_pushcfunction` ABI mismatch** — handle via macros/wrappers that add NULL debugname
17. **`lua_resume` signature** — wrapper adding NULL `from` parameter
18. **`lua_getinfo` / `lua_getlocal` / `lua_setlocal`** — wrappers translating `lua_Debug*` to level-based API
19. **`lua_error` return type** — wrapper or macro (noreturn is compatible for callers)
20. **`luaL_Buffer` layout** — typedef + adapter macros mapping to `luaL_Strbuf`

---

## Phase 6: Build Example with Luau + Compat Layer

- Add `compat/lua51_compat.cpp` to the Luau link target in the Makefile
- Include `compat/lua51_compat.h` from the example (or make it a drop-in replacement header)
- Build: `make example-luau`
- Run and verify all tests pass identically to the Lua 5.1 version

---

## File Structure (Final)

```
ext_luau/
├── PLAN.md
├── Makefile              # Root build script
├── lua51/                # Lua 5.1 sources (untouched)
├── luau/                 # Luau sources (untouched)
├── compat/
│   ├── lua51_compat.h    # Lua 5.1 API compatibility declarations
│   └── lua51_compat.cpp  # Lua 5.1 API compatibility implementations
└── example/
    └── main.c            # Test app using ALL Lua 5.1 C API functions
```
