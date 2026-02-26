/*
 * Lua 5.5 to Lua 5.1 API compatibility shim.
 *
 * Compiled WITH lua55 headers to wrap lua55_* functions to lua51 API.
 * This allows linking lua51 code against lua55 library.
 */

#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>

/* Define constants that differ from lua55 defaults before including headers */
#undef LUA_REGISTRYINDEX
#define LUA_REGISTRYINDEX   (-10000)

/* Include lua55 headers */
#include "lua55/lua.h"
#include "lua55/lauxlib.h"
#include "lua55/lualib.h"

/* Additional constants not in lua55 */
#define LUA_GLOBALSINDEX    (-10002)
#define LUA_ENVIRONINDEX    (-10001)

/* Type aliases - map lua55 types to lua51 names */
typedef lua55_State lua_State;
typedef lua55_Debug lua_Debug;
typedef lua55_Number lua_Number;
typedef lua55_Integer lua_Integer;
typedef lua55_Unsigned lua_Unsigned;
typedef lua55_KContext lua_KContext;
typedef lua55_CFunction lua_CFunction;
typedef lua55_KFunction lua_KFunction;
typedef lua55_Reader lua_Reader;
typedef lua55_Writer lua_Writer;
typedef lua55_Alloc lua_Alloc;
typedef lua55_WarnFunction lua_WarnFunction;
typedef lua55_Hook lua_Hook;

/* lauxlib types */
typedef lua55L_Buffer luaL_Buffer;
typedef lua55L_Reg luaL_Reg;

/* lua_upvalueindex macro */
#define lua_upvalueindex(i) lua55_upvalueindex(i)

/* Undef macros that conflict with function names */
#undef lua_objlen

/* Note: lua55 already defines these as macros - no need to redefine:
 * lua_pop, lua_newtable, lua_register, lua_pushcfunction, lua_strlen,
 * lua_isfunction, lua_istable, lua_isnil, lua_isboolean, lua_islightuserdata,
 * lua_isnumber, lua_isstring, lua_isthread, lua_getglobal, lua_setglobal,
 * lua_equal, lua_lessthan, lua_objlen
 */

#ifdef __cplusplus
extern "C" {
#endif

/* State management */
lua_State *lua_newstate(lua_Alloc f, void *ud) {
    return lua55_newstate(f, ud, 0);
}

void lua_close(lua_State *L) {
    lua55_close(L);
}

lua_State *lua_newthread(lua_State *L) {
    return lua55_newthread(L);
}

lua_CFunction lua_atpanic(lua_State *L, lua_CFunction panicf) {
    return lua55_atpanic(L, panicf);
}

lua_Alloc lua_getallocf(lua_State *L, void **ud) {
    return lua55_getallocf(L, ud);
}

void lua_setallocf(lua_State *L, lua_Alloc f, void *ud) {
    lua55_setallocf(L, f, ud);
}

/* Stack functions */
int lua_gettop(lua_State *L) {
    return lua55_gettop(L);
}

void lua_settop(lua_State *L, int idx) {
    lua55_settop(L, idx);
}

void lua_pushvalue(lua_State *L, int idx) {
    lua55_pushvalue(L, idx);
}

void lua_remove(lua_State *L, int idx) {
    int top = lua55_gettop(L);
    lua55_rotate(L, idx, 1);
    lua55_settop(L, top - 1);
}

void lua_insert(lua_State *L, int idx) {
    lua55_rotate(L, idx, 1);
}

void lua_replace(lua_State *L, int idx) {
    lua55_copy(L, -1, idx);
    lua55_settop(L, -2);
}

int lua_checkstack(lua_State *L, int sz) {
    return lua55_checkstack(L, sz);
}

void lua_xmove(lua_State *from, lua_State *to, int n) {
    lua55_xmove(from, to, n);
}

/* Push functions */
void lua_pushnil(lua_State *L) {
    lua55_pushnil(L);
}

void lua_pushnumber(lua_State *L, lua_Number n) {
    lua55_pushnumber(L, n);
}

void lua_pushinteger(lua_State *L, lua_Integer n) {
    lua55_pushinteger(L, n);
}

void lua_pushlstring(lua_State *L, const char *s, size_t len) {
    lua55_pushlstring(L, s, len);
}

void lua_pushstring(lua_State *L, const char *s) {
    lua55_pushstring(L, s);
}

const char *lua_pushvfstring(lua_State *L, const char *fmt, va_list argp) {
    return lua55_pushvfstring(L, fmt, argp);
}

const char *lua_pushfstring(lua_State *L, const char *fmt, ...) {
    va_list argp;
    va_start(argp, fmt);
    const char *s = lua55_pushvfstring(L, fmt, argp);
    va_end(argp);
    return s;
}

void lua_pushcclosure(lua_State *L, lua_CFunction fn, int n) {
    lua55_pushcclosure(L, fn, n);
}

void lua_pushboolean(lua_State *L, int b) {
    lua55_pushboolean(L, b);
}

void lua_pushlightuserdata(lua_State *L, void *p) {
    lua55_pushlightuserdata(L, p);
}

int lua_pushthread(lua_State *L) {
    return lua55_pushthread(L);
}

/* Get functions - lua55 has these as macros, but we need functions too */
int lua_type(lua_State *L, int idx) {
    return lua55_type(L, idx);
}

const char *lua_typename(lua_State *L, int t) {
    return lua55_typename(L, t);
}

lua_Number lua_tonumberx(lua_State *L, int idx, int *isnum) {
    return lua55_tonumberx(L, idx, isnum);
}

lua_Number lua_tonumber(lua_State *L, int idx) {
    return lua55_tonumberx(L, idx, NULL);
}

lua_Integer lua_tointegerx(lua_State *L, int idx, int *isnum) {
    return lua55_tointegerx(L, idx, isnum);
}

lua_Integer lua_tointeger(lua_State *L, int idx) {
    return lua55_tointegerx(L, idx, NULL);
}

int lua_isnumber(lua_State *L, int idx) {
    return lua55_isnumber(L, idx);
}

int lua_isstring(lua_State *L, int idx) {
    return lua55_isstring(L, idx);
}

int lua_iscfunction(lua_State *L, int idx) {
    return lua55_iscfunction(L, idx);
}

int lua_isuserdata(lua_State *L, int idx) {
    return lua55_isuserdata(L, idx);
}

int lua_rawequal(lua_State *L, int idx1, int idx2) {
    return lua55_rawequal(L, idx1, idx2);
}

int lua_toboolean(lua_State *L, int idx) {
    return lua55_toboolean(L, idx);
}

const char *lua_tolstring(lua_State *L, int idx, size_t *len) {
    return lua55_tolstring(L, idx, len);
}

size_t lua_rawlen(lua_State *L, int idx) {
    return lua55_rawlen(L, idx);
}

size_t lua_objlen(lua_State *L, int idx) {
    return lua55_rawlen(L, idx);
}

lua_CFunction lua_tocfunction(lua_State *L, int idx) {
    return lua55_tocfunction(L, idx);
}

void *lua_touserdata(lua_State *L, int idx) {
    return lua55_touserdata(L, idx);
}

lua_State *lua_tothread(lua_State *L, int idx) {
    return lua55_tothread(L, idx);
}

const void *lua_topointer(lua_State *L, int idx) {
    return lua55_topointer(L, idx);
}

/* Table functions */
void lua_gettable(lua_State *L, int idx) {
    lua55_gettable(L, idx);
}

void lua_getfield(lua_State *L, int idx, const char *k) {
    if (idx == LUA_GLOBALSINDEX) {
        lua55_getglobal(L, k);
    } else {
        lua55_getfield(L, idx, k);
    }
}

void lua_rawget(lua_State *L, int idx) {
    lua55_rawget(L, idx);
}

void lua_rawgeti(lua_State *L, int idx, lua_Integer n) {
    lua55_rawgeti(L, idx, n);
}

void lua_createtable(lua_State *L, int narr, int nrec) {
    lua55_createtable(L, narr, nrec);
}

void lua_newuserdata(lua_State *L, size_t sz) {
    lua55_newuserdatauv(L, sz, 1);
}

int lua_getmetatable(lua_State *L, int objindex) {
    return lua55_getmetatable(L, objindex);
}

void lua_settable(lua_State *L, int idx) {
    lua55_settable(L, idx);
}

void lua_setfield(lua_State *L, int idx, const char *k) {
    if (idx == LUA_GLOBALSINDEX) {
        lua55_setglobal(L, k);
    } else {
        lua55_setfield(L, idx, k);
    }
}

void lua_rawset(lua_State *L, int idx) {
    lua55_rawset(L, idx);
}

void lua_rawseti(lua_State *L, int idx, lua_Integer n) {
    lua55_rawseti(L, idx, n);
}

int lua_setmetatable(lua_State *L, int objindex) {
    return lua55_setmetatable(L, objindex);
}

/* Call functions */
void lua_call(lua_State *L, int nargs, int nresults) {
    lua55_callk(L, nargs, nresults, 0, NULL);
}

int lua_pcall(lua_State *L, int nargs, int nresults, int msgh) {
    return lua55_pcallk(L, nargs, nresults, msgh, 0, NULL);
}

int lua_cpcall(lua_State *L, lua_CFunction func, void *ud) {
    lua55_pushcclosure(L, func, 0);
    lua55_pushlightuserdata(L, ud);
    return lua55_pcallk(L, 1, 0, 0, 0, NULL);
}

/* lua_load - lua55 has mode parameter, pass NULL for default */
int lua_load(lua_State *L, lua_Reader reader, void *data, const char *chunkname) {
    return lua55_load(L, reader, data, chunkname, NULL);
}

/* lua_dump - lua55 has strip parameter, pass 0 */
int lua_dump(lua_State *L, lua_Writer writer, void *data) {
    return lua55_dump(L, writer, data, 0);
}

/* Coroutine functions */
int lua_yield(lua_State *L, int nresults) {
    return lua55_yieldk(L, nresults, 0, NULL);
}

int lua_resume(lua_State *L, int narg) {
    int nres = 0;
    return lua55_resume(L, NULL, narg, &nres);
}

int lua_status(lua_State *L) {
    return lua55_status(L);
}

int lua_isyieldable(lua_State *L) {
    return lua55_isyieldable(L);
}

/* GC functions */
int lua_gc(lua_State *L, int what, ...) {
    va_list argp;
    va_start(argp, what);
    int result = 0;
    
    if (what == LUA_GCSTOP || what == LUA_GCRESTART || what == LUA_GCCOLLECT) {
        result = lua55_gc(L, what);
    }
    else if (what == LUA_GCCOUNT || what == LUA_GCCOUNTB || what == LUA_GCSTEP) {
        int data = va_arg(argp, int);
        result = lua55_gc(L, what, data);
    }
    else if (what == 6) {
        int data = va_arg(argp, int);
        result = lua55_gc(L, LUA_GCPARAM, LUA_GCPPAUSE, data);
    }
    else if (what == 7) {
        int data = va_arg(argp, int);
        result = lua55_gc(L, LUA_GCPARAM, LUA_GCPSTEPMUL, data);
    }
    else if (what == LUA_GCISRUNNING || what == LUA_GCGEN || what == LUA_GCINC) {
        result = lua55_gc(L, what);
    }
    else {
        result = lua55_gc(L, what);
    }
    
    va_end(argp);
    return result;
}

/* Error and misc functions */
int lua_error(lua_State *L) {
    return lua55_error(L);
}

int lua_next(lua_State *L, int idx) {
    return lua55_next(L, idx);
}

void lua_concat(lua_State *L, int n) {
    lua55_concat(L, n);
}

void lua_len(lua_State *L, int idx) {
    lua55_len(L, idx);
}

/* Debug API */
int lua_getstack(lua_State *L, int level, lua_Debug *ar) {
    return lua55_getstack(L, level, ar);
}

int lua_getinfo(lua_State *L, const char *what, lua_Debug *ar) {
    return lua55_getinfo(L, what, ar);
}

const char *lua_getlocal(lua_State *L, const lua_Debug *ar, int n) {
    return lua55_getlocal(L, ar, n);
}

const char *lua_setlocal(lua_State *L, const lua_Debug *ar, int n) {
    return lua55_setlocal(L, ar, n);
}

const char *lua_getupvalue(lua_State *L, int funcindex, int n) {
    return lua55_getupvalue(L, funcindex, n);
}

const char *lua_setupvalue(lua_State *L, int funcindex, int n) {
    return lua55_setupvalue(L, funcindex, n);
}

void lua_sethook(lua_State *L, lua_Hook func, int mask, int count) {
    lua55_sethook(L, func, mask, count);
}

lua_Hook lua_gethook(lua_State *L) {
    return lua55_gethook(L);
}

int lua_gethookmask(lua_State *L) {
    return lua55_gethookmask(L);
}

int lua_gethookcount(lua_State *L) {
    return lua55_gethookcount(L);
}

/* lauxlib functions */
lua_State *luaL_newstate(void) {
    return lua55L_newstate();
}

void luaL_register(lua_State *L, const char *libname, const luaL_Reg *l) {
    if (libname) {
        lua55_createtable(L, 0, 0);
        lua55L_setfuncs(L, l, 0);
        lua55_setglobal(L, libname);
    } else {
        lua55L_setfuncs(L, l, 0);
    }
}

void luaL_openlibs(lua_State *L) {
    lua55L_openlibs(L);
}

void luaL_checkversion_(lua_State *L, lua_Number ver, size_t sz) {
    lua55L_checkversion_(L, ver, sz);
}

int luaL_getmetafield(lua_State *L, int obj, const char *e) {
    return lua55L_getmetafield(L, obj, e);
}

int luaL_callmeta(lua_State *L, int obj, const char *e) {
    return lua55L_callmeta(L, obj, e);
}

const char *luaL_typename(lua_State *L, int tp) {
    return lua55_typename(L, tp);
}

int luaL_error(lua_State *L, const char *fmt, ...) {
    va_list argp;
    va_start(argp, fmt);
    int result = lua55L_error(L, fmt, argp);
    va_end(argp);
    return result;
}

lua_Integer luaL_checkinteger(lua_State *L, int arg) {
    return lua55L_checkinteger(L, arg);
}

lua_Integer luaL_optinteger(lua_State *L, int arg, lua_Integer def) {
    return lua55L_optinteger(L, arg, def);
}

lua_Number luaL_checknumber(lua_State *L, int arg) {
    return lua55L_checknumber(L, arg);
}

lua_Number luaL_optnumber(lua_State *L, int arg, lua_Number def) {
    return lua55L_optnumber(L, arg, def);
}

const char *luaL_checklstring(lua_State *L, int arg, size_t *l) {
    return lua55L_checklstring(L, arg, l);
}

const char *luaL_optlstring(lua_State *L, int arg, const char *d, size_t *l) {
    return lua55L_optlstring(L, arg, d, l);
}

void luaL_checkstack(lua_State *L, int sz, const char *msg) {
    lua55L_checkstack(L, sz, msg);
}

void luaL_checktype(lua_State *L, int arg, int t) {
    lua55L_checktype(L, arg, t);
}

void luaL_checkany(lua_State *L, int arg) {
    lua55L_checkany(L, arg);
}

int luaL_argerror(lua_State *L, int narg, const char *extramsg) {
    return lua55L_argerror(L, narg, extramsg);
}

void luaL_where(lua_State *L, int lvl) {
    lua55L_where(L, lvl);
}

int luaL_ref(lua_State *L, int t) {
    return lua55L_ref(L, t);
}

void luaL_unref(lua_State *L, int t, int ref) {
    lua55L_unref(L, t, ref);
}

int luaL_newmetatable(lua_State *L, const char *tname) {
    return lua55L_newmetatable(L, tname);
}

void luaL_setmetatable(lua_State *L, const char *tname) {
    lua55L_setmetatable(L, tname);
}

void *luaL_testudata(lua_State *L, int ud, const char *tname) {
    return lua55L_testudata(L, ud, tname);
}

void *luaL_checkudata(lua_State *L, int ud, const char *tname) {
    return lua55L_checkudata(L, ud, tname);
}

int luaL_checkoption(lua_State *L, int arg, const char *def, const char *const lst[]) {
    return lua55L_checkoption(L, arg, def, lst);
}

int luaL_loadfilex(lua_State *L, const char *filename, const char *mode) {
    return lua55L_loadfilex(L, filename, mode);
}

int luaL_loadfile(lua_State *L, const char *filename) {
    return lua55L_loadfilex(L, filename, NULL);
}

int luaL_loadbufferx(lua_State *L, const char *buff, size_t sz, const char *name, const char *mode) {
    return lua55L_loadbufferx(L, buff, sz, name, mode);
}

int luaL_loadbuffer(lua_State *L, const char *buff, size_t sz, const char *name) {
    return lua55L_loadbufferx(L, buff, sz, name, NULL);
}

int luaL_loadstring(lua_State *L, const char *s) {
    return lua55L_loadstring(L, s);
}

void luaL_gsub(lua_State *L, const char *s, const char *p, const char *r) {
    lua55L_gsub(L, s, p, r);
}

void luaL_setfuncs(lua_State *L, const luaL_Reg *l, int nup) {
    lua55L_setfuncs(L, l, nup);
}

int luaL_getsubtable(lua_State *L, int idx, const char *fname) {
    return lua55L_getsubtable(L, idx, fname);
}

void luaL_traceback(lua_State *L, lua_State *L1, const char *msg, int level) {
    lua55L_traceback(L, L1, msg, level);
}

void luaL_requiref(lua_State *L, const char *modname, lua_CFunction openf, int glb) {
    lua55L_requiref(L, modname, openf, glb);
}

/* Buffer functions */
void luaL_buffinit(lua_State *L, luaL_Buffer *B) {
    lua55L_buffinit(L, B);
}

char *luaL_prepbuffsize(luaL_Buffer *B, size_t sz) {
    return lua55L_prepbuffsize(B, sz);
}

void luaL_addlstring(luaL_Buffer *B, const char *s, size_t l) {
    lua55L_addlstring(B, s, l);
}

void luaL_addstring(luaL_Buffer *B, const char *s) {
    lua55L_addstring(B, s);
}

void luaL_addvalue(luaL_Buffer *B) {
    lua55L_addvalue(B);
}

void luaL_pushresult(luaL_Buffer *B) {
    lua55L_pushresult(B);
}

void luaL_pushresultsize(luaL_Buffer *B, size_t sz) {
    lua55L_pushresultsize(B, sz);
}

#define luaL_buffinitsize(L,B,sz) luaL_prepbuffsize(B,sz)

char *luaL_prepbuffer(luaL_Buffer *B) {
    return lua55L_prepbuffsize(B, LUAL_BUFFERSIZE);
}

/* Standard library open functions - redirect to lua55 versions */
int luaopen_base(lua_State *L) {
    return lua55open_base(L);
}

int luaopen_package(lua_State *L) {
    return lua55open_package(L);
}

int luaopen_coroutine(lua_State *L) {
    return lua55open_coroutine(L);
}

int luaopen_table(lua_State *L) {
    return lua55open_table(L);
}

int luaopen_io(lua_State *L) {
    return lua55open_io(L);
}

int luaopen_os(lua_State *L) {
    return lua55open_os(L);
}

int luaopen_string(lua_State *L) {
    return lua55open_string(L);
}

int luaopen_math(lua_State *L) {
    return lua55open_math(L);
}

int luaopen_utf8(lua_State *L) {
    return lua55open_utf8(L);
}

int luaopen_debug(lua_State *L) {
    return lua55open_debug(L);
}

#ifdef __cplusplus
}
#endif
