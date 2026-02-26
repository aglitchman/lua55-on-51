/*
** $Id: lcorolib.c $
** Coroutine Library
** See Copyright Notice in lua.h
*/

#define lcorolib_c
#define LUA_LIB

#include "lprefix.h"


#include <stdlib.h>

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"
#include "llimits.h"


static lua55_State *getco (lua55_State *L) {
  lua55_State *co = lua55_tothread(L, 1);
  lua55L_argexpected(L, co, 1, "thread");
  return co;
}


/*
** Resumes a coroutine. Returns the number of results for non-error
** cases or -1 for errors.
*/
static int auxresume (lua55_State *L, lua55_State *co, int narg) {
  int status, nres;
  if (l_unlikely(!lua55_checkstack(co, narg))) {
    lua55_pushliteral(L, "too many arguments to resume");
    return -1;  /* error flag */
  }
  lua55_xmove(L, co, narg);
  status = lua55_resume(co, L, narg, &nres);
  if (l_likely(status == LUA_OK || status == LUA_YIELD)) {
    if (l_unlikely(!lua55_checkstack(L, nres + 1))) {
      lua55_pop(co, nres);  /* remove results anyway */
      lua55_pushliteral(L, "too many results to resume");
      return -1;  /* error flag */
    }
    lua55_xmove(co, L, nres);  /* move yielded values */
    return nres;
  }
  else {
    lua55_xmove(co, L, 1);  /* move error message */
    return -1;  /* error flag */
  }
}


static int luaB_coresume (lua55_State *L) {
  lua55_State *co = getco(L);
  int r;
  r = auxresume(L, co, lua55_gettop(L) - 1);
  if (l_unlikely(r < 0)) {
    lua55_pushboolean(L, 0);
    lua55_insert(L, -2);
    return 2;  /* return false + error message */
  }
  else {
    lua55_pushboolean(L, 1);
    lua55_insert(L, -(r + 1));
    return r + 1;  /* return true + 'resume' returns */
  }
}


static int luaB_auxwrap (lua55_State *L) {
  lua55_State *co = lua55_tothread(L, lua55_upvalueindex(1));
  int r = auxresume(L, co, lua55_gettop(L));
  if (l_unlikely(r < 0)) {  /* error? */
    int stat = lua55_status(co);
    if (stat != LUA_OK && stat != LUA_YIELD) {  /* error in the coroutine? */
      stat = lua55_closethread(co, L);  /* close its tbc variables */
      lua_assert(stat != LUA_OK);
      lua55_xmove(co, L, 1);  /* move error message to the caller */
    }
    if (stat != LUA_ERRMEM &&  /* not a memory error and ... */
        lua55_type(L, -1) == LUA_TSTRING) {  /* ... error object is a string? */
      lua55L_where(L, 1);  /* add extra info, if available */
      lua55_insert(L, -2);
      lua55_concat(L, 2);
    }
    return lua55_error(L);  /* propagate error */
  }
  return r;
}


static int luaB_cocreate (lua55_State *L) {
  lua55_State *NL;
  lua55L_checktype(L, 1, LUA_TFUNCTION);
  NL = lua55_newthread(L);
  lua55_pushvalue(L, 1);  /* move function to top */
  lua55_xmove(L, NL, 1);  /* move function from L to NL */
  return 1;
}


static int luaB_cowrap (lua55_State *L) {
  luaB_cocreate(L);
  lua55_pushcclosure(L, luaB_auxwrap, 1);
  return 1;
}


static int luaB_yield (lua55_State *L) {
  return lua55_yield(L, lua55_gettop(L));
}


#define COS_RUN		0
#define COS_DEAD	1
#define COS_YIELD	2
#define COS_NORM	3


static const char *const statname[] =
  {"running", "dead", "suspended", "normal"};


static int auxstatus (lua55_State *L, lua55_State *co) {
  if (L == co) return COS_RUN;
  else {
    switch (lua55_status(co)) {
      case LUA_YIELD:
        return COS_YIELD;
      case LUA_OK: {
        lua_Debug ar;
        if (lua55_getstack(co, 0, &ar))  /* does it have frames? */
          return COS_NORM;  /* it is running */
        else if (lua55_gettop(co) == 0)
            return COS_DEAD;
        else
          return COS_YIELD;  /* initial state */
      }
      default:  /* some error occurred */
        return COS_DEAD;
    }
  }
}


static int luaB_costatus (lua55_State *L) {
  lua55_State *co = getco(L);
  lua55_pushstring(L, statname[auxstatus(L, co)]);
  return 1;
}


static lua55_State *getoptco (lua55_State *L) {
  return (lua55_isnone(L, 1) ? L : getco(L));
}


static int luaB_yieldable (lua55_State *L) {
  lua55_State *co = getoptco(L);
  lua55_pushboolean(L, lua55_isyieldable(co));
  return 1;
}


static int luaB_corunning (lua55_State *L) {
  int ismain = lua55_pushthread(L);
  lua55_pushboolean(L, ismain);
  return 2;
}


static int luaB_close (lua55_State *L) {
  lua55_State *co = getoptco(L);
  int status = auxstatus(L, co);
  switch (status) {
    case COS_DEAD: case COS_YIELD: {
      status = lua55_closethread(co, L);
      if (status == LUA_OK) {
        lua55_pushboolean(L, 1);
        return 1;
      }
      else {
        lua55_pushboolean(L, 0);
        lua55_xmove(co, L, 1);  /* move error message */
        return 2;
      }
    }
    case COS_NORM:
      return lua55L_error(L, "cannot close a %s coroutine", statname[status]);
    case COS_RUN:
      lua55_geti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);  /* get main */
      if (lua55_tothread(L, -1) == co)
        return lua55L_error(L, "cannot close main thread");
      lua55_closethread(co, L);  /* close itself */
      /* previous call does not return *//* FALLTHROUGH */
    default:
      lua_assert(0);
      return 0;
  }
}


static const luaL_Reg co_funcs[] = {
  {"create", luaB_cocreate},
  {"resume", luaB_coresume},
  {"running", luaB_corunning},
  {"status", luaB_costatus},
  {"wrap", luaB_cowrap},
  {"yield", luaB_yield},
  {"isyieldable", luaB_yieldable},
  {"close", luaB_close},
  {NULL, NULL}
};



LUAMOD_API int lua55open_coroutine (lua55_State *L) {
  lua55L_newlib(L, co_funcs);
  return 1;
}

