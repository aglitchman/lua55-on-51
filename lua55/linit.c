/*
** $Id: linit.c $
** Initialization of libraries for lua.c and other clients
** See Copyright Notice in lua.h
*/


#define linit_c
#define LUA_LIB


#include "lprefix.h"


#include <stddef.h>

#include "lua.h"

#include "lualib.h"
#include "lauxlib.h"
#include "llimits.h"


/*
** Standard Libraries. (Must be listed in the same ORDER of their
** respective constants LUA_<libname>K.)
*/
static const luaL_Reg stdlibs[] = {
  {LUA_GNAME, lua55open_base},
  {LUA_LOADLIBNAME, lua55open_package},
  {LUA_COLIBNAME, lua55open_coroutine},
  {LUA_DBLIBNAME, lua55open_debug},
  {LUA_IOLIBNAME, lua55open_io},
  {LUA_MATHLIBNAME, lua55open_math},
  {LUA_OSLIBNAME, lua55open_os},
  {LUA_STRLIBNAME, lua55open_string},
  {LUA_TABLIBNAME, lua55open_table},
  {LUA_UTF8LIBNAME, lua55open_utf8},
  {NULL, NULL}
};


/*
** require and preload selected standard libraries
*/
LUALIB_API void lua55L_openselectedlibs (lua55_State *L, int load, int preload) {
  int mask;
  const luaL_Reg *lib;
  lua55L_getsubtable(L, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);
  for (lib = stdlibs, mask = 1; lib->name != NULL; lib++, mask <<= 1) {
    if (load & mask) {  /* selected? */
      lua55L_requiref(L, lib->name, lib->func, 1);  /* require library */
      lua55_pop(L, 1);  /* remove result from the stack */
    }
    else if (preload & mask) {  /* selected? */
      lua55_pushcfunction(L, lib->func);
      lua55_setfield(L, -2, lib->name);  /* add library to PRELOAD table */
    }
  }
  lua_assert((mask >> 1) == LUA_UTF8LIBK);
  lua55_pop(L, 1);  /* remove PRELOAD table */
}

