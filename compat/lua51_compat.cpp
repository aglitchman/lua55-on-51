#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cstdarg>

// Include Luau headers directly (NOT compat headers)
#include "../luau/VM/include/lua.h"
#include "../luau/VM/include/lualib.h"
#include "../luau/Compiler/include/luacode.h"

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
    lua_error(L);
    return 0; // unreachable
}

// ============== luaL_error ==============
extern "C" int lua51_compat_lerror(lua_State* L, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    lua_pushvfstring(L, fmt, args);
    va_end(args);
    lua_error(L);
    return 0; // unreachable
}

// ============== luaL_argerror ==============
extern "C" int lua51_compat_argerror(lua_State* L, int numarg, const char* extramsg) {
    luaL_argerrorL(L, numarg, extramsg);
    return 0; // unreachable
}

// ============== lua_getstack ==============
extern "C" int lua51_compat_getstack(lua_State* L, int level, lua_Debug* ar) {
    lua_Debug tmp;
    memset(&tmp, 0, sizeof(tmp));
    int result = lua_getinfo(L, level, "l", &tmp);
    if (result == 0)
        return 0;
    memset(ar, 0, sizeof(lua_Debug));
    // Store level in userdata pointer
    ar->userdata = (void*)(intptr_t)level;
    return 1;
}

// ============== lua_getinfo ==============
extern "C" int lua51_compat_getinfo(lua_State* L, const char* what, lua_Debug* ar) {
    int level = (int)(intptr_t)ar->userdata;
    lua_Debug tmp;
    memset(&tmp, 0, sizeof(tmp));
    int result = lua_getinfo(L, level, what, &tmp);
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

// ============== lua_getlocal / lua_setlocal ==============
extern "C" const char* lua51_compat_getlocal(lua_State* L, const lua_Debug* ar, int n) {
    int level = (int)(intptr_t)ar->userdata;
    return lua_getlocal(L, level, n);
}

extern "C" const char* lua51_compat_setlocal(lua_State* L, const lua_Debug* ar, int n) {
    int level = (int)(intptr_t)ar->userdata;
    return lua_setlocal(L, level, n);
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

// ============== lua_equal / lua_lessthan ==============
extern "C" int lua51_compat_equal(lua_State* L, int idx1, int idx2) {
    return lua_rawequal(L, idx1, idx2);
}

extern "C" int lua51_compat_lessthan(lua_State* L, int idx1, int idx2) {
    (void)L; (void)idx1; (void)idx2;
    return 0; // stub
}

// ============== lua_cpcall ==============
struct CpcallData {
    lua_CFunction func;
    void* ud;
};

static int cpcall_wrapper(lua_State* L) {
    CpcallData* data = (CpcallData*)lua_touserdata(L, 1);
    lua_pushlightuserdata(L, data->ud);
    lua_replace(L, 1);
    return data->func(L);
}

extern "C" int lua51_compat_cpcall(lua_State* L, lua_CFunction func, void* ud) {
    CpcallData data = {func, ud};
    lua_pushlightuserdata(L, &data);
    // Use lua_pushcclosurek directly (Luau's real API)
    lua_pushcclosurek(L, cpcall_wrapper, NULL, 0, NULL);
    lua_insert(L, -2);
    return lua_pcall(L, 1, 0, 0);
}

// ============== lua_getfenv / lua_setfenv stubs ==============
extern "C" void lua51_compat_getfenv(lua_State* L, int idx) {
    (void)idx;
    lua_pushnil(L); // Luau doesn't have function environments
}

extern "C" int lua51_compat_setfenv(lua_State* L, int idx) {
    (void)idx;
    lua_pop(L, 1); // pop the env table
    return 0;
}

// ============== lua_newstate stub ==============
extern "C" lua_State* lua51_compat_newstate(lua_Alloc f, void* ud) {
    (void)f; (void)ud;
    return luaL_newstate(); // Luau ignores custom allocator
}

// ============== lua_resume / lua_yield stubs ==============
extern "C" int lua51_compat_resume(lua_State* L, int narg) {
    return lua_resume(L, NULL, narg);
}

extern "C" int lua51_compat_yield(lua_State* L, int nresults) {
    return lua_yield(L, nresults);
}

// ============== lua_load / lua_dump stubs ==============
extern "C" int lua51_compat_load(lua_State* L, const char* (*reader)(lua_State*, void*, size_t*), void* data, const char* chunkname) {
    (void)reader; (void)data; (void)chunkname;
    lua_pushstring(L, "lua_load not supported in Luau compat layer");
    return LUA_ERRSYNTAX;
}

extern "C" int lua51_compat_dump(lua_State* L, int (*writer)(lua_State*, const void*, size_t, void*), void* data) {
    (void)L; (void)writer; (void)data;
    return 1; // error
}

// ============== Other stubs ==============
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

extern "C" char* lua51_compat_prepbuffer(luaL_Strbuf* B) {
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
