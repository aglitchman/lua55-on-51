#pragma once

// Include Luau headers first
#include "lua.h"
#include "lualib.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// Macro overrides for Lua 5.1 compatibility
// ============================================================

// lua_pushcclosure: Lua 5.1 = (L, fn, nup), Luau = (L, fn, debugname, nup)
#undef lua_pushcclosure
#undef lua_pushcfunction
#define lua_pushcclosure(L, fn, nup) lua_pushcclosurek(L, fn, NULL, nup, NULL)
#define lua_pushcfunction(L, fn) lua_pushcclosurek(L, fn, NULL, 0, NULL)

// lua_register
#undef lua_register
#define lua_register(L, n, f) (lua_pushcfunction(L, f), lua_setglobal(L, n))

// lua_error: Luau returns void (noreturn), Lua 5.1 returns int
// For `return lua_error(L)` pattern, we need a wrapper returning int
LUA_API int lua51_compat_error(lua_State* L);
#define lua51_error(L) lua51_compat_error(L)

// luaL_error: Luau returns void (noreturn), Lua 5.1 returns int
// For `return luaL_error(L, ...)` pattern
#undef luaL_error
LUA_API int lua51_compat_lerror(lua_State* L, const char* fmt, ...);
#define luaL_error lua51_compat_lerror

// GC compat
#ifndef LUA_GCSETPAUSE
#define LUA_GCSETPAUSE 6
#endif

// lua_atpanic
LUA_API lua_CFunction lua51_compat_atpanic(lua_State* L, lua_CFunction panicf);
#define lua_atpanic(L, f) lua51_compat_atpanic(L, f)

// lua_getstack: store level in lua_Debug.userdata for subsequent getinfo
LUA_API int lua51_compat_getstack(lua_State* L, int level, lua_Debug* ar);
// lua_getinfo: Lua 5.1 = (L, what, ar), Luau = (L, level, what, ar)
LUA_API int lua51_compat_getinfo(lua_State* L, const char* what, lua_Debug* ar);
// Override Luau's lua_getinfo macro/function
#define lua_getstack(L, level, ar) lua51_compat_getstack(L, level, ar)
#define lua_getinfo(L, what, ar) lua51_compat_getinfo(L, what, ar)

// Hook API
#ifndef LUA_MASKCALL
#define LUA_MASKCALL (1 << 0)
#define LUA_MASKRET (1 << 1)
#define LUA_MASKLINE (1 << 2)
#define LUA_MASKCOUNT (1 << 3)
#define LUA_HOOKCALL 0
#define LUA_HOOKRET 1
#define LUA_HOOKLINE 2
#define LUA_HOOKCOUNT 3
#endif

LUA_API int lua51_compat_sethook(lua_State* L, lua_Hook func, int mask, int count);
LUA_API lua_Hook lua51_compat_gethook(lua_State* L);
LUA_API int lua51_compat_gethookmask(lua_State* L);
LUA_API int lua51_compat_gethookcount(lua_State* L);
#define lua_sethook(L, f, mask, count) lua51_compat_sethook(L, f, mask, count)
#define lua_gethook(L) lua51_compat_gethook(L)
#define lua_gethookmask(L) lua51_compat_gethookmask(L)
#define lua_gethookcount(L) lua51_compat_gethookcount(L)

// luaL_loadbuffer / luaL_loadstring
LUALIB_API int lua51_compat_loadbuffer(lua_State* L, const char* buff, size_t sz, const char* name);
LUALIB_API int lua51_compat_loadstring(lua_State* L, const char* s);
#define luaL_loadbuffer(L, buff, sz, name) lua51_compat_loadbuffer(L, buff, sz, name)
#define luaL_loadstring(L, s) lua51_compat_loadstring(L, s)
#undef luaL_dostring
#define luaL_dostring(L, s) (lua51_compat_loadstring(L, s) || lua_pcall(L, 0, LUA_MULTRET, 0))

// luaL_ref / luaL_unref
// Luau already defines LUA_NOREF and LUA_REFNIL, don't redefine
LUALIB_API int lua51_compat_ref(lua_State* L, int t);
LUALIB_API void lua51_compat_unref(lua_State* L, int t, int ref);
#define luaL_ref(L, t) lua51_compat_ref(L, t)
#define luaL_unref(L, t, ref) lua51_compat_unref(L, t, ref)

// luaL_typerror
#undef luaL_typeerror
LUALIB_API int lua51_compat_typerror(lua_State* L, int narg, const char* tname);
#define luaL_typerror(L, narg, tname) lua51_compat_typerror(L, narg, tname)

// Lua 5.1 macros missing in Luau
#ifndef luaL_checkint
#define luaL_checkint(L, n) ((int)luaL_checkinteger(L, (n)))
#define luaL_optint(L, n, d) ((int)luaL_optinteger(L, (n), (d)))
#define luaL_checklong(L, n) ((long)luaL_checkinteger(L, (n)))
#define luaL_optlong(L, n, d) ((long)luaL_optinteger(L, (n), (d)))
#endif

// Buffer compat
#ifndef LUAL_BUFFERSIZE
#define LUAL_BUFFERSIZE LUA_BUFFERSIZE
#endif

// Stubs
LUA_API void lua51_compat_setallocf(lua_State* L, lua_Alloc f, void* ud);
LUA_API void lua51_compat_setlevel(lua_State* from, lua_State* to);
LUALIB_API int lua51_compat_loadfile(lua_State* L, const char* filename);
LUALIB_API void lua51_compat_openlib(lua_State* L, const char* libname, const luaL_Reg* l, int nup);
LUALIB_API const char* lua51_compat_gsub(lua_State* L, const char* s, const char* p, const char* r);
LUALIB_API char* lua51_compat_prepbuffer(luaL_Buffer* B);
LUALIB_API int lua51_compat_openio(lua_State* L);
LUALIB_API int lua51_compat_openpackage(lua_State* L);

#define lua_setallocf(L, f, ud) lua51_compat_setallocf(L, f, ud)
#define lua_setlevel(from, to) lua51_compat_setlevel(from, to)
#define luaL_loadfile(L, fn) lua51_compat_loadfile(L, fn)
#define luaL_dofile(L, fn) (lua51_compat_loadfile(L, fn) || lua_pcall(L, 0, LUA_MULTRET, 0))
#define luaI_openlib(L, libname, l, nup) lua51_compat_openlib(L, libname, l, nup)
#define luaL_gsub(L, s, p, r) lua51_compat_gsub(L, s, p, r)
#define luaL_prepbuffer(B) lua51_compat_prepbuffer(B)
#define luaopen_io(L) lua51_compat_openio(L)
#define luaopen_package(L) lua51_compat_openpackage(L)

#ifdef __cplusplus
}
#endif
