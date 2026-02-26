#include "lua.h"


int luaopen_lib2 (lua55_State *L);

LUAMOD_API int luaopen_lib21 (lua55_State *L) {
  return luaopen_lib2(L);
}


