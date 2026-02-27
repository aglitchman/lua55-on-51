/*
 * Lua 5.5 → Lua 5.1 API compatibility shim.
 *
 * Compiled as C (-x c) with lua55 headers (-I.).
 * All exported symbols match the lua51 ABI so that application code
 * compiled with lua51/src headers can link against this + lua55/liblua.a.
 */

#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* Include lua55 headers — LUA_REGISTRYINDEX gets the lua55 value */
#include "lua55/lua.h"
#include "lua55/lauxlib.h"
#include "lua55/lualib.h"

/* Undefine lua55 compat macros that conflict with our function names */
#undef lua_equal
#undef lua_lessthan
#undef lua_objlen
#undef lua_strlen

/* ── lua 5.1 pseudo-index constants ────────────────────────────── */
#define LUA51_REGISTRYINDEX  (-10000)
#define LUA51_ENVIRONINDEX   (-10001)
#define LUA51_GLOBALSINDEX   (-10002)

/* lua 5.1 GC opcodes that differ from lua55 */
#define LUA51_GCSETPAUSE     6
#define LUA51_GCSETSTEPMUL   7

/* ── Type aliases (lua55 → lua51 names) ───────────────────────── */
typedef lua55_State       lua_State;
/* lua_Number, lua_Integer, etc. already typedef'd at bottom of lua55/lua.h */

/* ── Translate lua51 pseudo-indices to lua55 equivalents ──────── */
/* Returns the translated index.  For LUA51_GLOBALSINDEX the caller
   must handle the case specially (push the global table). */
#define IS_PSEUDO51(idx) ((idx) <= LUA51_REGISTRYINDEX)

static int xidx(int idx) {
    if (idx > LUA51_REGISTRYINDEX)      return idx;  /* normal stack index */
    if (idx == LUA51_REGISTRYINDEX)     return LUA_REGISTRYINDEX;
    if (idx == LUA51_ENVIRONINDEX)      return LUA_REGISTRYINDEX; /* stub */
    if (idx == LUA51_GLOBALSINDEX)      return 0;    /* sentinel – caller handles */
    /* upvalue index: lua51 encodes as (-10002 - i) */
    return lua55_upvalueindex(LUA51_GLOBALSINDEX - idx);
}

/* Push the global table onto the stack (lua55 equivalent of LUA_GLOBALSINDEX) */
static void push_globaltable(lua_State *L) {
    lua55_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
}

/* ── lua51_Debug layout ──────────────────────────────────────── */
/* The application allocates this (smaller) struct; our functions
   must not write beyond it. */
struct lua51_Debug {
    int event;
    const char *name;
    const char *namewhat;
    const char *what;
    const char *source;
    int currentline;
    int nups;
    int linedefined;
    int lastlinedefined;
    char short_src[60];
    int i_ci;
};

/* ── lua51_Buffer layout ─────────────────────────────────────── */
/* LUAL_BUFFERSIZE in lua51 = BUFSIZ (typically 8192).
   We replicate the exact lua51 struct so macros like luaL_addchar
   (which access ->p and ->buffer directly) work correctly. */
#ifndef LUA51_BUFFERSIZE
#define LUA51_BUFFERSIZE  8192
#endif

struct lua51_Buffer {
    char *p;
    int   lvl;
    lua_State *L;
    char  buffer[LUA51_BUFFERSIZE];
};

/* ── Keep one lua55_Debug for getstack↔getinfo pairing ───────── */
static lua55_Debug g_debug55;

/* ── Hook wrapper ────────────────────────────────────────────── */
typedef void (*lua51_Hook)(lua_State *L, struct lua51_Debug *ar);
static lua51_Hook g_user_hook;

static void hook_bridge(lua_State *L, lua55_Debug *ar) {
    if (!g_user_hook) return;
    struct lua51_Debug ar51;
    ar51.event        = ar->event;
    ar51.name         = ar->name;
    ar51.namewhat     = ar->namewhat;
    ar51.what         = ar->what;
    ar51.source       = ar->source;
    ar51.currentline  = ar->currentline;
    ar51.nups         = (int)ar->nups;
    ar51.linedefined  = ar->linedefined;
    ar51.lastlinedefined = ar->lastlinedefined;
    memcpy(ar51.short_src, ar->short_src, sizeof(ar51.short_src));
    ar51.i_ci         = 0;
    g_user_hook(L, &ar51);
}

/* ================================================================
 *  State management
 * ================================================================ */

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

/* ================================================================
 *  Stack manipulation
 * ================================================================ */

int lua_gettop(lua_State *L) {
    return lua55_gettop(L);
}

void lua_settop(lua_State *L, int idx) {
    lua55_settop(L, idx);
}

void lua_pushvalue(lua_State *L, int idx) {
    if (idx == LUA51_GLOBALSINDEX)  { push_globaltable(L); return; }
    if (idx == LUA51_REGISTRYINDEX) { lua55_pushvalue(L, LUA_REGISTRYINDEX); return; }
    lua55_pushvalue(L, IS_PSEUDO51(idx) ? xidx(idx) : idx);
}

void lua_remove(lua_State *L, int idx) {
    lua55_rotate(L, idx, -1);
    lua55_settop(L, -2);
}

void lua_insert(lua_State *L, int idx) {
    lua55_rotate(L, idx, 1);
}

void lua_replace(lua_State *L, int idx) {
    if (idx == LUA51_GLOBALSINDEX) {
        /* set global table from top of stack */
        lua55_rawseti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
        return;
    }
    int i55 = IS_PSEUDO51(idx) ? xidx(idx) : idx;
    lua55_copy(L, -1, i55);
    lua55_settop(L, -2);
}

int lua_checkstack(lua_State *L, int sz) {
    return lua55_checkstack(L, sz);
}

void lua_xmove(lua_State *from, lua_State *to, int n) {
    lua55_xmove(from, to, n);
}

/* ================================================================
 *  Access functions (stack → C)
 * ================================================================ */

int lua_type(lua_State *L, int idx) {
    if (idx == LUA51_GLOBALSINDEX)  return LUA_TTABLE;
    if (idx == LUA51_ENVIRONINDEX)  return LUA_TTABLE;
    if (idx == LUA51_REGISTRYINDEX) return LUA_TTABLE;
    return lua55_type(L, IS_PSEUDO51(idx) ? xidx(idx) : idx);
}

const char *lua_typename(lua_State *L, int t) {
    return lua55_typename(L, t);
}

lua_Number lua_tonumber(lua_State *L, int idx) {
    return lua55_tonumberx(L, IS_PSEUDO51(idx) ? xidx(idx) : idx, NULL);
}

lua_Number lua_tonumberx(lua_State *L, int idx, int *isnum) {
    return lua55_tonumberx(L, IS_PSEUDO51(idx) ? xidx(idx) : idx, isnum);
}

lua_Integer lua_tointeger(lua_State *L, int idx) {
    /* lua51 truncates any number to integer; lua55 returns 0 for non-integers */
    int i55 = IS_PSEUDO51(idx) ? xidx(idx) : idx;
    int isnum;
    lua_Number n = lua55_tonumberx(L, i55, &isnum);
    if (isnum) return (lua_Integer)n;
    return 0;
}

lua_Integer lua_tointegerx(lua_State *L, int idx, int *isnum) {
    return lua55_tointegerx(L, IS_PSEUDO51(idx) ? xidx(idx) : idx, isnum);
}

int lua_isnumber(lua_State *L, int idx) {
    return lua55_isnumber(L, IS_PSEUDO51(idx) ? xidx(idx) : idx);
}

int lua_isstring(lua_State *L, int idx) {
    return lua55_isstring(L, IS_PSEUDO51(idx) ? xidx(idx) : idx);
}

int lua_iscfunction(lua_State *L, int idx) {
    return lua55_iscfunction(L, IS_PSEUDO51(idx) ? xidx(idx) : idx);
}

int lua_isuserdata(lua_State *L, int idx) {
    return lua55_isuserdata(L, IS_PSEUDO51(idx) ? xidx(idx) : idx);
}

int lua_rawequal(lua_State *L, int idx1, int idx2) {
    return lua55_rawequal(L,
        IS_PSEUDO51(idx1) ? xidx(idx1) : idx1,
        IS_PSEUDO51(idx2) ? xidx(idx2) : idx2);
}

int lua_equal(lua_State *L, int idx1, int idx2) {
    return lua55_compare(L,
        IS_PSEUDO51(idx1) ? xidx(idx1) : idx1,
        IS_PSEUDO51(idx2) ? xidx(idx2) : idx2,
        LUA_OPEQ);
}

int lua_lessthan(lua_State *L, int idx1, int idx2) {
    return lua55_compare(L,
        IS_PSEUDO51(idx1) ? xidx(idx1) : idx1,
        IS_PSEUDO51(idx2) ? xidx(idx2) : idx2,
        LUA_OPLT);
}

int lua_toboolean(lua_State *L, int idx) {
    return lua55_toboolean(L, IS_PSEUDO51(idx) ? xidx(idx) : idx);
}

const char *lua_tolstring(lua_State *L, int idx, size_t *len) {
    return lua55_tolstring(L, IS_PSEUDO51(idx) ? xidx(idx) : idx, len);
}

size_t lua_rawlen(lua_State *L, int idx) {
    return (size_t)lua55_rawlen(L, IS_PSEUDO51(idx) ? xidx(idx) : idx);
}

size_t lua_objlen(lua_State *L, int idx) {
    return (size_t)lua55_rawlen(L, IS_PSEUDO51(idx) ? xidx(idx) : idx);
}

lua_CFunction lua_tocfunction(lua_State *L, int idx) {
    return lua55_tocfunction(L, IS_PSEUDO51(idx) ? xidx(idx) : idx);
}

void *lua_touserdata(lua_State *L, int idx) {
    return lua55_touserdata(L, IS_PSEUDO51(idx) ? xidx(idx) : idx);
}

lua_State *lua_tothread(lua_State *L, int idx) {
    return lua55_tothread(L, IS_PSEUDO51(idx) ? xidx(idx) : idx);
}

const void *lua_topointer(lua_State *L, int idx) {
    return lua55_topointer(L, IS_PSEUDO51(idx) ? xidx(idx) : idx);
}

/* ================================================================
 *  Push functions (C → stack)
 * ================================================================ */

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

/* ================================================================
 *  Get functions (Lua → stack)
 * ================================================================ */

void lua_gettable(lua_State *L, int idx) {
    if (idx == LUA51_GLOBALSINDEX) {
        push_globaltable(L);
        lua55_insert(L, -2);   /* key is now on top, table below */
        lua55_gettable(L, -2);
        lua55_remove(L, -2);   /* remove global table */
        return;
    }
    lua55_gettable(L, IS_PSEUDO51(idx) ? xidx(idx) : idx);
}

void lua_getfield(lua_State *L, int idx, const char *k) {
    if (idx == LUA51_GLOBALSINDEX) {
        lua55_getglobal(L, k);
        return;
    }
    lua55_getfield(L, IS_PSEUDO51(idx) ? xidx(idx) : idx, k);
}

void lua_rawget(lua_State *L, int idx) {
    lua55_rawget(L, IS_PSEUDO51(idx) ? xidx(idx) : idx);
}

void lua_rawgeti(lua_State *L, int idx, int n) {
    lua55_rawgeti(L, IS_PSEUDO51(idx) ? xidx(idx) : idx, (lua55_Integer)n);
}

void lua_createtable(lua_State *L, int narr, int nrec) {
    lua55_createtable(L, narr, nrec);
}

void *lua_newuserdata(lua_State *L, size_t sz) {
    return lua55_newuserdatauv(L, sz, 1);
}

int lua_getmetatable(lua_State *L, int objindex) {
    return lua55_getmetatable(L, IS_PSEUDO51(objindex) ? xidx(objindex) : objindex);
}

void lua_getfenv(lua_State *L, int idx) {
    (void)idx;
    lua55_pushnil(L);
}

/* ================================================================
 *  Set functions (stack → Lua)
 * ================================================================ */

void lua_settable(lua_State *L, int idx) {
    if (idx == LUA51_GLOBALSINDEX) {
        push_globaltable(L);
        lua55_insert(L, -3);   /* table below key and value */
        lua55_settable(L, -3);
        lua55_settop(L, -2);   /* remove global table */
        return;
    }
    lua55_settable(L, IS_PSEUDO51(idx) ? xidx(idx) : idx);
}

void lua_setfield(lua_State *L, int idx, const char *k) {
    if (idx == LUA51_GLOBALSINDEX) {
        lua55_setglobal(L, k);
        return;
    }
    lua55_setfield(L, IS_PSEUDO51(idx) ? xidx(idx) : idx, k);
}

void lua_rawset(lua_State *L, int idx) {
    lua55_rawset(L, IS_PSEUDO51(idx) ? xidx(idx) : idx);
}

void lua_rawseti(lua_State *L, int idx, int n) {
    lua55_rawseti(L, IS_PSEUDO51(idx) ? xidx(idx) : idx, (lua55_Integer)n);
}

int lua_setmetatable(lua_State *L, int objindex) {
    return lua55_setmetatable(L, IS_PSEUDO51(objindex) ? xidx(objindex) : objindex);
}

int lua_setfenv(lua_State *L, int idx) {
    (void)idx;
    lua55_settop(L, -2);   /* pop the value that would have been the env */
    return 0;
}

/* ================================================================
 *  Call functions
 * ================================================================ */

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

int lua_load(lua_State *L, lua_Reader reader, void *data, const char *chunkname) {
    return lua55_load(L, reader, data, chunkname, NULL);
}

int lua_dump(lua_State *L, lua_Writer writer, void *data) {
    return lua55_dump(L, writer, data, 0);
}

/* ================================================================
 *  Coroutine functions
 * ================================================================ */

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

/* ================================================================
 *  GC — lua51 signature: int lua_gc(L, what, data)
 * ================================================================ */

int lua_gc(lua_State *L, int what, int data) {
    switch (what) {
        case LUA_GCSTOP:
        case LUA_GCRESTART:
        case LUA_GCCOLLECT:
            return lua55_gc(L, what);
        case LUA_GCCOUNT:
        case LUA_GCCOUNTB:
            return lua55_gc(L, what);
        case LUA_GCSTEP:
            return lua55_gc(L, what, data);
        case LUA51_GCSETPAUSE:
            return lua55_gc(L, LUA_GCPARAM, LUA_GCPPAUSE, data, 0);
        case LUA51_GCSETSTEPMUL:
            return lua55_gc(L, LUA_GCPARAM, LUA_GCPSTEPMUL, data, 0);
        default:
            return lua55_gc(L, what);
    }
}

/* ================================================================
 *  Miscellaneous
 * ================================================================ */

int lua_error(lua_State *L) {
    return lua55_error(L);
}

int lua_next(lua_State *L, int idx) {
    return lua55_next(L, IS_PSEUDO51(idx) ? xidx(idx) : idx);
}

void lua_concat(lua_State *L, int n) {
    lua55_concat(L, n);
}

void lua_len(lua_State *L, int idx) {
    lua55_len(L, IS_PSEUDO51(idx) ? xidx(idx) : idx);
}

/* lua51 compatibility */
void lua_setlevel(lua_State *from, lua_State *to) {
    (void)from; (void)to;
}

/* ================================================================
 *  Debug API
 * ================================================================ */

int lua_getstack(lua_State *L, int level, void *ar_raw) {
    (void)ar_raw;
    return lua55_getstack(L, level, &g_debug55);
}

int lua_getinfo(lua_State *L, const char *what, void *ar_raw) {
    struct lua51_Debug *ar51 = (struct lua51_Debug *)ar_raw;
    int result = lua55_getinfo(L, what, &g_debug55);
    if (result) {
        ar51->event           = g_debug55.event;
        ar51->name            = g_debug55.name;
        ar51->namewhat        = g_debug55.namewhat;
        ar51->what            = g_debug55.what;
        ar51->source          = g_debug55.source;
        ar51->currentline     = g_debug55.currentline;
        ar51->nups            = (int)g_debug55.nups;
        ar51->linedefined     = g_debug55.linedefined;
        ar51->lastlinedefined = g_debug55.lastlinedefined;
        memcpy(ar51->short_src, g_debug55.short_src, sizeof(ar51->short_src));
        ar51->i_ci            = 0;
    }
    return result;
}

const char *lua_getlocal(lua_State *L, const void *ar, int n) {
    (void)ar;
    return lua55_getlocal(L, &g_debug55, n);
}

const char *lua_setlocal(lua_State *L, const void *ar, int n) {
    (void)ar;
    return lua55_setlocal(L, &g_debug55, n);
}

const char *lua_getupvalue(lua_State *L, int funcindex, int n) {
    return lua55_getupvalue(L, IS_PSEUDO51(funcindex) ? xidx(funcindex) : funcindex, n);
}

const char *lua_setupvalue(lua_State *L, int funcindex, int n) {
    return lua55_setupvalue(L, IS_PSEUDO51(funcindex) ? xidx(funcindex) : funcindex, n);
}

void lua_sethook(lua_State *L, void *func, int mask, int count) {
    g_user_hook = (lua51_Hook)func;
    if (func) {
        lua55_sethook(L, hook_bridge, mask, count);
    } else {
        lua55_sethook(L, NULL, mask, count);
    }
}

lua55_Hook lua_gethook(lua_State *L) {
    return lua55_gethook(L);
}

int lua_gethookmask(lua_State *L) {
    return lua55_gethookmask(L);
}

int lua_gethookcount(lua_State *L) {
    return lua55_gethookcount(L);
}

/* ================================================================
 *  Auxiliary library  (luaL_*)
 * ================================================================ */

lua_State *luaL_newstate(void) {
    return lua55L_newstate();
}

void luaL_register(lua_State *L, const char *libname, const luaL_Reg *l) {
    if (libname) {
        /* reuse existing table or create new one */
        lua55_getglobal(L, libname);
        if (lua55_type(L, -1) != LUA_TTABLE) {
            lua55_settop(L, -2);
            lua55_createtable(L, 0, 0);
        }
        lua55L_setfuncs(L, l, 0);
        lua55_pushvalue(L, -1);
        lua55_setglobal(L, libname);
        /* table remains on stack */
    } else {
        lua55L_setfuncs(L, l, 0);
    }
}

void luaL_openlib(lua_State *L, const char *libname, const luaL_Reg *l, int nup) {
    if (libname) {
        /* reuse existing global table or create a new one */
        lua55_getglobal(L, libname);
        if (lua55_type(L, -1) != LUA_TTABLE) {
            lua55_settop(L, -2);         /* pop non-table */
            lua55_createtable(L, 0, 0);
        }
        lua55_insert(L, -(nup + 1));     /* move table below upvalues */
        lua55L_setfuncs(L, l, nup);      /* register funcs (pops upvalues) */
        lua55_pushvalue(L, -1);
        lua55_setglobal(L, libname);
    } else {
        lua55L_setfuncs(L, l, nup);
    }
}

/* Forward declarations for compat51 functions (defined below) */
static int compat51_unpack(lua_State *L);
static int compat51_loadstring(lua_State *L);
static int compat51_gcinfo(lua_State *L);
static int compat51_tostring(lua_State *L);

int luaL_getmetafield(lua_State *L, int obj, const char *e) {
    return lua55L_getmetafield(L, IS_PSEUDO51(obj) ? xidx(obj) : obj, e);
}

int luaL_callmeta(lua_State *L, int obj, const char *e) {
    return lua55L_callmeta(L, IS_PSEUDO51(obj) ? xidx(obj) : obj, e);
}

int luaL_error(lua_State *L, const char *fmt, ...) {
    va_list argp;
    va_start(argp, fmt);
    lua55_pushvfstring(L, fmt, argp);
    va_end(argp);
    return lua55_error(L);
}

int luaL_typerror(lua_State *L, int narg, const char *tname) {
    return lua55L_typeerror(L, narg, tname);
}

int luaL_argerror(lua_State *L, int narg, const char *extramsg) {
    return lua55L_argerror(L, narg, extramsg);
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

int luaL_newmetatable(lua_State *L, const char *tname) {
    return lua55L_newmetatable(L, tname);
}

void luaL_setmetatable(lua_State *L, const char *tname) {
    lua55L_setmetatable(L, tname);
}

void *luaL_testudata(lua_State *L, int ud, const char *tname) {
    return lua55L_testudata(L, IS_PSEUDO51(ud) ? xidx(ud) : ud, tname);
}

void *luaL_checkudata(lua_State *L, int ud, const char *tname) {
    return lua55L_checkudata(L, IS_PSEUDO51(ud) ? xidx(ud) : ud, tname);
}

int luaL_checkoption(lua_State *L, int arg, const char *def, const char *const lst[]) {
    return lua55L_checkoption(L, arg, def, lst);
}

void luaL_where(lua_State *L, int lvl) {
    lua55L_where(L, lvl);
}

int luaL_ref(lua_State *L, int t) {
    return lua55L_ref(L, IS_PSEUDO51(t) ? xidx(t) : t);
}

void luaL_unref(lua_State *L, int t, int ref) {
    lua55L_unref(L, IS_PSEUDO51(t) ? xidx(t) : t, ref);
}

int luaL_loadfile(lua_State *L, const char *filename) {
    return lua55L_loadfilex(L, filename, NULL);
}

int luaL_loadbuffer(lua_State *L, const char *buff, size_t sz, const char *name) {
    return lua55L_loadbufferx(L, buff, sz, name, NULL);
}

int luaL_loadstring(lua_State *L, const char *s) {
    return lua55L_loadstring(L, s);
}

const char *luaL_gsub(lua_State *L, const char *s, const char *p, const char *r) {
    lua55L_gsub(L, s, p, r);
    return lua55_tostring(L, -1);
}

void luaL_setfuncs(lua_State *L, const luaL_Reg *l, int nup) {
    lua55L_setfuncs(L, l, nup);
}

int luaL_getsubtable(lua_State *L, int idx, const char *fname) {
    return lua55L_getsubtable(L, IS_PSEUDO51(idx) ? xidx(idx) : idx, fname);
}

void luaL_traceback(lua_State *L, lua_State *L1, const char *msg, int level) {
    lua55L_traceback(L, L1, msg, level);
}

void luaL_requiref(lua_State *L, const char *modname, lua_CFunction openf, int glb) {
    lua55L_requiref(L, modname, openf, glb);
}

const char *luaL_findtable(lua_State *L, int idx, const char *fname, int szhint) {
    (void)szhint;
    int i55 = IS_PSEUDO51(idx) ? xidx(idx) : idx;
    if (lua55L_getsubtable(L, i55, fname))
        return NULL;  /* table already existed */
    return NULL;      /* created new table */
}

/* ================================================================
 *  Buffer API — implemented with lua51 struct layout
 * ================================================================ */

static void buf_flush(struct lua51_Buffer *B) {
    size_t len = (size_t)(B->p - B->buffer);
    if (len > 0) {
        lua55_pushlstring(B->L, B->buffer, len);
        B->lvl++;
        B->p = B->buffer;
    }
}

void luaL_buffinit(lua_State *L, void *B_raw) {
    struct lua51_Buffer *B = (struct lua51_Buffer *)B_raw;
    B->p   = B->buffer;
    B->lvl = 0;
    B->L   = L;
}

char *luaL_prepbuffer(void *B_raw) {
    struct lua51_Buffer *B = (struct lua51_Buffer *)B_raw;
    buf_flush(B);
    return B->buffer;
}

void luaL_addlstring(void *B_raw, const char *s, size_t l) {
    struct lua51_Buffer *B = (struct lua51_Buffer *)B_raw;
    while (l > 0) {
        size_t space = (size_t)(LUA51_BUFFERSIZE - (B->p - B->buffer));
        if (l <= space) {
            memcpy(B->p, s, l);
            B->p += l;
            return;
        }
        memcpy(B->p, s, space);
        B->p += space;
        s += space;
        l -= space;
        buf_flush(B);
    }
}

void luaL_addstring(void *B_raw, const char *s) {
    luaL_addlstring(B_raw, s, strlen(s));
}

void luaL_addvalue(void *B_raw) {
    struct lua51_Buffer *B = (struct lua51_Buffer *)B_raw;
    size_t vl;
    const char *s = lua55_tolstring(B->L, -1, &vl);
    buf_flush(B);
    /* the value is now on top, above accumulated strings */
    B->lvl++;
}

void luaL_pushresult(void *B_raw) {
    struct lua51_Buffer *B = (struct lua51_Buffer *)B_raw;
    buf_flush(B);
    if (B->lvl == 0) {
        lua55_pushlstring(B->L, "", 0);
    } else {
        lua55_concat(B->L, B->lvl);
    }
}

/* ================================================================
 *  Lua 5.1 compat: standard library functions removed in Lua 5.5
 * ================================================================ */

/* -- math compat -------------------------------------------------- */

static int compat51_math_atan2(lua_State *L) {
    lua55_pushnumber(L, atan2(lua55L_checknumber(L, 1),
                              lua55L_checknumber(L, 2)));
    return 1;
}

static int compat51_math_cosh(lua_State *L) {
    lua55_pushnumber(L, cosh(lua55L_checknumber(L, 1)));
    return 1;
}

static int compat51_math_sinh(lua_State *L) {
    lua55_pushnumber(L, sinh(lua55L_checknumber(L, 1)));
    return 1;
}

static int compat51_math_tanh(lua_State *L) {
    lua55_pushnumber(L, tanh(lua55L_checknumber(L, 1)));
    return 1;
}

static int compat51_math_pow(lua_State *L) {
    lua55_pushnumber(L, pow(lua55L_checknumber(L, 1),
                            lua55L_checknumber(L, 2)));
    return 1;
}

static int compat51_math_log10(lua_State *L) {
    lua55_pushnumber(L, log10(lua55L_checknumber(L, 1)));
    return 1;
}

static const luaL_Reg compat51_mathlib[] = {
    {"atan2", compat51_math_atan2},
    {"cosh",  compat51_math_cosh},
    {"sinh",  compat51_math_sinh},
    {"tanh",  compat51_math_tanh},
    {"pow",   compat51_math_pow},
    {"log10", compat51_math_log10},
    {NULL, NULL}
};

/* -- table compat ------------------------------------------------- */

static int compat51_table_foreach(lua_State *L) {
    lua55L_checktype(L, 1, LUA_TTABLE);
    lua55L_checktype(L, 2, LUA_TFUNCTION);
    lua55_pushnil(L);
    while (lua55_next(L, 1)) {
        lua55_pushvalue(L, 2);
        lua55_pushvalue(L, -3);
        lua55_pushvalue(L, -3);
        lua55_call(L, 2, 1);
        if (!lua55_isnil(L, -1))
            return 1;
        lua55_pop(L, 2);
    }
    return 0;
}

static int compat51_table_foreachi(lua_State *L) {
    lua_Integer i;
    lua_Integer n;
    lua55L_checktype(L, 1, LUA_TTABLE);
    lua55L_checktype(L, 2, LUA_TFUNCTION);
    n = (lua_Integer)lua55_rawlen(L, 1);
    for (i = 1; i <= n; i++) {
        lua55_pushvalue(L, 2);
        lua55_pushinteger(L, i);
        lua55_rawgeti(L, 1, i);
        lua55_call(L, 2, 1);
        if (!lua55_isnil(L, -1))
            return 1;
        lua55_pop(L, 1);
    }
    return 0;
}

static int compat51_table_getn(lua_State *L) {
    lua55L_checktype(L, 1, LUA_TTABLE);
    lua55_pushinteger(L, (lua_Integer)lua55_rawlen(L, 1));
    return 1;
}

static int compat51_table_maxn(lua_State *L) {
    lua_Number max = 0;
    lua55L_checktype(L, 1, LUA_TTABLE);
    lua55_pushnil(L);
    while (lua55_next(L, 1)) {
        lua55_pop(L, 1);
        if (lua55_type(L, -1) == LUA_TNUMBER) {
            lua_Number v = lua55_tonumberx(L, -1, NULL);
            if (v > max) max = v;
        }
    }
    lua55_pushnumber(L, max);
    return 1;
}

static int compat51_table_setn(lua_State *L) {
    lua55L_checktype(L, 1, LUA_TTABLE);
    return lua55L_error(L, "'setn' is obsolete");
}

static const luaL_Reg compat51_tablib[] = {
    {"foreach",  compat51_table_foreach},
    {"foreachi", compat51_table_foreachi},
    {"getn",     compat51_table_getn},
    {"maxn",     compat51_table_maxn},
    {"setn",     compat51_table_setn},
    {NULL, NULL}
};

void luaL_openlibs(lua_State *L) {
    lua55L_openlibs(L);
    /* Re-register compat51 additions (lua55L_openlibs calls lua55open_*
       directly, bypassing our luaopen_* wrappers) */
    /* base: add unpack, loadstring, gcinfo to _G */
    lua55_pushglobaltable(L);
    lua55_pushcclosure(L, compat51_unpack, 0);
    lua55_setfield(L, -2, "unpack");
    lua55_pushcclosure(L, compat51_loadstring, 0);
    lua55_setfield(L, -2, "loadstring");
    lua55_pushcclosure(L, compat51_gcinfo, 0);
    lua55_setfield(L, -2, "gcinfo");
    lua55_pushcclosure(L, compat51_tostring, 0);
    lua55_setfield(L, -2, "tostring");
    lua55_pop(L, 1);
    /* math */
    lua55_getglobal(L, "math");
    lua55L_setfuncs(L, compat51_mathlib, 0);
    lua55_pop(L, 1);
    /* table */
    lua55_getglobal(L, "table");
    lua55L_setfuncs(L, compat51_tablib, 0);
    lua55_pop(L, 1);
    /* string: gfind = gmatch */
    lua55_getglobal(L, "string");
    lua55_getfield(L, -1, "gmatch");
    lua55_setfield(L, -2, "gfind");
    lua55_pop(L, 1);
}

/* -- base compat -------------------------------------------------- */

static int compat51_unpack(lua_State *L) {
    lua_Integer i, e, n;
    lua55L_checktype(L, 1, LUA_TTABLE);
    i = lua55L_optinteger(L, 2, 1);
    if (lua55_isnoneornil(L, 3))
        e = (lua_Integer)lua55_rawlen(L, 1);
    else
        e = lua55L_checkinteger(L, 3);
    if (i > e) return 0;
    n = e - i + 1;
    if (n <= 0 || !lua55_checkstack(L, (int)n))
        return lua55L_error(L, "too many results to unpack");
    lua55_rawgeti(L, 1, i);
    while (i++ < e)
        lua55_rawgeti(L, 1, i);
    return (int)n;
}

static int compat51_loadstring(lua_State *L) {
    size_t l;
    const char *s = lua55L_checklstring(L, 1, &l);
    const char *chunkname = lua55L_optlstring(L, 2, s, NULL);
    int status = lua55L_loadbufferx(L, s, l, chunkname, NULL);
    if (status == LUA_OK) {
        return 1;
    } else {
        lua55_pushnil(L);
        lua55_rotate(L, -2, 1);
        return 2;
    }
}

static int compat51_gcinfo(lua_State *L) {
    lua55_pushinteger(L, lua55_gc(L, LUA_GCCOUNT));
    return 1;
}

static int compat51_tostring(lua_State *L) {
    lua55L_checkany(L, 1);
    if (lua55_type(L, 1) == LUA_TNUMBER) {
        /* Lua 5.1 format: %.14g — no trailing ".0" for integral floats */
        char buf[64];
        snprintf(buf, sizeof(buf), "%.14g", (double)lua55_tonumberx(L, 1, NULL));
        lua55_pushstring(L, buf);
    } else {
        lua55L_tolstring(L, 1, NULL);
    }
    return 1;
}

/* ================================================================
 *  Standard library open functions
 * ================================================================ */

int luaopen_base(lua_State *L) {
    int r = lua55open_base(L);
    lua55_pushcclosure(L, compat51_unpack, 0);
    lua55_setfield(L, -2, "unpack");
    lua55_pushcclosure(L, compat51_loadstring, 0);
    lua55_setfield(L, -2, "loadstring");
    lua55_pushcclosure(L, compat51_gcinfo, 0);
    lua55_setfield(L, -2, "gcinfo");
    lua55_pushcclosure(L, compat51_tostring, 0);
    lua55_setfield(L, -2, "tostring");
    return r;
}

int luaopen_package(lua_State *L)   { return lua55open_package(L); }
int luaopen_coroutine(lua_State *L) { return lua55open_coroutine(L); }

int luaopen_table(lua_State *L) {
    int r = lua55open_table(L);
    lua55L_setfuncs(L, compat51_tablib, 0);
    return r;
}

int luaopen_io(lua_State *L)        { return lua55open_io(L); }
int luaopen_os(lua_State *L)        { return lua55open_os(L); }

int luaopen_string(lua_State *L) {
    int r = lua55open_string(L);
    lua55_getfield(L, -1, "gmatch");
    lua55_setfield(L, -2, "gfind");
    return r;
}

int luaopen_math(lua_State *L) {
    int r = lua55open_math(L);
    lua55L_setfuncs(L, compat51_mathlib, 0);
    return r;
}

int luaopen_utf8(lua_State *L)      { return lua55open_utf8(L); }
int luaopen_debug(lua_State *L)     { return lua55open_debug(L); }
