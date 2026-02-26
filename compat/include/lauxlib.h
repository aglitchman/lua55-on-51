/*
** Lua 5.1 auxiliary library compatibility header for Luau
*/

#ifndef lua51compat_lauxlib_h
#define lua51compat_lauxlib_h

#include <stddef.h>
#include <stdio.h>

#include "lua.h"

// Include Luau's lualib.h which has all auxiliary functions
#include "../../luau/VM/include/lualib.h"

// Extra error code for luaL_load
#ifndef LUA_ERRFILE
#define LUA_ERRFILE (LUA_ERRERR + 1)
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// luaL_error: Lua 5.1 returns int, Luau is noreturn
// ============================================================
#undef luaL_error
LUA_API int lua51_compat_lerror(lua_State* L, const char* fmt, ...);
#define luaL_error lua51_compat_lerror

// ============================================================
// luaL_typerror: Lua 5.1 = returns int, Luau has luaL_typeerrorL (noreturn)
// ============================================================
#undef luaL_typeerror
LUALIB_API int lua51_compat_typerror(lua_State* L, int narg, const char* tname);
#define luaL_typerror(L, narg, tname) lua51_compat_typerror(L, narg, tname)

// ============================================================
// luaL_argerror: Lua 5.1 returns int, Luau is noreturn
// ============================================================
#undef luaL_argerror
LUALIB_API int lua51_compat_argerror(lua_State* L, int numarg, const char* extramsg);
#define luaL_argerror lua51_compat_argerror

// Re-define luaL_argcheck to use our luaL_argerror
#undef luaL_argcheck
#define luaL_argcheck(L, cond, numarg, extramsg) \
    ((void)((cond) || luaL_argerror(L, (numarg), (extramsg))))

// ============================================================
// luaL_loadbuffer / luaL_loadstring: not in Luau (needs compile+load)
// ============================================================
LUALIB_API int lua51_compat_loadbuffer(lua_State* L, const char* buff, size_t sz, const char* name);
LUALIB_API int lua51_compat_loadstring(lua_State* L, const char* s);
#define luaL_loadbuffer(L, buff, sz, name) lua51_compat_loadbuffer(L, buff, sz, name)
#define luaL_loadstring(L, s) lua51_compat_loadstring(L, s)

#undef luaL_dostring
#define luaL_dostring(L, s) (lua51_compat_loadstring(L, s) || lua_pcall(L, 0, LUA_MULTRET, 0))

// ============================================================
// luaL_ref / luaL_unref: Luau has lua_ref/lua_unref with different signatures
// ============================================================
LUALIB_API int lua51_compat_ref(lua_State* L, int t);
LUALIB_API void lua51_compat_unref(lua_State* L, int t, int ref);
#define luaL_ref(L, t) lua51_compat_ref(L, t)
#define luaL_unref(L, t, ref) lua51_compat_unref(L, t, ref)

// Lua 5.1 ref compat macros
#undef lua_ref
#undef lua_unref
#undef lua_getref
#define lua_ref(L, lock) ((lock) ? luaL_ref(L, LUA_REGISTRYINDEX) : \
      (lua_pushstring(L, "unlocked references are obsolete"), lua_error(L), 0))
#define lua_unref(L, ref) luaL_unref(L, LUA_REGISTRYINDEX, (ref))
#define lua_getref(L, ref) lua_rawgeti(L, LUA_REGISTRYINDEX, (ref))

// ============================================================
// luaL_loadfile stub
// ============================================================
LUALIB_API int lua51_compat_loadfile(lua_State* L, const char* filename);
#define luaL_loadfile(L, fn) lua51_compat_loadfile(L, fn)

#undef luaL_dofile
#define luaL_dofile(L, fn) (lua51_compat_loadfile(L, fn) || lua_pcall(L, 0, LUA_MULTRET, 0))

// ============================================================
// luaI_openlib stub
// ============================================================
LUALIB_API void lua51_compat_openlib(lua_State* L, const char* libname, const luaL_Reg* l, int nup);
#define luaI_openlib(L, libname, l, nup) lua51_compat_openlib(L, libname, l, nup)

// ============================================================
// luaL_gsub stub
// ============================================================
LUALIB_API const char* lua51_compat_gsub(lua_State* L, const char* s, const char* p, const char* r);
#undef luaL_gsub
#define luaL_gsub(L, s, p, r) lua51_compat_gsub(L, s, p, r)

// ============================================================
// Lua 5.1 integer macros
// ============================================================
#ifndef luaL_checkint
#define luaL_checkint(L, n)  ((int)luaL_checkinteger(L, (n)))
#define luaL_optint(L, n, d) ((int)luaL_optinteger(L, (n), (d)))
#define luaL_checklong(L, n)  ((long)luaL_checkinteger(L, (n)))
#define luaL_optlong(L, n, d) ((long)luaL_optinteger(L, (n), (d)))
#endif

// ============================================================
// luaL_typename: Luau has it as a function, Lua 5.1 has it as a macro
// Both work, but make sure the macro form is available
// ============================================================
#ifndef luaL_typename
#define luaL_typename(L, i) lua_typename(L, lua_type(L, (i)))
#endif

// ============================================================
// Buffer compat
// ============================================================
// Luau's luaL_Buffer (typedef for luaL_Strbuf) is already defined via lualib.h
// Lua 5.1's luaL_Buffer has a different layout but we use Luau's layout
// since all buffer functions go through Luau

#ifndef LUAL_BUFFERSIZE
#define LUAL_BUFFERSIZE LUA_BUFFERSIZE
#endif

// luaL_prepbuffer: Lua 5.1 takes (B), Luau has luaL_prepbuffsize(B, size)
LUALIB_API char* lua51_compat_prepbuffer(luaL_Buffer* B);
#undef luaL_prepbuffer
#define luaL_prepbuffer(B) lua51_compat_prepbuffer(B)

// luaL_addstring: Luau defines it as a macro, Lua 5.1 has it as a function
// Luau's macro version is fine, but ensure it's available
#ifndef luaL_addstring
#define luaL_addstring(B, s) luaL_addlstring(B, s, strlen(s))
#endif

// luaL_addsize: Lua 5.1 macro
#ifndef luaL_addsize
#define luaL_addsize(B, n) ((B)->p += (n))
#endif

// luaL_putchar compat
#ifndef luaL_putchar
#define luaL_putchar(B, c) luaL_addchar(B, c)
#endif

// luaL_reg compat typedef
#ifndef luaL_reg
#define luaL_reg luaL_Reg
#endif

#ifdef __cplusplus
}
#endif

#endif
