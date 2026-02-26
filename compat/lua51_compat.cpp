/*
** Lua 5.1 C API compatibility shim for Luau.
**
** Includes Lua 5.1 headers for correct types and C++ name mangling.
** Provides all functions that are missing in Luau or have different
** signatures, making compat+Luau a drop-in replacement for liblua.a.
*/

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cstdarg>

// Lua 5.1 headers — type constants now match Luau after VM patch
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

// ================================================================
// Forward declarations of Luau functions with unique names.
// These exist in Luau's .a and don't conflict with Lua 5.1.
// ================================================================

// Luau's extended versions (lua_tonumber/tointeger are macros in Luau)
double lua_tonumberx(lua_State* L, int idx, int* isnum);
int lua_tointegerx(lua_State* L, int idx, int* isnum);

// Luau's tagged variants (lua_pushlightuserdata/newuserdata are macros in Luau)
void lua_pushlightuserdatatagged(lua_State* L, void* p, int tag);
void* lua_newuserdatatagged(lua_State* L, size_t sz, int tag);

// Luau's pushfstringL (lua_pushfstring is a macro in Luau)
const char* lua_pushfstringL(lua_State* L, const char* fmt, ...);

// Luau's pushcclosurek (lua_pushcclosure/pushcfunction are macros in Luau)
typedef int (*luau_Continuation)(lua_State*, int);
void lua_pushcclosurek(lua_State* L, lua_CFunction fn, const char* debugname, int nup, luau_Continuation cont);

// Luau's error functions (luaL_error/argerror are macros in Luau)
void luaL_typeerrorL(lua_State* L, int narg, const char* tname);
void luaL_argerrorL(lua_State* L, int narg, const char* extramsg);

// Luau's register (same sig, but we need to call it from luaI_openlib)
// It's already declared in lauxlib.h as luaL_register — we can call it directly.

// ================================================================
// Bridge function declarations (extern "C" from luau_bridge.cpp).
// For functions where Luau has the SAME name but DIFFERENT C++ mangling.
// ================================================================

extern "C" {
int         luau_bridge_getinfo(lua_State* L, int level, const char* what, lua_Debug* ar51);
int         luau_bridge_resume(lua_State* L, lua_State* from, int narg);
const char* luau_bridge_getlocal(lua_State* L, int level, int n);
const char* luau_bridge_setlocal(lua_State* L, int level, int n);
void        luau_bridge_pushinteger(lua_State* L, int n);
int         luau_bridge_optinteger(lua_State* L, int nArg, int def);
void        luau_bridge_set_panic(lua_State* L, void (*func)(lua_State*, int));
int         luau_bridge_ref(lua_State* L, int idx);
void        luau_bridge_unref(lua_State* L, int ref);
char*       luau_bridge_compile(const char* source, size_t sourceLength, size_t* outSize);
int         luau_bridge_load(lua_State* L, const char* chunkname, const char* data, size_t size, int env);
void        luau_bridge_sethook(lua_State* L, void* hook, int mask, int count);
void*       luau_bridge_gethook(void);
int         luau_bridge_gethookmask(void);
int         luau_bridge_gethookcount(void);
}

// ================================================================
// Implementations
// ================================================================

// ============== lua_tonumber (macro in Luau, function in Lua 5.1) ==============
lua_Number lua_tonumber(lua_State* L, int idx) {
    return lua_tonumberx(L, idx, NULL);
}

// ============== lua_tointeger (macro in Luau, function in Lua 5.1) ==============
lua_Integer lua_tointeger(lua_State* L, int idx) {
    return (lua_Integer)lua_tointegerx(L, idx, NULL);
}

// ============== lua_pushinteger (ptrdiff_t in 5.1, int in Luau) ==============
void lua_pushinteger(lua_State* L, lua_Integer n) {
    luau_bridge_pushinteger(L, (int)n);
}

// ============== lua_pushlightuserdata (macro in Luau) ==============
void lua_pushlightuserdata(lua_State* L, void* p) {
    lua_pushlightuserdatatagged(L, p, 0);
}

// ============== lua_newuserdata (macro in Luau) ==============
void* lua_newuserdata(lua_State* L, size_t sz) {
    return lua_newuserdatatagged(L, sz, 0);
}

// ============== lua_pushfstring (macro in Luau) ==============
const char* lua_pushfstring(lua_State* L, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    const char* result = lua_pushvfstring(L, fmt, args);
    va_end(args);
    return result;
}

// ============== lua_pushcclosure (3 params in 5.1, macro in Luau) ==============
void lua_pushcclosure(lua_State* L, lua_CFunction fn, int n) {
    lua_pushcclosurek(L, fn, NULL, n, NULL);
}

// ============== lua_atpanic ==============
static lua_CFunction s_panic_func = NULL;

static void panic_bridge(lua_State* L, int errcode) {
    (void)errcode;
    if (s_panic_func)
        s_panic_func(L);
}

lua_CFunction lua_atpanic(lua_State* L, lua_CFunction panicf) {
    lua_CFunction old = s_panic_func;
    s_panic_func = panicf;
    luau_bridge_set_panic(L, panic_bridge);
    return old;
}

// ============== lua_load (stub) ==============
int lua_load(lua_State* L, lua_Reader reader, void* dt, const char* chunkname) {
    (void)reader; (void)dt; (void)chunkname;
    lua_pushstring(L, "lua_load not supported in Luau compat layer");
    return LUA_ERRSYNTAX;
}

// ============== lua_dump (stub) ==============
int lua_dump(lua_State* L, lua_Writer writer, void* data) {
    (void)L; (void)writer; (void)data;
    return 1;
}

// ============== lua_resume (2 params in 5.1, 3 in Luau) ==============
int lua_resume(lua_State* L, int narg) {
    return luau_bridge_resume(L, NULL, narg);
}

// ============== lua_setallocf (stub) ==============
void lua_setallocf(lua_State* L, lua_Alloc f, void* ud) {
    (void)L; (void)f; (void)ud;
}

// ============== lua_setlevel (stub) ==============
void lua_setlevel(lua_State* from, lua_State* to) {
    (void)from; (void)to;
}

// ============== lua_getstack ==============
int lua_getstack(lua_State* L, int level, lua_Debug* ar) {
    lua_Debug tmp;
    memset(&tmp, 0, sizeof(tmp));
    int result = luau_bridge_getinfo(L, level, "l", &tmp);
    if (!result)
        return 0;
    memset(ar, 0, sizeof(*ar));
    ar->i_ci = level;
    return 1;
}

// ============== lua_getinfo (3 params in 5.1, 4 in Luau) ==============
int lua_getinfo(lua_State* L, const char* what, lua_Debug* ar) {
    int level = ar->i_ci;
    return luau_bridge_getinfo(L, level, what, ar);
}

// ============== lua_getlocal / lua_setlocal ==============
const char* lua_getlocal(lua_State* L, const lua_Debug* ar, int n) {
    int level = ar->i_ci;
    return luau_bridge_getlocal(L, level, n);
}

const char* lua_setlocal(lua_State* L, const lua_Debug* ar, int n) {
    int level = ar->i_ci;
    return luau_bridge_setlocal(L, level, n);
}

// ============== lua_sethook / gethook / gethookmask / gethookcount ==============
int lua_sethook(lua_State* L, lua_Hook func, int mask, int count) {
    luau_bridge_sethook(L, (void*)func, mask, count);
    return 1;
}

lua_Hook lua_gethook(lua_State* L) {
    (void)L;
    return (lua_Hook)luau_bridge_gethook();
}

int lua_gethookmask(lua_State* L) {
    (void)L;
    return luau_bridge_gethookmask();
}

int lua_gethookcount(lua_State* L) {
    (void)L;
    return luau_bridge_gethookcount();
}

// ============== luaL_error (returns int, Luau's is noreturn) ==============
int luaL_error(lua_State* L, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    lua_pushvfstring(L, fmt, args);
    va_end(args);
    lua_error(L);
    return 0; // unreachable
}

// ============== luaL_argerror (returns int) ==============
int luaL_argerror(lua_State* L, int numarg, const char* extramsg) {
    luaL_argerrorL(L, numarg, extramsg);
    return 0; // unreachable
}

// ============== luaL_typerror ==============
int luaL_typerror(lua_State* L, int narg, const char* tname) {
    luaL_typeerrorL(L, narg, tname);
    return 0; // unreachable
}

// ============== luaL_optinteger (3rd param is ptrdiff_t in 5.1, int in Luau) ==============
lua_Integer luaL_optinteger(lua_State* L, int nArg, lua_Integer def) {
    return (lua_Integer)luau_bridge_optinteger(L, nArg, (int)def);
}

// ============== luaL_loadbuffer / luaL_loadstring ==============
int luaL_loadbuffer(lua_State* L, const char* buff, size_t sz, const char* name) {
    size_t bytecodeSize = 0;
    char* bytecode = luau_bridge_compile(buff, sz, &bytecodeSize);
    if (!bytecode) {
        lua_pushstring(L, "compilation failed");
        return LUA_ERRSYNTAX;
    }
    int result = luau_bridge_load(L, name ? name : "=(load)", bytecode, bytecodeSize, 0);
    free(bytecode);
    return result != 0 ? LUA_ERRSYNTAX : 0;
}

int luaL_loadstring(lua_State* L, const char* s) {
    return luaL_loadbuffer(L, s, strlen(s), s);
}

// ============== luaL_loadfile ==============
int luaL_loadfile(lua_State* L, const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        lua_pushfstring(L, "cannot open %s", filename);
        return LUA_ERRFILE;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc(size);
    if (!buf) {
        fclose(f);
        lua_pushstring(L, "out of memory");
        return LUA_ERRMEM;
    }
    size_t nread = fread(buf, 1, size, f);
    fclose(f);
    char chunkname[1024];
    snprintf(chunkname, sizeof(chunkname), "@%s", filename);
    int status = luaL_loadbuffer(L, buf, nread, chunkname);
    free(buf);
    return status;
}

// ============== luaL_ref / luaL_unref ==============
#undef lua_ref
#undef lua_unref

int luaL_ref(lua_State* L, int t) {
    (void)t;
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return LUA_REFNIL;
    }
    int ref = luau_bridge_ref(L, -1);
    lua_pop(L, 1);
    return ref;
}

void luaL_unref(lua_State* L, int t, int ref) {
    (void)t;
    if (ref >= 0)
        luau_bridge_unref(L, ref);
}

// ============== luaI_openlib ==============
void luaI_openlib(lua_State* L, const char* libname, const luaL_Reg* l, int nup) {
    (void)nup;
    if (libname)
        luaL_register(L, libname, l);
}

// ============== luaL_gsub (stub) ==============
const char* luaL_gsub(lua_State* L, const char* s, const char* p, const char* r) {
    (void)L; (void)s; (void)p; (void)r;
    return NULL;
}

// ============== luaopen_io (minimal file reading) ==============

static int io_file_read(lua_State* L) {
    FILE** fp = (FILE**)lua_touserdata(L, 1);
    if (!fp || !*fp) return luaL_error(L, "attempt to use a closed file");
    const char* mode = luaL_optstring(L, 2, "*l");
    if (strcmp(mode, "*l") == 0) {
        char buf[4096];
        if (fgets(buf, sizeof(buf), *fp)) {
            size_t len = strlen(buf);
            if (len > 0 && buf[len - 1] == '\n') len--;
            if (len > 0 && buf[len - 1] == '\r') len--;
            lua_pushlstring(L, buf, len);
            return 1;
        }
        lua_pushnil(L);
        return 1;
    } else if (strcmp(mode, "*a") == 0) {
        luaL_Buffer b;
        luaL_buffinit(L, &b);
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), *fp)) > 0)
            luaL_addlstring(&b, buf, n);
        luaL_pushresult(&b);
        return 1;
    } else if (strcmp(mode, "*n") == 0) {
        double d;
        if (fscanf(*fp, "%lf", &d) == 1) {
            lua_pushnumber(L, d);
            return 1;
        }
        lua_pushnil(L);
        return 1;
    }
    return luaL_error(L, "unsupported read mode '%s'", mode);
}

static int io_file_close(lua_State* L) {
    FILE** fp = (FILE**)lua_touserdata(L, 1);
    if (fp && *fp) {
        fclose(*fp);
        *fp = NULL;
    }
    return 0;
}

static int io_file_gc(lua_State* L) {
    return io_file_close(L);
}

static int io_open(lua_State* L) {
    const char* filename = luaL_checkstring(L, 1);
    const char* mode = luaL_optstring(L, 2, "r");
    FILE* f = fopen(filename, mode);
    if (!f) {
        lua_pushnil(L);
        lua_pushfstring(L, "cannot open %s", filename);
        return 2;
    }
    FILE** fp = (FILE**)lua_newuserdata(L, sizeof(FILE*));
    *fp = f;
    if (luaL_newmetatable(L, "io.file")) {
        lua_pushstring(L, "__index");
        lua_newtable(L);
        lua_pushcfunction(L, io_file_read);
        lua_setfield(L, -2, "read");
        lua_pushcfunction(L, io_file_close);
        lua_setfield(L, -2, "close");
        lua_settable(L, -3);
        lua_pushcfunction(L, io_file_gc);
        lua_setfield(L, -2, "__gc");
    }
    lua_setmetatable(L, -2);
    return 1;
}

static int io_lines_iter(lua_State* L) {
    FILE** fp = (FILE**)lua_touserdata(L, lua_upvalueindex(1));
    if (!fp || !*fp) return 0;
    char buf[4096];
    if (fgets(buf, sizeof(buf), *fp)) {
        size_t len = strlen(buf);
        if (len > 0 && buf[len - 1] == '\n') len--;
        if (len > 0 && buf[len - 1] == '\r') len--;
        lua_pushlstring(L, buf, len);
        return 1;
    }
    fclose(*fp);
    *fp = NULL;
    return 0;
}

static int io_lines(lua_State* L) {
    const char* filename = luaL_checkstring(L, 1);
    FILE* f = fopen(filename, "r");
    if (!f) return luaL_error(L, "cannot open %s", filename);
    FILE** fp = (FILE**)lua_newuserdata(L, sizeof(FILE*));
    *fp = f;
    lua_pushcclosure(L, io_lines_iter, 1);
    return 1;
}

int luaopen_io(lua_State* L) {
    lua_createtable(L, 0, 3);
    lua_pushcfunction(L, io_open);
    lua_setfield(L, -2, "open");
    lua_pushcfunction(L, io_lines);
    lua_setfield(L, -2, "lines");
    lua_setglobal(L, "io");
    return 0;
}

static int pkg_require(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);

    // Check package.loaded[name]
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "loaded");
    lua_getfield(L, -1, name);
    if (!lua_isnil(L, -1)) {
        return 1; // already loaded
    }
    lua_pop(L, 1); // pop nil

    // Check package.preload[name]
    lua_getfield(L, -2, "preload"); // package table is at -3
    lua_getfield(L, -1, name);
    if (lua_isfunction(L, -1)) {
        lua_pushstring(L, name);
        lua_call(L, 1, 1);
        // Store in package.loaded
        lua_pushvalue(L, -1);
        lua_setfield(L, -5, name); // loaded table is at -5
        return 1;
    }
    lua_pop(L, 2); // pop nil + preload table

    // Check if module exists as a global table (built-in modules like math, string, etc.)
    lua_getglobal(L, name);
    if (!lua_isnil(L, -1)) {
        lua_pushvalue(L, -1);
        lua_setfield(L, -4, name); // store in loaded table
        return 1;
    }
    lua_pop(L, 1);

    // Search package.path
    lua_getfield(L, -2, "path"); // package table is at -2
    const char* path = lua_tostring(L, -1);
    if (!path) {
        lua_pop(L, 3); // path, loaded, package
        return luaL_error(L, "package.path is not set");
    }

    // Convert module dots to path separators
    char modpath[512];
    size_t mi = 0;
    for (const char* p = name; *p && mi < sizeof(modpath) - 1; p++) {
        modpath[mi++] = (*p == '.') ? '/' : *p;
    }
    modpath[mi] = '\0';

    // Try each path template
    char tried[2048] = "";
    size_t tried_len = 0;
    const char* cur = path;
    while (*cur) {
        const char* semi = strchr(cur, ';');
        size_t tlen = semi ? (size_t)(semi - cur) : strlen(cur);

        char filepath[1024];
        size_t fi = 0;
        for (size_t i = 0; i < tlen && fi < sizeof(filepath) - 1; i++) {
            if (cur[i] == '?') {
                for (size_t j = 0; modpath[j] && fi < sizeof(filepath) - 1; j++)
                    filepath[fi++] = modpath[j];
            } else {
                filepath[fi++] = cur[i];
            }
        }
        filepath[fi] = '\0';

        if (luaL_loadfile(L, filepath) == 0) {
            lua_call(L, 0, 1);
            if (lua_isnil(L, -1)) {
                lua_pop(L, 1);
                lua_pushboolean(L, 1);
            }
            // Store in package.loaded
            lua_pushvalue(L, -1);
            lua_setfield(L, -5, name); // loaded table
            return 1;
        }
        lua_pop(L, 1); // pop error message

        // Append to tried list
        tried_len += snprintf(tried + tried_len, sizeof(tried) - tried_len,
                              "\n\tno file '%s'", filepath);

        cur = semi ? semi + 1 : cur + tlen;
    }

    lua_pop(L, 3); // path, loaded, package
    return luaL_error(L, "module '%s' not found:%s", name, tried);
}

int luaopen_package(lua_State* L) {
    // Create package table
    lua_createtable(L, 0, 4);

    // package.loaded
    lua_createtable(L, 0, 0);
    lua_setfield(L, -2, "loaded");

    // package.preload
    lua_createtable(L, 0, 0);
    lua_setfield(L, -2, "preload");

    // package.path (default)
    lua_pushstring(L, "./?.lua;./?/init.lua");
    lua_setfield(L, -2, "path");

    // Set as global "package"
    lua_setglobal(L, "package");

    // Register "require" global
    lua_pushcfunction(L, pkg_require);
    lua_setglobal(L, "require");

    return 0;
}

// ================================================================
// Buffer API — standalone implementation matching Lua 5.1 layout.
// ================================================================

static int emptybuffer(luaL_Buffer* B) {
    size_t l = (size_t)(B->p - B->buffer);
    if (l == 0)
        return 0;
    lua_pushlstring(B->L, B->buffer, l);
    B->p = B->buffer;
    B->lvl++;
    return 1;
}

static void adjuststack(luaL_Buffer* B) {
    if (B->lvl > 1) {
        lua_State* L = B->L;
        int toget = 1;
        size_t toplen = lua_objlen(L, -1);
        do {
            size_t l = lua_objlen(L, -(toget + 1));
            if (l > toplen) break;
            toplen += l;
            toget++;
        } while (toget < B->lvl);
        lua_concat(L, toget);
        B->lvl = B->lvl - toget + 1;
    }
}

void luaL_buffinit(lua_State* L, luaL_Buffer* B) {
    B->L = L;
    B->p = B->buffer;
    B->lvl = 0;
}

char* luaL_prepbuffer(luaL_Buffer* B) {
    if (emptybuffer(B))
        adjuststack(B);
    return B->buffer;
}

void luaL_addlstring(luaL_Buffer* B, const char* s, size_t l) {
    while (l > 0) {
        size_t space = (size_t)(B->buffer + LUAL_BUFFERSIZE - B->p);
        if (l <= space) {
            memcpy(B->p, s, l);
            B->p += l;
            return;
        }
        memcpy(B->p, s, space);
        B->p += space;
        s += space;
        l -= space;
        emptybuffer(B);
        adjuststack(B);
    }
}

void luaL_addstring(luaL_Buffer* B, const char* s) {
    luaL_addlstring(B, s, strlen(s));
}

void luaL_addvalue(luaL_Buffer* B) {
    lua_State* L = B->L;
    size_t vl;
    const char* s = lua_tolstring(L, -1, &vl);
    luaL_addlstring(B, s, vl);
    lua_pop(L, 1);
}

void luaL_pushresult(luaL_Buffer* B) {
    emptybuffer(B);
    if (B->lvl == 0) {
        lua_pushlstring(B->L, "", 0);
    } else if (B->lvl > 1) {
        lua_concat(B->L, B->lvl);
    }
}
