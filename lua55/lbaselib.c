/*
** $Id: lbaselib.c $
** Basic library
** See Copyright Notice in lua.h
*/

#define lbaselib_c
#define LUA_LIB

#include "lprefix.h"


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"
#include "llimits.h"


static int luaB_print (lua55_State *L) {
  int n = lua55_gettop(L);  /* number of arguments */
  int i;
  for (i = 1; i <= n; i++) {  /* for each argument */
    size_t l;
    const char *s = lua55L_tolstring(L, i, &l);  /* convert it to string */
    if (i > 1)  /* not the first element? */
      lua_writestring("\t", 1);  /* add a tab before it */
    lua_writestring(s, l);  /* print it */
    lua55_pop(L, 1);  /* pop result */
  }
  lua_writeline();
  return 0;
}


/*
** Creates a warning with all given arguments.
** Check first for errors; otherwise an error may interrupt
** the composition of a warning, leaving it unfinished.
*/
static int luaB_warn (lua55_State *L) {
  int n = lua55_gettop(L);  /* number of arguments */
  int i;
  lua55L_checkstring(L, 1);  /* at least one argument */
  for (i = 2; i <= n; i++)
    lua55L_checkstring(L, i);  /* make sure all arguments are strings */
  for (i = 1; i < n; i++)  /* compose warning */
    lua55_warning(L, lua55_tostring(L, i), 1);
  lua55_warning(L, lua55_tostring(L, n), 0);  /* close warning */
  return 0;
}


#define SPACECHARS	" \f\n\r\t\v"

static const char *b_str2int (const char *s, unsigned base, lua_Integer *pn) {
  lua_Unsigned n = 0;
  int neg = 0;
  s += strspn(s, SPACECHARS);  /* skip initial spaces */
  if (*s == '-') { s++; neg = 1; }  /* handle sign */
  else if (*s == '+') s++;
  if (!isalnum(cast_uchar(*s)))  /* no digit? */
    return NULL;
  do {
    unsigned digit = cast_uint(isdigit(cast_uchar(*s))
                               ? *s - '0'
                               : (toupper(cast_uchar(*s)) - 'A') + 10);
    if (digit >= base) return NULL;  /* invalid numeral */
    n = n * base + digit;
    s++;
  } while (isalnum(cast_uchar(*s)));
  s += strspn(s, SPACECHARS);  /* skip trailing spaces */
  *pn = (lua_Integer)((neg) ? (0u - n) : n);
  return s;
}


static int luaB_tonumber (lua55_State *L) {
  if (lua55_isnoneornil(L, 2)) {  /* standard conversion? */
    if (lua55_type(L, 1) == LUA_TNUMBER) {  /* already a number? */
      lua55_settop(L, 1);  /* yes; return it */
      return 1;
    }
    else {
      size_t l;
      const char *s = lua55_tolstring(L, 1, &l);
      if (s != NULL && lua55_stringtonumber(L, s) == l + 1)
        return 1;  /* successful conversion to number */
      /* else not a number */
      lua55L_checkany(L, 1);  /* (but there must be some parameter) */
    }
  }
  else {
    size_t l;
    const char *s;
    lua_Integer n = 0;  /* to avoid warnings */
    lua_Integer base = lua55L_checkinteger(L, 2);
    lua55L_checktype(L, 1, LUA_TSTRING);  /* no numbers as strings */
    s = lua55_tolstring(L, 1, &l);
    lua55L_argcheck(L, 2 <= base && base <= 36, 2, "base out of range");
    if (b_str2int(s, cast_uint(base), &n) == s + l) {
      lua55_pushinteger(L, n);
      return 1;
    }  /* else not a number */
  }  /* else not a number */
  lua55L_pushfail(L);  /* not a number */
  return 1;
}


static int luaB_error (lua55_State *L) {
  int level = (int)lua55L_optinteger(L, 2, 1);
  lua55_settop(L, 1);
  if (lua55_type(L, 1) == LUA_TSTRING && level > 0) {
    lua55L_where(L, level);   /* add extra information */
    lua55_pushvalue(L, 1);
    lua55_concat(L, 2);
  }
  return lua55_error(L);
}


static int luaB_getmetatable (lua55_State *L) {
  lua55L_checkany(L, 1);
  if (!lua55_getmetatable(L, 1)) {
    lua55_pushnil(L);
    return 1;  /* no metatable */
  }
  lua55L_getmetafield(L, 1, "__metatable");
  return 1;  /* returns either __metatable field (if present) or metatable */
}


static int luaB_setmetatable (lua55_State *L) {
  int t = lua55_type(L, 2);
  lua55L_checktype(L, 1, LUA_TTABLE);
  lua55L_argexpected(L, t == LUA_TNIL || t == LUA_TTABLE, 2, "nil or table");
  if (l_unlikely(lua55L_getmetafield(L, 1, "__metatable") != LUA_TNIL))
    return lua55L_error(L, "cannot change a protected metatable");
  lua55_settop(L, 2);
  lua55_setmetatable(L, 1);
  return 1;
}


static int luaB_rawequal (lua55_State *L) {
  lua55L_checkany(L, 1);
  lua55L_checkany(L, 2);
  lua55_pushboolean(L, lua55_rawequal(L, 1, 2));
  return 1;
}


static int luaB_rawlen (lua55_State *L) {
  int t = lua55_type(L, 1);
  lua55L_argexpected(L, t == LUA_TTABLE || t == LUA_TSTRING, 1,
                      "table or string");
  lua55_pushinteger(L, l_castU2S(lua55_rawlen(L, 1)));
  return 1;
}


static int luaB_rawget (lua55_State *L) {
  lua55L_checktype(L, 1, LUA_TTABLE);
  lua55L_checkany(L, 2);
  lua55_settop(L, 2);
  lua55_rawget(L, 1);
  return 1;
}

static int luaB_rawset (lua55_State *L) {
  lua55L_checktype(L, 1, LUA_TTABLE);
  lua55L_checkany(L, 2);
  lua55L_checkany(L, 3);
  lua55_settop(L, 3);
  lua55_rawset(L, 1);
  return 1;
}


static int pushmode (lua55_State *L, int oldmode) {
  if (oldmode == -1)
    lua55L_pushfail(L);  /* invalid call to 'lua55_gc' */
  else
    lua55_pushstring(L, (oldmode == LUA_GCINC) ? "incremental"
                                             : "generational");
  return 1;
}


/*
** check whether call to 'lua55_gc' was valid (not inside a finalizer)
*/
#define checkvalres(res) { if (res == -1) break; }

static int luaB_collectgarbage (lua55_State *L) {
  static const char *const opts[] = {"stop", "restart", "collect",
    "count", "step", "isrunning", "generational", "incremental",
    "param", NULL};
  static const char optsnum[] = {LUA_GCSTOP, LUA_GCRESTART, LUA_GCCOLLECT,
    LUA_GCCOUNT, LUA_GCSTEP, LUA_GCISRUNNING, LUA_GCGEN, LUA_GCINC,
    LUA_GCPARAM};
  int o = optsnum[lua55L_checkoption(L, 1, "collect", opts)];
  switch (o) {
    case LUA_GCCOUNT: {
      int k = lua55_gc(L, o);
      int b = lua55_gc(L, LUA_GCCOUNTB);
      checkvalres(k);
      lua55_pushnumber(L, (lua_Number)k + ((lua_Number)b/1024));
      return 1;
    }
    case LUA_GCSTEP: {
      lua_Integer n = lua55L_optinteger(L, 2, 0);
      int res = lua55_gc(L, o, cast_sizet(n));
      checkvalres(res);
      lua55_pushboolean(L, res);
      return 1;
    }
    case LUA_GCISRUNNING: {
      int res = lua55_gc(L, o);
      checkvalres(res);
      lua55_pushboolean(L, res);
      return 1;
    }
    case LUA_GCGEN: {
      return pushmode(L, lua55_gc(L, o));
    }
    case LUA_GCINC: {
      return pushmode(L, lua55_gc(L, o));
    }
    case LUA_GCPARAM: {
      static const char *const params[] = {
        "minormul", "majorminor", "minormajor",
        "pause", "stepmul", "stepsize", NULL};
      static const char pnum[] = {
        LUA_GCPMINORMUL, LUA_GCPMAJORMINOR, LUA_GCPMINORMAJOR,
        LUA_GCPPAUSE, LUA_GCPSTEPMUL, LUA_GCPSTEPSIZE};
      int p = pnum[lua55L_checkoption(L, 2, NULL, params)];
      lua_Integer value = lua55L_optinteger(L, 3, -1);
      lua55_pushinteger(L, lua55_gc(L, o, p, (int)value));
      return 1;
    }
    default: {
      int res = lua55_gc(L, o);
      checkvalres(res);
      lua55_pushinteger(L, res);
      return 1;
    }
  }
  lua55L_pushfail(L);  /* invalid call (inside a finalizer) */
  return 1;
}


static int luaB_type (lua55_State *L) {
  int t = lua55_type(L, 1);
  lua55L_argcheck(L, t != LUA_TNONE, 1, "value expected");
  lua55_pushstring(L, lua55_typename(L, t));
  return 1;
}


static int luaB_next (lua55_State *L) {
  lua55L_checktype(L, 1, LUA_TTABLE);
  lua55_settop(L, 2);  /* create a 2nd argument if there isn't one */
  if (lua55_next(L, 1))
    return 2;
  else {
    lua55_pushnil(L);
    return 1;
  }
}


static int pairscont (lua55_State *L, int status, lua_KContext k) {
  (void)L; (void)status; (void)k;  /* unused */
  return 4;  /* __pairs did all the work, just return its results */
}

static int luaB_pairs (lua55_State *L) {
  lua55L_checkany(L, 1);
  if (lua55L_getmetafield(L, 1, "__pairs") == LUA_TNIL) {  /* no metamethod? */
    lua55_pushcfunction(L, luaB_next);  /* will return generator and */
    lua55_pushvalue(L, 1);  /* state */
    lua55_pushnil(L);  /* initial value */
    lua55_pushnil(L);  /* to-be-closed object */
  }
  else {
    lua55_pushvalue(L, 1);  /* argument 'self' to metamethod */
    lua55_callk(L, 1, 4, 0, pairscont);  /* get 4 values from metamethod */
  }
  return 4;
}


/*
** Traversal function for 'ipairs'
*/
static int ipairsaux (lua55_State *L) {
  lua_Integer i = lua55L_checkinteger(L, 2);
  i = lua55L_intop(+, i, 1);
  lua55_pushinteger(L, i);
  return (lua55_geti(L, 1, i) == LUA_TNIL) ? 1 : 2;
}


/*
** 'ipairs' function. Returns 'ipairsaux', given "table", 0.
** (The given "table" may not be a table.)
*/
static int luaB_ipairs (lua55_State *L) {
  lua55L_checkany(L, 1);
  lua55_pushcfunction(L, ipairsaux);  /* iteration function */
  lua55_pushvalue(L, 1);  /* state */
  lua55_pushinteger(L, 0);  /* initial value */
  return 3;
}


static int load_aux (lua55_State *L, int status, int envidx) {
  if (l_likely(status == LUA_OK)) {
    if (envidx != 0) {  /* 'env' parameter? */
      lua55_pushvalue(L, envidx);  /* environment for loaded function */
      if (!lua55_setupvalue(L, -2, 1))  /* set it as 1st upvalue */
        lua55_pop(L, 1);  /* remove 'env' if not used by previous call */
    }
    return 1;
  }
  else {  /* error (message is on top of the stack) */
    lua55L_pushfail(L);
    lua55_insert(L, -2);  /* put before error message */
    return 2;  /* return fail plus error message */
  }
}


static const char *getMode (lua55_State *L, int idx) {
  const char *mode = lua55L_optstring(L, idx, "bt");
  if (strchr(mode, 'B') != NULL)  /* Lua code cannot use fixed buffers */
    lua55L_argerror(L, idx, "invalid mode");
  return mode;
}


static int luaB_loadfile (lua55_State *L) {
  const char *fname = lua55L_optstring(L, 1, NULL);
  const char *mode = getMode(L, 2);
  int env = (!lua55_isnone(L, 3) ? 3 : 0);  /* 'env' index or 0 if no 'env' */
  int status = lua55L_loadfilex(L, fname, mode);
  return load_aux(L, status, env);
}


/*
** {======================================================
** Generic Read function
** =======================================================
*/


/*
** reserved slot, above all arguments, to hold a copy of the returned
** string to avoid it being collected while parsed. 'load' has four
** optional arguments (chunk, source name, mode, and environment).
*/
#define RESERVEDSLOT	5


/*
** Reader for generic 'load' function: 'lua55_load' uses the
** stack for internal stuff, so the reader cannot change the
** stack top. Instead, it keeps its resulting string in a
** reserved slot inside the stack.
*/
static const char *generic_reader (lua55_State *L, void *ud, size_t *size) {
  (void)(ud);  /* not used */
  lua55L_checkstack(L, 2, "too many nested functions");
  lua55_pushvalue(L, 1);  /* get function */
  lua55_call(L, 0, 1);  /* call it */
  if (lua55_isnil(L, -1)) {
    lua55_pop(L, 1);  /* pop result */
    *size = 0;
    return NULL;
  }
  else if (l_unlikely(!lua55_isstring(L, -1)))
    lua55L_error(L, "reader function must return a string");
  lua55_replace(L, RESERVEDSLOT);  /* save string in reserved slot */
  return lua55_tolstring(L, RESERVEDSLOT, size);
}


static int luaB_load (lua55_State *L) {
  int status;
  size_t l;
  const char *s = lua55_tolstring(L, 1, &l);
  const char *mode = getMode(L, 3);
  int env = (!lua55_isnone(L, 4) ? 4 : 0);  /* 'env' index or 0 if no 'env' */
  if (s != NULL) {  /* loading a string? */
    const char *chunkname = lua55L_optstring(L, 2, s);
    status = lua55L_loadbufferx(L, s, l, chunkname, mode);
  }
  else {  /* loading from a reader function */
    const char *chunkname = lua55L_optstring(L, 2, "=(load)");
    lua55L_checktype(L, 1, LUA_TFUNCTION);
    lua55_settop(L, RESERVEDSLOT);  /* create reserved slot */
    status = lua55_load(L, generic_reader, NULL, chunkname, mode);
  }
  return load_aux(L, status, env);
}

/* }====================================================== */


static int dofilecont (lua55_State *L, int d1, lua_KContext d2) {
  (void)d1;  (void)d2;  /* only to match 'lua_Kfunction' prototype */
  return lua55_gettop(L) - 1;
}


static int luaB_dofile (lua55_State *L) {
  const char *fname = lua55L_optstring(L, 1, NULL);
  lua55_settop(L, 1);
  if (l_unlikely(lua55L_loadfile(L, fname) != LUA_OK))
    return lua55_error(L);
  lua55_callk(L, 0, LUA_MULTRET, 0, dofilecont);
  return dofilecont(L, 0, 0);
}


static int luaB_assert (lua55_State *L) {
  if (l_likely(lua55_toboolean(L, 1)))  /* condition is true? */
    return lua55_gettop(L);  /* return all arguments */
  else {  /* error */
    lua55L_checkany(L, 1);  /* there must be a condition */
    lua55_remove(L, 1);  /* remove it */
    lua55_pushliteral(L, "assertion failed!");  /* default message */
    lua55_settop(L, 1);  /* leave only message (default if no other one) */
    return luaB_error(L);  /* call 'error' */
  }
}


static int luaB_select (lua55_State *L) {
  int n = lua55_gettop(L);
  if (lua55_type(L, 1) == LUA_TSTRING && *lua55_tostring(L, 1) == '#') {
    lua55_pushinteger(L, n-1);
    return 1;
  }
  else {
    lua_Integer i = lua55L_checkinteger(L, 1);
    if (i < 0) i = n + i;
    else if (i > n) i = n;
    lua55L_argcheck(L, 1 <= i, 1, "index out of range");
    return n - (int)i;
  }
}


/*
** Continuation function for 'pcall' and 'xpcall'. Both functions
** already pushed a 'true' before doing the call, so in case of success
** 'finishpcall' only has to return everything in the stack minus
** 'extra' values (where 'extra' is exactly the number of items to be
** ignored).
*/
static int finishpcall (lua55_State *L, int status, lua_KContext extra) {
  if (l_unlikely(status != LUA_OK && status != LUA_YIELD)) {  /* error? */
    lua55_pushboolean(L, 0);  /* first result (false) */
    lua55_pushvalue(L, -2);  /* error message */
    return 2;  /* return false, msg */
  }
  else
    return lua55_gettop(L) - (int)extra;  /* return all results */
}


static int luaB_pcall (lua55_State *L) {
  int status;
  lua55L_checkany(L, 1);
  lua55_pushboolean(L, 1);  /* first result if no errors */
  lua55_insert(L, 1);  /* put it in place */
  status = lua55_pcallk(L, lua55_gettop(L) - 2, LUA_MULTRET, 0, 0, finishpcall);
  return finishpcall(L, status, 0);
}


/*
** Do a protected call with error handling. After 'lua55_rotate', the
** stack will have <f, err, true, f, [args...]>; so, the function passes
** 2 to 'finishpcall' to skip the 2 first values when returning results.
*/
static int luaB_xpcall (lua55_State *L) {
  int status;
  int n = lua55_gettop(L);
  lua55L_checktype(L, 2, LUA_TFUNCTION);  /* check error function */
  lua55_pushboolean(L, 1);  /* first result */
  lua55_pushvalue(L, 1);  /* function */
  lua55_rotate(L, 3, 2);  /* move them below function's arguments */
  status = lua55_pcallk(L, n - 2, LUA_MULTRET, 2, 2, finishpcall);
  return finishpcall(L, status, 2);
}


static int luaB_tostring (lua55_State *L) {
  lua55L_checkany(L, 1);
  lua55L_tolstring(L, 1, NULL);
  return 1;
}


static const luaL_Reg base_funcs[] = {
  {"assert", luaB_assert},
  {"collectgarbage", luaB_collectgarbage},
  {"dofile", luaB_dofile},
  {"error", luaB_error},
  {"getmetatable", luaB_getmetatable},
  {"ipairs", luaB_ipairs},
  {"loadfile", luaB_loadfile},
  {"load", luaB_load},
  {"next", luaB_next},
  {"pairs", luaB_pairs},
  {"pcall", luaB_pcall},
  {"print", luaB_print},
  {"warn", luaB_warn},
  {"rawequal", luaB_rawequal},
  {"rawlen", luaB_rawlen},
  {"rawget", luaB_rawget},
  {"rawset", luaB_rawset},
  {"select", luaB_select},
  {"setmetatable", luaB_setmetatable},
  {"tonumber", luaB_tonumber},
  {"tostring", luaB_tostring},
  {"type", luaB_type},
  {"xpcall", luaB_xpcall},
  /* placeholders */
  {LUA_GNAME, NULL},
  {"_VERSION", NULL},
  {NULL, NULL}
};


LUAMOD_API int lua55open_base (lua55_State *L) {
  /* open lib into global table */
  lua55_pushglobaltable(L);
  lua55L_setfuncs(L, base_funcs, 0);
  /* set global _G */
  lua55_pushvalue(L, -1);
  lua55_setfield(L, -2, LUA_GNAME);
  /* set global _VERSION */
  lua55_pushliteral(L, LUA_VERSION);
  lua55_setfield(L, -2, "_VERSION");
  return 1;
}

