# AGENTS.md — Lua 5.1 API Compat Layer

This project provides drop-in Lua 5.1 C API compatibility on top of **Luau** and **Lua 5.5**. Application code (e.g. `compat_tests/main.c`) includes only standard Lua 5.1 headers (`lua.h`, `lauxlib.h`, `lualib.h`) from `lua51/src` without any `#ifdef`. The only difference between builds is which library is linked.

## Builds

| Build       | Headers        | Link                                       |
|-------------|----------------|---------------------------------------------|
| Lua 5.1     | `-Ilua51/src`  | `lua51/src/liblua.a`                        |
| Lua 5.5     | `-Ilua51/src`  | `libcompat55.a` (lua55 + compat shim)       |
| Luau        | `-Ilua51/src`  | `libcompat.a` + Luau `.a` files             |
| Luau (RT)   | `-Ilua51/src`  | `libcompat_runtime.a` + Luau VM `.a` files  |

## File Structure

```
ext_luau/
├── AGENTS.md
├── PLAN.md                   # Luau compat plan
├── PLAN_LUA55.md             # Lua 5.5 compat plan
├── Makefile                  # GNU Make build (all targets)
├── CMakeLists.txt            # CMake build (compat55 only)
├── .github/workflows/        # CI: build libcompat55.a for all platforms
├── patches/
│   ├── apply_luau_patches.sh
│   └── luau_type_reorder.patch
├── lua51/                    # Lua 5.1 sources (untouched)
│   └── src/                  # Headers used by ALL builds
├── lua55/                    # Lua 5.5 sources (lua55_ prefixed API)
├── luau/                     # Luau sources (patched type enum only)
├── compat/
│   ├── luau_bridge.cpp       # Luau-side bridges (compiled with Luau headers)
│   ├── lua51_compat.cpp      # Lua 5.1 API shim for Luau (compiled with lua51 headers)
│   └── lua55_compat.cpp      # Lua 5.1 API shim for Lua 5.5 (compiled as C with -I.)
└── compat_tests/
    └── main.c                # Test app using ONLY standard Lua 5.1 headers
```

## Lua 5.5 Compat Layer (compat55)

Lua 5.5 uses `lua55_` prefix for all exports (`lua55_State`, `lua55_pushnumber`, `lua55L_Buffer`, etc.), so there are no naming conflicts with Lua 5.1 API.

- `compat/lua55_compat.cpp` — Single file compiled as C (`-x c` / `LANGUAGE C`). Includes `lua55/lua.h` via `-I.` (project root). Implements all Lua 5.1 API functions by wrapping `lua55_*` calls.
- `libcompat55.a` — Combined static library containing all lua55 object files + the compat shim. Self-contained, no additional link dependencies beyond system libs.

### CMake build (cross-platform)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build          # produces build/libcompat55.a
```

Test executable (disabled for Emscripten, controlled by `COMPAT55_BUILD_TESTS`):

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCOMPAT55_BUILD_TESTS=ON
cmake --build build          # also produces build/test_lua55
```

### Makefile build (Linux only)

```bash
make compat55-lib            # produces compat/libcompat55.a
make compat-test-lua55       # produces compat_tests/test_lua55
```

## Luau Compat Layer

### Luau VM Patches (REQUIRED)

Patches are based on Luau commit `b968ef74` (release/709).

Luau's type enum includes `LUA_TVECTOR` which shifts `LUA_TSTRING` through `LUA_TTHREAD` by +1 compared to Lua 5.1. Three files in the Luau VM must be patched:

1. **`luau/VM/include/lua.h`** — Move `LUA_TVECTOR` from between `LUA_TNUMBER` and `LUA_TSTRING` to after `LUA_TBUFFER`. This makes `LUA_TSTRING=4`, `LUA_TTABLE=5`, etc., matching Lua 5.1 exactly.

2. **`luau/VM/src/lobject.h`** — Change `iscollectable(o)` from `(ttype(o) >= LUA_TSTRING)` to `(ttype(o) >= LUA_TSTRING && ttype(o) != LUA_TVECTOR)`. Required because VECTOR is now positioned after GC types but is NOT a GC-collectible type.

3. **`luau/VM/src/ltm.cpp`** — Reorder `luaT_typenames[]` array to match the new enum order (move `"vector"` entry to after `"buffer"`).

```bash
./patches/apply_luau_patches.sh
make -C luau clean && make -C luau
```

When updating Luau sources, re-apply patches and rebuild.

### Type Constant Mapping (after patch)

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

### Library Architecture

Two-file architecture to avoid C++ name mangling conflicts:

- `compat/luau_bridge.cpp` — Compiled with Luau headers. `extern "C"` bridges for APIs with same name but different C++ signature.
- `compat/lua51_compat.cpp` — Compiled with Lua 5.1 headers (`-Ilua51/src`). Exports all missing/incompatible Lua 5.1 functions.
- Both builds (lua51 and luau) use `-Ilua51/src` for headers. No wrapper headers needed.
