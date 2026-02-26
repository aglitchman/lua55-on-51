#include "lua.h"

/* function from lib1.c */
LUAMOD_API int lib1_export (lua55_State *L);

LUAMOD_API int luaopen_lib11 (lua55_State *L) {
  return lib1_export(L);
}


