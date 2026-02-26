#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#define TEST(name) static int test_##name(lua_State *L)
#define RUN(name) do { \
    printf("  %-40s", #name); \
    if (test_##name(L) == 0) printf("OK\n"); \
    else { printf("FAIL\n"); fails++; } \
} while(0)

static int l_loadfile(lua_State *L) {
    const char *filename = luaL_checkstring(L, 1);
    if (luaL_loadfile(L, filename) != 0) {
        lua_pushnil(L);
        lua_insert(L, -2);
        return 2;
    }
    return 1;
}

static int l_loadstring(lua_State *L) {
    const char *s = luaL_checkstring(L, 1);
    if (luaL_loadstring(L, s) != 0) {
        lua_pushnil(L);
        lua_insert(L, -2);
        return 2;
    }
    return 1;
}

static int my_panic(lua_State *L) {
    const char *msg = lua_tostring(L, -1);
    fprintf(stderr, "PANIC: %s\n", msg ? msg : "(null)");
    return 0;
}

static int my_cfunction(lua_State *L) {
    lua_pushnumber(L, 42);
    return 1;
}

static int my_cfunction2(lua_State *L) {
    lua_pushstring(L, "hello from C");
    return 1;
}

static int my_error_func(lua_State *L) {
    return luaL_error(L, "intentional error");
}

static int my_cpcall_func(lua_State *L) {
    int *data = (int *)lua_touserdata(L, 1);
    *data = 12345;
    return 0;
}

static void my_hook(lua_State *L, lua_Debug *ar) {
    (void)L; (void)ar;
}

/* ===== State manipulation ===== */

TEST(atpanic) {
    lua_CFunction old = lua_atpanic(L, my_panic);
    lua_atpanic(L, old ? old : my_panic);
    return 0;
}

TEST(newthread) {
    lua_State *L1 = lua_newthread(L);
    if (!L1) return 1;
    lua_pop(L, 1);
    return 0;
}

/* ===== Stack manipulation ===== */

TEST(stack_ops) {
    int top = lua_gettop(L);
    lua_pushnumber(L, 1);
    lua_pushnumber(L, 2);
    lua_pushnumber(L, 3);
    if (lua_gettop(L) != top + 3) return 1;

    lua_pushvalue(L, -1);
    if (lua_tonumber(L, -1) != 3) return 1;
    lua_pop(L, 1);

    lua_remove(L, -2);
    if (lua_tonumber(L, -1) != 3) return 1;
    if (lua_tonumber(L, -2) != 1) return 1;

    lua_pushnil(L);
    lua_insert(L, -2);

    lua_pushnumber(L, 99);
    lua_replace(L, -2);

    if (!lua_checkstack(L, 100)) return 1;

    lua_settop(L, top);
    return 0;
}

/* ===== Access functions ===== */

TEST(type_checks) {
    lua_pushnil(L);
    if (lua_type(L, -1) != LUA_TNIL) return 1;
    if (!lua_isnil(L, -1)) return 1;
    lua_pop(L, 1);

    lua_pushnumber(L, 3.14);
    if (!lua_isnumber(L, -1)) return 1;
    if (lua_tonumber(L, -1) != 3.14) return 1;
    if (lua_tointeger(L, -1) != 3) return 1;
    lua_pop(L, 1);

    lua_pushstring(L, "test");
    if (!lua_isstring(L, -1)) return 1;
    if (lua_type(L, -1) != LUA_TSTRING) return 1;
    {
        size_t len;
        const char *s = lua_tolstring(L, -1, &len);
        if (len != 4 || strcmp(s, "test") != 0) return 1;
    }
    lua_pop(L, 1);

    lua_pushcfunction(L, my_cfunction);
    if (!lua_iscfunction(L, -1)) return 1;
    if (!lua_isfunction(L, -1)) return 1;
    if (lua_tocfunction(L, -1) != my_cfunction) return 1;
    lua_pop(L, 1);

    lua_pushboolean(L, 1);
    if (!lua_isboolean(L, -1)) return 1;
    if (!lua_toboolean(L, -1)) return 1;
    lua_pop(L, 1);

    lua_pushinteger(L, 42);
    if (lua_tointeger(L, -1) != 42) return 1;
    lua_pop(L, 1);

    lua_pushlightuserdata(L, (void *)0xDEAD);
    if (!lua_islightuserdata(L, -1)) return 1;
    if (lua_touserdata(L, -1) != (void *)0xDEAD) return 1;
    lua_pop(L, 1);

    lua_pushthread(L);
    if (!lua_isthread(L, -1)) return 1;
    if (lua_tothread(L, -1) != L) return 1;
    lua_pop(L, 1);

    if (!lua_isnone(L, 100)) return 1;
    if (!lua_isnoneornil(L, 100)) return 1;

    lua_pushstring(L, "typename");
    {
        const char *tn = lua_typename(L, lua_type(L, -1));
        if (strcmp(tn, "string") != 0) return 1;
    }
    lua_pop(L, 1);

    return 0;
}

TEST(rawequal) {
    lua_pushnumber(L, 10);
    lua_pushnumber(L, 10);
    if (!lua_rawequal(L, -1, -2)) return 1;
    lua_pop(L, 2);
    return 0;
}

TEST(objlen) {
    lua_pushstring(L, "hello");
    if (lua_objlen(L, -1) != 5) return 1;
    lua_pop(L, 1);
    return 0;
}

TEST(topointer) {
    lua_newtable(L);
    if (lua_topointer(L, -1) == NULL) return 1;
    lua_pop(L, 1);
    return 0;
}

/* ===== Push functions ===== */

TEST(push_functions) {
    lua_pushnil(L);
    lua_pushnumber(L, 1.5);
    lua_pushinteger(L, 10);
    lua_pushlstring(L, "abc\0def", 7);
    lua_pushstring(L, "hello");
    lua_pushboolean(L, 0);
    lua_pushlightuserdata(L, NULL);
    lua_pushliteral(L, "literal");
    lua_pop(L, 8);

    lua_pushfstring(L, "num=%d str=%s", 42, "test");
    {
        const char *s = lua_tostring(L, -1);
        if (!s || strcmp(s, "num=42 str=test") != 0) return 1;
    }
    lua_pop(L, 1);

    lua_pushcclosure(L, my_cfunction, 0);
    if (!lua_iscfunction(L, -1)) return 1;
    lua_pop(L, 1);

    return 0;
}

/* ===== Table / get / set functions ===== */

TEST(table_ops) {
    lua_newtable(L);

    lua_pushstring(L, "key1");
    lua_pushnumber(L, 100);
    lua_settable(L, -3);

    lua_pushstring(L, "key1");
    lua_gettable(L, -2);
    if (lua_tonumber(L, -1) != 100) return 1;
    lua_pop(L, 1);

    lua_pushnumber(L, 200);
    lua_setfield(L, -2, "key2");

    lua_getfield(L, -1, "key2");
    if (lua_tonumber(L, -1) != 200) return 1;
    lua_pop(L, 1);

    lua_pushnumber(L, 300);
    lua_rawseti(L, -2, 1);

    lua_rawgeti(L, -1, 1);
    if (lua_tonumber(L, -1) != 300) return 1;
    lua_pop(L, 1);

    lua_pushstring(L, "rk");
    lua_pushnumber(L, 400);
    lua_rawset(L, -3);

    lua_pushstring(L, "rk");
    lua_rawget(L, -2);
    if (lua_tonumber(L, -1) != 400) return 1;
    lua_pop(L, 1);

    lua_pop(L, 1);

    lua_createtable(L, 5, 5);
    lua_pop(L, 1);

    return 0;
}

TEST(userdata) {
    int *ud = (int *)lua_newuserdata(L, sizeof(int));
    *ud = 42;
    if (!lua_isuserdata(L, -1)) return 1;
    if (*(int *)lua_touserdata(L, -1) != 42) return 1;
    lua_pop(L, 1);
    return 0;
}

TEST(metatable) {
    lua_newtable(L);
    lua_newtable(L);
    lua_setmetatable(L, -2);
    if (!lua_getmetatable(L, -1)) return 1;
    lua_pop(L, 2);
    return 0;
}

TEST(global) {
    lua_pushnumber(L, 777);
    lua_setglobal(L, "myvar");
    lua_getglobal(L, "myvar");
    if (lua_tonumber(L, -1) != 777) return 1;
    lua_pop(L, 1);
    return 0;
}

/* ===== Load and call ===== */

TEST(call_pcall) {
    lua_pushcfunction(L, my_cfunction);
    lua_call(L, 0, 1);
    if (lua_tonumber(L, -1) != 42) return 1;
    lua_pop(L, 1);

    lua_pushcfunction(L, my_cfunction);
    if (lua_pcall(L, 0, 1, 0) != 0) return 1;
    if (lua_tonumber(L, -1) != 42) return 1;
    lua_pop(L, 1);

    lua_pushcfunction(L, my_error_func);
    if (lua_pcall(L, 0, 0, 0) != LUA_ERRRUN) return 1;
    lua_pop(L, 1);

    return 0;
}

TEST(cpcall) {
    int data = 0;
    if (lua_cpcall(L, my_cpcall_func, &data) != 0) return 1;
    if (data != 12345) return 1;
    return 0;
}

/* ===== GC ===== */

TEST(gc) {
    lua_gc(L, LUA_GCSTOP, 0);
    lua_gc(L, LUA_GCRESTART, 0);
    lua_gc(L, LUA_GCCOLLECT, 0);
    lua_gc(L, LUA_GCCOUNT, 0);
    lua_gc(L, LUA_GCCOUNTB, 0);
    lua_gc(L, LUA_GCSTEP, 0);
    lua_gc(L, LUA_GCSETPAUSE, 200);
    lua_gc(L, LUA_GCSETSTEPMUL, 200);
    return 0;
}

/* ===== Miscellaneous ===== */

TEST(error_handling) {
    lua_pushcfunction(L, my_error_func);
    int r = lua_pcall(L, 0, 0, 0);
    if (r != LUA_ERRRUN) return 1;
    lua_pop(L, 1);
    return 0;
}

TEST(next) {
    lua_newtable(L);
    lua_pushnumber(L, 1);
    lua_setfield(L, -2, "a");
    lua_pushnumber(L, 2);
    lua_setfield(L, -2, "b");

    int count = 0;
    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
        count++;
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    if (count != 2) return 1;
    return 0;
}

TEST(concat) {
    lua_pushstring(L, "hello");
    lua_pushstring(L, " ");
    lua_pushstring(L, "world");
    lua_concat(L, 3);
    if (strcmp(lua_tostring(L, -1), "hello world") != 0) return 1;
    lua_pop(L, 1);
    return 0;
}

TEST(getallocf) {
    void *ud;
    lua_Alloc f = lua_getallocf(L, &ud);
    if (f == NULL) return 1;
    return 0;
}

/* ===== Macros ===== */

TEST(macros) {
    lua_pushstring(L, "test");
    lua_strlen(L, -1);
    lua_pop(L, 1);

    lua_register(L, "my_cfunc2", my_cfunction2);
    lua_getglobal(L, "my_cfunc2");
    if (!lua_isfunction(L, -1)) return 1;
    lua_pop(L, 1);

    lua_newtable(L);
    if (!lua_istable(L, -1)) return 1;
    lua_pop(L, 1);

    return 0;
}

/* ===== Debug API ===== */

static int getstack_helper(lua_State *L) {
    lua_Debug ar;
    if (!lua_getstack(L, 0, &ar)) {
        lua_pushboolean(L, 0);
        return 1;
    }
    lua_getinfo(L, "nSl", &ar);
    lua_pushboolean(L, 1);
    return 1;
}

TEST(getstack_getinfo) {
    /* Must call from Lua context so there's a stack frame */
    luaL_dostring(L, "function test_getstack(f) return f() end");
    lua_getglobal(L, "test_getstack");
    lua_pushcfunction(L, getstack_helper);
    if (lua_pcall(L, 1, 1, 0) != 0) {
        printf("(%s) ", lua_tostring(L, -1));
        lua_pop(L, 1);
        return 1;
    }
    int ok = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return ok ? 0 : 1;
}

TEST(upvalue) {
    lua_pushnumber(L, 99);
    lua_pushcclosure(L, my_cfunction, 1);
    const char *name = lua_getupvalue(L, -1, 1);
    if (!name) return 1;
    lua_pop(L, 1);

    lua_pushnumber(L, 50);
    const char *name2 = lua_setupvalue(L, -2, 1);
    if (!name2) return 1;

    lua_pop(L, 1);
    return 0;
}

TEST(sethook) {
    lua_sethook(L, my_hook, LUA_MASKCALL, 0);
    lua_sethook(L, NULL, 0, 0);
    return 0;
}

/* ===== Auxiliary library ===== */

static const luaL_Reg mylib[] = {
    {"myfunc", my_cfunction},
    {"myfunc2", my_cfunction2},
    {NULL, NULL}
};

TEST(luaL_register_test) {
    luaL_register(L, "mylib", mylib);
    lua_getfield(L, -1, "myfunc");
    if (!lua_isfunction(L, -1)) return 1;
    lua_pop(L, 2);
    return 0;
}

static int my_tostring_meta(lua_State *L) {
    lua_pushstring(L, "meta_tostring_result");
    return 1;
}

TEST(callmeta) {
    lua_newuserdata(L, 4);
    lua_newtable(L);
    lua_pushcfunction(L, my_tostring_meta);
    lua_setfield(L, -2, "__tostring");
    lua_setmetatable(L, -2);

    if (!luaL_callmeta(L, -1, "__tostring")) return 1;
    if (strcmp(lua_tostring(L, -1), "meta_tostring_result") != 0) return 1;
    lua_pop(L, 2);
    return 0;
}

TEST(typerror) {
    lua_pushcfunction(L, my_error_func);
    lua_pushnumber(L, 42);
    int r = lua_pcall(L, 0, 0, 0);
    if (r != LUA_ERRRUN) return 1;
    lua_pop(L, 1);
    return 0;
}

static int check_args_func(lua_State *L) {
    luaL_checktype(L, 1, LUA_TNUMBER);
    luaL_checkany(L, 1);
    luaL_checkstack(L, 10, "not enough stack");

    lua_Number n = luaL_checknumber(L, 1);
    luaL_optnumber(L, 2, 0.0);
    lua_Integer i = luaL_checkinteger(L, 1);
    luaL_optinteger(L, 2, 0);
    (void)n; (void)i;

    size_t len;
    luaL_checklstring(L, 3, &len);
    luaL_optlstring(L, 4, "default", NULL);

    luaL_argcheck(L, n > 0, 1, "must be positive");

    const char *s = luaL_checkstring(L, 3);
    luaL_optstring(L, 4, "def");
    (void)s;

    int ci = luaL_checkint(L, 1);
    luaL_optint(L, 2, 0);
    long cl = luaL_checklong(L, 1);
    luaL_optlong(L, 2, 0);
    (void)ci; (void)cl;

    return 0;
}

TEST(check_functions) {
    lua_pushcfunction(L, check_args_func);
    lua_pushnumber(L, 5);
    lua_pushnil(L);
    lua_pushstring(L, "str");
    if (lua_pcall(L, 3, 0, 0) != 0) {
        printf("(%s) ", lua_tostring(L, -1));
        lua_pop(L, 1);
        return 1;
    }
    return 0;
}

TEST(newmetatable_checkudata) {
    luaL_newmetatable(L, "TestMeta");
    lua_pop(L, 1);

    int *ud = (int *)lua_newuserdata(L, sizeof(int));
    *ud = 99;
    luaL_getmetatable(L, "TestMeta");
    lua_setmetatable(L, -2);

    void *p = luaL_checkudata(L, -1, "TestMeta");
    if (p == NULL || *(int *)p != 99) return 1;
    lua_pop(L, 1);
    return 0;
}

TEST(where_error) {
    luaL_where(L, 0);
    lua_pop(L, 1);
    return 0;
}

static int checkoption_func(lua_State *L) {
    static const char *const opts[] = {"alpha", "beta", "gamma", NULL};
    luaL_checkoption(L, 1, "alpha", opts);
    return 0;
}

TEST(checkoption) {
    lua_pushcfunction(L, checkoption_func);
    lua_pushstring(L, "beta");
    if (lua_pcall(L, 1, 0, 0) != 0) {
        lua_pop(L, 1);
        return 1;
    }
    return 0;
}

TEST(ref_unref) {
    lua_pushnumber(L, 42);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    if (ref == LUA_NOREF) return 1;

    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    if (lua_tonumber(L, -1) != 42) return 1;
    lua_pop(L, 1);

    luaL_unref(L, LUA_REGISTRYINDEX, ref);
    return 0;
}

TEST(loadbuffer) {
    const char *code = "return 1 + 2";
    if (luaL_loadbuffer(L, code, strlen(code), "test") != 0) {
        lua_pop(L, 1);
        return 1;
    }
    if (lua_pcall(L, 0, 1, 0) != 0) {
        lua_pop(L, 1);
        return 1;
    }
    if (lua_tonumber(L, -1) != 3) return 1;
    lua_pop(L, 1);
    return 0;
}

TEST(loadstring) {
    if (luaL_loadstring(L, "return 'hello'") != 0) {
        lua_pop(L, 1);
        return 1;
    }
    if (lua_pcall(L, 0, 1, 0) != 0) {
        lua_pop(L, 1);
        return 1;
    }
    if (strcmp(lua_tostring(L, -1), "hello") != 0) return 1;
    lua_pop(L, 1);
    return 0;
}

TEST(dostring) {
    if (luaL_dostring(L, "myvar_ds = 123") != 0) {
        lua_pop(L, 1);
        return 1;
    }
    lua_getglobal(L, "myvar_ds");
    if (lua_tonumber(L, -1) != 123) return 1;
    lua_pop(L, 1);
    return 0;
}

TEST(buffer_api) {
    luaL_Buffer b;
    luaL_buffinit(L, &b);
    luaL_addlstring(&b, "hel", 3);
    luaL_addstring(&b, "lo");
    luaL_addchar(&b, '!');
    luaL_pushresult(&b);
    if (strcmp(lua_tostring(L, -1), "hello!") != 0) return 1;
    lua_pop(L, 1);
    return 0;
}

TEST(typename_macro) {
    lua_pushnumber(L, 1);
    const char *tn = luaL_typename(L, -1);
    if (strcmp(tn, "number") != 0) return 1;
    lua_pop(L, 1);
    return 0;
}

/* ===== Standard libraries ===== */

TEST(openlibs) {
    /* luaL_openlibs is already called, just verify some globals exist */
    lua_getglobal(L, "table");
    if (!lua_istable(L, -1)) return 1;
    lua_pop(L, 1);

    lua_getglobal(L, "string");
    if (!lua_istable(L, -1)) return 1;
    lua_pop(L, 1);

    lua_getglobal(L, "math");
    if (!lua_istable(L, -1)) return 1;
    lua_pop(L, 1);

    lua_getglobal(L, "os");
    if (!lua_istable(L, -1)) return 1;
    lua_pop(L, 1);

    lua_getglobal(L, "debug");
    if (!lua_istable(L, -1)) return 1;
    lua_pop(L, 1);

    return 0;
}

/* ===== Main ===== */

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    int fails = 0;

    lua_State *L = luaL_newstate();
    if (!L) {
        fprintf(stderr, "Failed to create Lua state\n");
        return 1;
    }
    luaL_openlibs(L);

    /* Ensure package/require is available (Luau build needs this) */
    lua_getglobal(L, "require");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        luaopen_package(L);
        luaopen_io(L);
    } else {
        lua_pop(L, 1);
    }

    /* Provide loadstring/loadfile if missing (Luau doesn't expose them) */
    lua_getglobal(L, "loadstring");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_pushcfunction(L, l_loadstring);
        lua_setglobal(L, "loadstring");
    } else {
        lua_pop(L, 1);
    }
    lua_getglobal(L, "loadfile");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_pushcfunction(L, l_loadfile);
        lua_setglobal(L, "loadfile");
    } else {
        lua_pop(L, 1);
    }

    /* Shim debug.getinfo if missing (Luau uses debug.info instead) */
    luaL_dostring(L,
        "if debug and not debug.getinfo and debug.info then\n"
        "  debug.getinfo = function(level, what)\n"
        "    what = what or 'nSl'\n"
        "    level = level + 1\n"
        "    local info = {}\n"
        "    if what:find('s') or what:find('S') then\n"
        "      info.short_src = debug.info(level, 's')\n"
        "      info.source = info.short_src\n"
        "    end\n"
        "    if what:find('l') then\n"
        "      info.currentline = debug.info(level, 'l')\n"
        "    end\n"
        "    if what:find('n') then\n"
        "      info.name = debug.info(level, 'n')\n"
        "    end\n"
        "    return info\n"
        "  end\n"
        "end");

    printf("Running Lua 5.1 C API tests (Defold subset):\n");

    /* State */
    RUN(atpanic);
    RUN(newthread);

    /* Stack */
    RUN(stack_ops);

    /* Access */
    RUN(type_checks);
    RUN(rawequal);
    RUN(objlen);
    RUN(topointer);

    /* Push */
    RUN(push_functions);

    /* Table / get / set */
    RUN(table_ops);
    RUN(userdata);
    RUN(metatable);
    RUN(global);

    /* Load and call */
    RUN(call_pcall);
    RUN(cpcall);

    /* GC */
    RUN(gc);

    /* Misc */
    RUN(error_handling);
    RUN(next);
    RUN(concat);
    RUN(getallocf);
    RUN(macros);

    /* Debug */
    RUN(getstack_getinfo);
    RUN(upvalue);
    RUN(sethook);

    /* Aux library */
    RUN(luaL_register_test);
    RUN(callmeta);
    RUN(typerror);
    RUN(check_functions);
    RUN(newmetatable_checkudata);
    RUN(where_error);
    RUN(checkoption);
    RUN(ref_unref);
    RUN(loadbuffer);
    RUN(loadstring);
    RUN(dostring);
    RUN(buffer_api);
    RUN(typename_macro);

    /* Standard libs */
    RUN(openlibs);

    printf("\nResults: %d C API test(s) failed\n", fails);

    /* ===== Lua file tests ===== */
    printf("\nRunning Lua file tests:\n");
    {
        /* Compute base dir from argv[0] (e.g. "example/test_lua51" -> "example/") */
        char basedir[512] = "";
        if (argv[0]) {
            const char *last_sep = strrchr(argv[0], '/');
            if (last_sep) {
                size_t len = (size_t)(last_sep - argv[0] + 1);
                if (len < sizeof(basedir)) {
                    memcpy(basedir, argv[0], len);
                    basedir[len] = '\0';
                }
            }
        }

        /* Set package.path with root dir tests/lume */
        char pathcmd[1024];
        snprintf(pathcmd, sizeof(pathcmd),
            "package.path = \"%stests/lume/?.lua;%stests/lume/?/init.lua\"",
            basedir, basedir);
        luaL_dostring(L, pathcmd);

        /* Run tests/lume/test.lua */
        char filepath[512];
        snprintf(filepath, sizeof(filepath), "%stests/lume/test.lua", basedir);

        printf("  Loading %s\n", filepath);
        if (luaL_dofile(L, filepath) != 0) {
            printf("  FAIL: %s\n", lua_tostring(L, -1));
            lua_pop(L, 1);
            fails++;
        }

        /* json.lua: set package.path and loadfile for relative paths */
        snprintf(pathcmd, sizeof(pathcmd),
            "do\n"
            "  local bd = \"%stests/json.lua/\"\n"
            "  package.path = bd .. \"?.lua;\" .. bd .. \"bench/?.lua;\" .. bd .. \"bench/?/init.lua\"\n"
            "  local real_loadfile = loadfile\n"
            "  local dirs = { bd .. \"test/\", bd .. \"bench/\", bd }\n"
            "  loadfile = function(name)\n"
            "    for _, d in ipairs(dirs) do\n"
            "      local f, err = real_loadfile(d .. name)\n"
            "      if f then return f, err end\n"
            "    end\n"
            "    return nil, \"cannot find '\" .. name .. \"'\"\n"
            "  end\n"
            "end",
            basedir);
        luaL_dostring(L, pathcmd);

        /* Run json.lua test */
        snprintf(filepath, sizeof(filepath),
            "%stests/json.lua/test/test.lua", basedir);
        printf("  Loading %s\n", filepath);
        if (luaL_dofile(L, filepath) != 0) {
            printf("  FAIL: %s\n", lua_tostring(L, -1));
            lua_pop(L, 1);
            fails++;
        }

        /* Run json.lua bench */
        printf("\n  Running json.lua benchmarks:\n");
        snprintf(filepath, sizeof(filepath),
            "%stests/json.lua/bench/bench_all.lua", basedir);
        if (luaL_dofile(L, filepath) != 0) {
            printf("  FAIL: %s\n", lua_tostring(L, -1));
            lua_pop(L, 1);
            fails++;
        }
    }

    lua_close(L);
    return fails > 0 ? 1 : 0;
}
