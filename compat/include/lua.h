/*
** Thin wrapper around Lua 5.1's lua.h that patches type constants
** to match Luau's values (Luau inserts LUA_TVECTOR=4, shifting
** STRING..THREAD by +1).
**
** All other API declarations come directly from Lua 5.1's headers.
*/

#include "../../lua51/src/lua.h"

// Patch type constants: Luau has TVECTOR=4 between NUMBER and STRING
#undef LUA_TSTRING
#undef LUA_TTABLE
#undef LUA_TFUNCTION
#undef LUA_TUSERDATA
#undef LUA_TTHREAD

#define LUA_TSTRING     5
#define LUA_TTABLE      6
#define LUA_TFUNCTION   7
#define LUA_TUSERDATA   8
#define LUA_TTHREAD     9
