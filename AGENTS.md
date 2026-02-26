# AGENTS.md — Luau Compat Layer Notes

## Luau VM Patches (REQUIRED)

Luau's type enum includes `LUA_TVECTOR` which shifts `LUA_TSTRING` through `LUA_TTHREAD` by +1 compared to Lua 5.1. To make the compat layer work without header wrappers, three files in the Luau VM must be patched:

### Patched files

1. **`luau/VM/include/lua.h`** — Move `LUA_TVECTOR` from between `LUA_TNUMBER` and `LUA_TSTRING` to after `LUA_TBUFFER`. This makes `LUA_TSTRING=4`, `LUA_TTABLE=5`, etc., matching Lua 5.1 exactly.

2. **`luau/VM/src/lobject.h`** — Change `iscollectable(o)` from `(ttype(o) >= LUA_TSTRING)` to `(ttype(o) >= LUA_TSTRING && ttype(o) != LUA_TVECTOR)`. Required because VECTOR is now positioned after GC types but is NOT a GC-collectible type.

3. **`luau/VM/src/ltm.cpp`** — Reorder `luaT_typenames[]` array to match the new enum order (move `"vector"` entry to after `"buffer"`).

### Applying patches

Run from repo root:
```bash
./patches/apply_luau_patches.sh
make -C luau clean && make -C luau
```

Or apply manually using `patches/luau_type_reorder.patch` as reference.

### After Luau updates

When updating the Luau submodule/source, re-apply these patches and rebuild. The patch script checks for the presence of `LUA_TVECTOR` before modifying.

## Type Constant Mapping (after patch)

| Constant           | Lua 5.1 | Luau (patched) |
|--------------------|---------|----------------|
| LUA_TNIL           | 0       | 0              |
| LUA_TBOOLEAN       | 1       | 1              |
| LUA_TLIGHTUSERDATA | 2       | 2              |
| LUA_TNUMBER        | 3       | 3              |
| LUA_TSTRING        | 4       | 4              |
| LUA_TTABLE         | 5       | 5              |
| LUA_TFUNCTION      | 6       | 6              |
| LUA_TUSERDATA      | 7       | 7              |
| LUA_TTHREAD        | 8       | 8              |
| LUA_TBUFFER        | —       | 9              |
| LUA_TVECTOR        | —       | 10             |

## Compat Library Architecture

- `compat/luau_bridge.cpp` — Compiled with Luau headers. `extern "C"` bridges for APIs with same name but different C++ signature.
- `compat/lua51_compat.cpp` — Compiled with Lua 5.1 headers (`-Ilua51/src`). Exports all missing/incompatible Lua 5.1 functions.
- Both builds (lua51 and luau) use `-Ilua51/src` for headers. No wrapper headers needed.
