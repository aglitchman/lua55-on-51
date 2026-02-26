/*
** Lua 5.1 C API compatibility header for Luau
** This header provides a Lua 5.1 compatible API surface on top of Luau.
** Include this instead of Luau's lua.h when building Lua 5.1 C API code against Luau.
*/

#ifndef lua51compat_lua_h
#define lua51compat_lua_h

// Include Luau's real lua.h
#include "../../luau/VM/include/lua.h"

// Lua 5.1 version info
#undef LUA_VERSION
#undef LUA_RELEASE
#undef LUA_VERSION_NUM
#undef LUA_COPYRIGHT
#undef LUA_AUTHORS
#define LUA_VERSION     "Lua 5.1"
#define LUA_RELEASE     "Lua 5.1.5"
#define LUA_VERSION_NUM 501
#define LUA_COPYRIGHT   "Copyright (C) 1994-2012 Lua.org, PUC-Rio"
#define LUA_AUTHORS     "R. Ierusalimschy, L. H. de Figueiredo & W. Celes"

// Lua 5.1 type aliases
typedef const char* (*lua_Reader)(lua_State* L, void* ud, size_t* sz);
typedef int (*lua_Writer)(lua_State* L, const void* p, size_t sz, void* ud);

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// lua_pushcclosure / lua_pushcfunction: Lua 5.1 = (L, fn, nup), Luau = (L, fn, debugname, nup)
// ============================================================
#undef lua_pushcclosure
#undef lua_pushcfunction
#define lua_pushcclosure(L, fn, nup) lua_pushcclosurek(L, fn, NULL, nup, NULL)
#define lua_pushcfunction(L, fn) lua_pushcclosurek(L, fn, NULL, 0, NULL)

#undef lua_register
#define lua_register(L, n, f) (lua_pushcfunction(L, f), lua_setglobal(L, n))

// ============================================================
// lua_error: Luau returns void (noreturn), Lua 5.1 returns int
// ============================================================
LUA_API int lua51_compat_error(lua_State* L);
// Note: we do NOT redefine lua_error here because most code uses it correctly.
// The pattern `return lua_error(L)` works in both (noreturn is compatible).
// For code that needs the int return, use lua51_compat_error directly.

// ============================================================
// lua_atpanic
// ============================================================
LUA_API lua_CFunction lua51_compat_atpanic(lua_State* L, lua_CFunction panicf);
#define lua_atpanic(L, f) lua51_compat_atpanic(L, f)

// ============================================================
// Debug API: Lua 5.1 signatures differ from Luau
// ============================================================

// Hook constants
#ifndef LUA_HOOKCALL
#define LUA_HOOKCALL    0
#define LUA_HOOKRET     1
#define LUA_HOOKLINE    2
#define LUA_HOOKCOUNT   3
#define LUA_HOOKTAILRET 4
#endif

#ifndef LUA_MASKCALL
#define LUA_MASKCALL    (1 << LUA_HOOKCALL)
#define LUA_MASKRET     (1 << LUA_HOOKRET)
#define LUA_MASKLINE    (1 << LUA_HOOKLINE)
#define LUA_MASKCOUNT   (1 << LUA_HOOKCOUNT)
#endif

// lua_getstack: Lua 5.1 = (L, level, ar), stores level for lua_getinfo
LUA_API int lua51_compat_getstack(lua_State* L, int level, lua_Debug* ar);
#define lua_getstack(L, level, ar) lua51_compat_getstack(L, level, ar)

// lua_getinfo: Lua 5.1 = (L, what, ar), Luau = (L, level, what, ar)
LUA_API int lua51_compat_getinfo(lua_State* L, const char* what, lua_Debug* ar);
#undef lua_getinfo
#define lua_getinfo(L, what, ar) lua51_compat_getinfo(L, what, ar)

// lua_sethook / gethook / gethookmask / gethookcount
LUA_API int lua51_compat_sethook(lua_State* L, lua_Hook func, int mask, int count);
LUA_API lua_Hook lua51_compat_gethook(lua_State* L);
LUA_API int lua51_compat_gethookmask(lua_State* L);
LUA_API int lua51_compat_gethookcount(lua_State* L);
#define lua_sethook(L, f, mask, count) lua51_compat_sethook(L, f, mask, count)
#define lua_gethook(L) lua51_compat_gethook(L)
#define lua_gethookmask(L) lua51_compat_gethookmask(L)
#define lua_gethookcount(L) lua51_compat_gethookcount(L)

// lua_getlocal / lua_setlocal: Lua 5.1 = (L, ar, n), Luau = (L, level, n)
LUA_API const char* lua51_compat_getlocal(lua_State* L, const lua_Debug* ar, int n);
LUA_API const char* lua51_compat_setlocal(lua_State* L, const lua_Debug* ar, int n);
#undef lua_getlocal
#undef lua_setlocal
#define lua_getlocal(L, ar, n) lua51_compat_getlocal(L, ar, n)
#define lua_setlocal(L, ar, n) lua51_compat_setlocal(L, ar, n)

// ============================================================
// GC compat constants
// ============================================================
#ifndef LUA_GCSETPAUSE
#define LUA_GCSETPAUSE    6
#endif
#ifndef LUA_GCSETSTEPMUL
#define LUA_GCSETSTEPMUL  7
#endif

// ============================================================
// Stubs for functions not in Luau
// ============================================================
LUA_API void lua51_compat_setallocf(lua_State* L, lua_Alloc f, void* ud);
LUA_API void lua51_compat_setlevel(lua_State* from, lua_State* to);
#define lua_setallocf(L, f, ud) lua51_compat_setallocf(L, f, ud)
#define lua_setlevel(from, to) lua51_compat_setlevel(from, to)

// lua_load stub (Luau uses luau_load with different semantics)
LUA_API int lua51_compat_load(lua_State* L, lua_Reader reader, void* data, const char* chunkname);
#define lua_load(L, reader, data, name) lua51_compat_load(L, reader, data, name)

// lua_dump stub
LUA_API int lua51_compat_dump(lua_State* L, lua_Writer writer, void* data);
#define lua_dump(L, writer, data) lua51_compat_dump(L, writer, data)

// lua_resume: Lua 5.1 = (L, narg), Luau = (L, from, narg)
LUA_API int lua51_compat_resume(lua_State* L, int narg);
#undef lua_resume
#define lua_resume(L, narg) lua51_compat_resume(L, narg)

// lua_yield: Lua 5.1 = (L, nresults)
LUA_API int lua51_compat_yield(lua_State* L, int nresults);
#undef lua_yield
#define lua_yield(L, nresults) lua51_compat_yield(L, nresults)

// lua_newstate: Lua 5.1 = (f, ud), Luau = (f, ud)
// Signature matches but Luau's lua_Alloc might differ — provide a compat wrapper
LUA_API lua_State* lua51_compat_newstate(lua_Alloc f, void* ud);
#undef lua_newstate
#define lua_newstate(f, ud) lua51_compat_newstate(f, ud)

// lua_equal, lua_lessthan stubs
LUA_API int lua51_compat_equal(lua_State* L, int idx1, int idx2);
LUA_API int lua51_compat_lessthan(lua_State* L, int idx1, int idx2);
#define lua_equal(L, idx1, idx2) lua51_compat_equal(L, idx1, idx2)
#define lua_lessthan(L, idx1, idx2) lua51_compat_lessthan(L, idx1, idx2)

// lua_cpcall: Lua 5.1 function, not in Luau
LUA_API int lua51_compat_cpcall(lua_State* L, lua_CFunction func, void* ud);
#define lua_cpcall(L, func, ud) lua51_compat_cpcall(L, func, ud)

// lua_getfenv / lua_setfenv stubs
LUA_API void lua51_compat_getfenv(lua_State* L, int idx);
LUA_API int lua51_compat_setfenv(lua_State* L, int idx);
#define lua_getfenv(L, idx) lua51_compat_getfenv(L, idx)
#define lua_setfenv(L, idx) lua51_compat_setfenv(L, idx)

// ============================================================
// Lua 5.1 convenience macros
// ============================================================
#ifndef lua_strlen
#define lua_strlen(L, i) lua_objlen(L, (i))
#endif

#ifndef lua_open
#define lua_open() luaL_newstate()
#endif

#define lua_getregistry(L) lua_pushvalue(L, LUA_REGISTRYINDEX)
#define lua_getgccount(L) lua_gc(L, LUA_GCCOUNT, 0)

#ifdef __cplusplus
}
#endif

#endif
