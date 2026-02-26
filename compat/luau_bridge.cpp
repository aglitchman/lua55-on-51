/*
** Bridge functions for Luau APIs that have the same name but different
** C++ mangling from Lua 5.1. These wrappers have extern "C" linkage
** and unique names so the compat shim can call them without ambiguity.
*/

#include <cstring>

#include "../luau/VM/include/lua.h"
#include "../luau/VM/include/lualib.h"
#include "../luau/Compiler/include/luacode.h"

// Lua 5.1 lua_Debug layout (from lua51/src/lua.h, LUA_IDSIZE=60)
#define LUA51_IDSIZE 60

struct lua_Debug_51 {
    int event;
    const char* name;
    const char* namewhat;
    const char* what;
    const char* source;
    int currentline;
    int nups;
    int linedefined;
    int lastlinedefined;
    char short_src[LUA51_IDSIZE];
    int i_ci;
};

extern "C" {

// ============== lua_getinfo bridge ==============
int luau_bridge_getinfo(lua_State* L, int level, const char* what, lua_Debug_51* ar51) {
    lua_Debug ar;
    memset(&ar, 0, sizeof(ar));
    int result = lua_getinfo(L, level, what, &ar);
    if (!result)
        return 0;
    if (ar51) {
        ar51->name = ar.name;
        ar51->namewhat = NULL;
        ar51->what = ar.what;
        ar51->source = ar.source;
        ar51->currentline = ar.currentline;
        ar51->nups = (int)ar.nupvals;
        ar51->linedefined = ar.linedefined;
        ar51->lastlinedefined = 0;
        const char* src = ar.short_src ? ar.short_src : ar.ssbuf;
        if (src) {
            strncpy(ar51->short_src, src, LUA51_IDSIZE);
            ar51->short_src[LUA51_IDSIZE - 1] = '\0';
        }
    }
    return 1;
}

// ============== lua_resume bridge ==============
int luau_bridge_resume(lua_State* L, lua_State* from, int narg) {
    return lua_resume(L, from, narg);
}

// ============== lua_getlocal / lua_setlocal bridges ==============
const char* luau_bridge_getlocal(lua_State* L, int level, int n) {
    return lua_getlocal(L, level, n);
}

const char* luau_bridge_setlocal(lua_State* L, int level, int n) {
    return lua_setlocal(L, level, n);
}

// ============== lua_pushinteger bridge (Luau takes int, Lua 5.1 takes ptrdiff_t) ==============
void luau_bridge_pushinteger(lua_State* L, int n) {
    lua_pushinteger(L, n);
}

// ============== luaL_optinteger bridge (Luau takes int def, Lua 5.1 takes ptrdiff_t def) ==============
int luau_bridge_optinteger(lua_State* L, int nArg, int def) {
    return luaL_optinteger(L, nArg, def);
}

// ============== lua_callbacks bridges ==============
void luau_bridge_set_panic(lua_State* L, void (*func)(lua_State*, int)) {
    lua_callbacks(L)->panic = func;
}

void luau_bridge_set_debugstep(lua_State* L, void (*func)(lua_State*, lua_Debug*)) {
    lua_callbacks(L)->debugstep = func;
}

void luau_bridge_singlestep(lua_State* L, int enabled) {
    lua_singlestep(L, enabled);
}

// ============== lua_ref / lua_unref bridges ==============
int luau_bridge_ref(lua_State* L, int idx) {
    return lua_ref(L, idx);
}

void luau_bridge_unref(lua_State* L, int ref) {
    lua_unref(L, ref);
}

// ============== luau_compile / luau_load bridges ==============
char* luau_bridge_compile(const char* source, size_t sourceLength, size_t* outSize) {
    lua_CompileOptions opts = {};
    opts.optimizationLevel = 2;
    opts.debugLevel = 1;
    return luau_compile(source, sourceLength, &opts, outSize);
}

int luau_bridge_load(lua_State* L, const char* chunkname, const char* data, size_t size, int env) {
    return luau_load(L, chunkname, data, size, env);
}

// ============== Hook trampoline ==============
typedef void (*lua51_hook_t)(lua_State* L, lua_Debug_51* ar);
static lua51_hook_t s_user_hook = NULL;
static int s_hook_mask = 0;
static int s_hook_count = 0;

static void hook_trampoline(lua_State* L, lua_Debug* luau_ar) {
    if (!s_user_hook)
        return;
    lua_Debug_51 ar51;
    memset(&ar51, 0, sizeof(ar51));
    ar51.event = 2; // LUA_HOOKLINE
    ar51.currentline = luau_ar->currentline;
    ar51.name = luau_ar->name;
    ar51.what = luau_ar->what;
    ar51.source = luau_ar->source;
    const char* src = luau_ar->short_src ? luau_ar->short_src : luau_ar->ssbuf;
    if (src) {
        strncpy(ar51.short_src, src, LUA51_IDSIZE);
        ar51.short_src[LUA51_IDSIZE - 1] = '\0';
    }
    s_user_hook(L, &ar51);
}

void luau_bridge_sethook(lua_State* L, void* hook, int mask, int count) {
    s_user_hook = (lua51_hook_t)hook;
    s_hook_mask = mask;
    s_hook_count = count;
    if (hook && mask) {
        lua_callbacks(L)->debugstep = hook_trampoline;
        lua_singlestep(L, 1);
    } else {
        lua_callbacks(L)->debugstep = NULL;
        lua_singlestep(L, 0);
    }
}

void* luau_bridge_gethook(void) {
    return (void*)s_user_hook;
}

int luau_bridge_gethookmask(void) {
    return s_hook_mask;
}

int luau_bridge_gethookcount(void) {
    return s_hook_count;
}

} // extern "C"
