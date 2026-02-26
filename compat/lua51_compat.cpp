#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cstdarg>

// Include Luau headers directly (before compat macros)
#include "lua.h"
#include "lualib.h"
#include "luacode.h"

// Save original Luau functions before including compat header
// lua_error (noreturn in Luau)
static void call_luau_error(lua_State* L) {
    lua_error(L);
}

// lua_getinfo (4-arg in Luau)
static int call_luau_getinfo(lua_State* L, int level, const char* what, lua_Debug* ar) {
    return lua_getinfo(L, level, what, ar);
}

// luaL_errorL
static void call_luau_lerror(lua_State* L, const char* fmt, ...) {
    // We need to format the message and push it, then call lua_error
    va_list args;
    va_start(args, fmt);
    lua_pushvfstring(L, fmt, args);
    va_end(args);
    lua_error(L);
}

// Now include the compat header (which redefines macros)
#include "lua51_compat.h"

// ============== lua_atpanic ==============
static lua_CFunction s_panic_func = NULL;

static void luau_panic_bridge(lua_State* L, int errcode) {
    (void)errcode;
    if (s_panic_func) {
        s_panic_func(L);
    }
}

extern "C" lua_CFunction lua51_compat_atpanic(lua_State* L, lua_CFunction panicf) {
    lua_CFunction old = s_panic_func;
    s_panic_func = panicf;
    lua_callbacks(L)->panic = luau_panic_bridge;
    return old;
}

// ============== lua_error ==============
extern "C" int lua51_compat_error(lua_State* L) {
    call_luau_error(L);
    return 0; // unreachable
}

// ============== luaL_error ==============
extern "C" int lua51_compat_lerror(lua_State* L, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    lua_pushvfstring(L, fmt, args);
    va_end(args);
    call_luau_error(L);
    return 0; // unreachable
}

// ============== lua_getstack ==============
// Store level in lua_Debug.userdata for later use by lua_getinfo
extern "C" int lua51_compat_getstack(lua_State* L, int level, lua_Debug* ar) {
    // Verify level is valid by trying to get info
    lua_Debug tmp;
    memset(&tmp, 0, sizeof(tmp));
    int result = call_luau_getinfo(L, level, "l", &tmp);
    if (result == 0)
        return 0;
    memset(ar, 0, sizeof(lua_Debug));
    // Store level in userdata pointer (cast int to pointer)
    ar->userdata = (void*)(intptr_t)level;
    return 1;
}

// ============== lua_getinfo ==============
extern "C" int lua51_compat_getinfo(lua_State* L, const char* what, lua_Debug* ar) {
    int level = (int)(intptr_t)ar->userdata;
    lua_Debug tmp;
    memset(&tmp, 0, sizeof(tmp));
    int result = call_luau_getinfo(L, level, what, &tmp);
    if (result == 0)
        return 0;
    ar->name = tmp.name;
    ar->what = tmp.what;
    ar->source = tmp.source;
    ar->short_src = tmp.short_src;
    ar->currentline = tmp.currentline;
    ar->linedefined = tmp.linedefined;
    ar->nupvals = tmp.nupvals;
    ar->nparams = tmp.nparams;
    ar->isvararg = tmp.isvararg;
    return 1;
}

// ============== lua_sethook ==============
static lua_Hook s_hook_func = NULL;
static int s_hook_mask = 0;
static int s_hook_count = 0;

static void luau_hook_bridge(lua_State* L, lua_Debug* ar) {
    if (s_hook_func) {
        s_hook_func(L, ar);
    }
}

extern "C" int lua51_compat_sethook(lua_State* L, lua_Hook func, int mask, int count) {
    s_hook_func = func;
    s_hook_mask = mask;
    s_hook_count = count;
    lua_Callbacks* cb = lua_callbacks(L);
    if (func && mask) {
        cb->debugstep = luau_hook_bridge;
        lua_singlestep(L, 1);
    } else {
        cb->debugstep = NULL;
        lua_singlestep(L, 0);
    }
    return 1;
}

extern "C" lua_Hook lua51_compat_gethook(lua_State* L) {
    (void)L;
    return s_hook_func;
}

extern "C" int lua51_compat_gethookmask(lua_State* L) {
    (void)L;
    return s_hook_mask;
}

extern "C" int lua51_compat_gethookcount(lua_State* L) {
    (void)L;
    return s_hook_count;
}

// ============== luaL_loadbuffer / luaL_loadstring ==============
extern "C" int lua51_compat_loadbuffer(lua_State* L, const char* buff, size_t sz, const char* name) {
    size_t bytecodeSize = 0;
    char* bytecode = luau_compile(buff, sz, NULL, &bytecodeSize);
    if (!bytecode) {
        lua_pushstring(L, "compilation failed");
        return LUA_ERRSYNTAX;
    }
    int result = luau_load(L, name ? name : "=(load)", bytecode, bytecodeSize, 0);
    free(bytecode);
    if (result != 0) {
        // luau_load pushes an error message on failure
        return LUA_ERRSYNTAX;
    }
    return 0;
}

extern "C" int lua51_compat_loadstring(lua_State* L, const char* s) {
    return lua51_compat_loadbuffer(L, s, strlen(s), s);
}

// ============== luaL_ref / luaL_unref ==============
extern "C" int lua51_compat_ref(lua_State* L, int t) {
    (void)t;
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return LUA_REFNIL;
    }
    int ref = lua_ref(L, -1);
    lua_pop(L, 1);
    return ref;
}

extern "C" void lua51_compat_unref(lua_State* L, int t, int ref) {
    (void)t;
    if (ref >= 0) {
        lua_unref(L, ref);
    }
}

// ============== luaL_typerror ==============
extern "C" int lua51_compat_typerror(lua_State* L, int narg, const char* tname) {
    luaL_typeerrorL(L, narg, tname);
    return 0; // unreachable
}

// ============== Stubs ==============

extern "C" void lua51_compat_setallocf(lua_State* L, lua_Alloc f, void* ud) {
    (void)L; (void)f; (void)ud;
}

extern "C" void lua51_compat_setlevel(lua_State* from, lua_State* to) {
    (void)from; (void)to;
}

extern "C" int lua51_compat_loadfile(lua_State* L, const char* filename) {
    (void)filename;
    lua_pushstring(L, "luaL_loadfile not supported in Luau compat layer");
    return LUA_ERRSYNTAX;
}

extern "C" void lua51_compat_openlib(lua_State* L, const char* libname, const luaL_Reg* l, int nup) {
    (void)nup;
    if (libname) {
        luaL_register(L, libname, l);
    }
}

extern "C" const char* lua51_compat_gsub(lua_State* L, const char* s, const char* p, const char* r) {
    (void)L; (void)s; (void)p; (void)r;
    return NULL;
}

extern "C" char* lua51_compat_prepbuffer(luaL_Buffer* B) {
    return luaL_prepbuffsize(B, LUA_BUFFERSIZE);
}

extern "C" int lua51_compat_openio(lua_State* L) {
    lua_newtable(L);
    return 1;
}

extern "C" int lua51_compat_openpackage(lua_State* L) {
    lua_newtable(L);
    return 1;
}
