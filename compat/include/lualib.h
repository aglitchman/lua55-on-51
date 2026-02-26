/*
** Lua 5.1 standard libraries compatibility header for Luau
*/

#ifndef lua51compat_lualib_h
#define lua51compat_lualib_h

#include "lua.h"

// Luau's lualib.h (included via lauxlib.h or directly) provides:
// luaopen_base, luaopen_table, luaopen_os, luaopen_string, luaopen_math,
// luaopen_debug, luaL_openlibs
#include "../../luau/VM/include/lualib.h"

#ifdef __cplusplus
extern "C" {
#endif

// Lua 5.1 library names not in Luau
#ifndef LUA_IOLIBNAME
#define LUA_IOLIBNAME   "io"
#endif
#ifndef LUA_LOADLIBNAME
#define LUA_LOADLIBNAME "package"
#endif
#ifndef LUA_FILEHANDLE
#define LUA_FILEHANDLE  "FILE*"
#endif

// Stubs for libraries not available in Luau
LUALIB_API int lua51_compat_openio(lua_State* L);
LUALIB_API int lua51_compat_openpackage(lua_State* L);
#define luaopen_io(L) lua51_compat_openio(L)
#define luaopen_package(L) lua51_compat_openpackage(L)

#ifndef lua_assert
#define lua_assert(x) ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif
