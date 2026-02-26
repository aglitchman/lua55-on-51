/*
** $Id: lauxlib.c $
** Auxiliary functions for building Lua libraries
** See Copyright Notice in lua.h
*/

#define lauxlib_c
#define LUA_LIB

#include "lprefix.h"


#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/*
** This file uses only the official API of Lua.
** Any function declared here could be written as an application function.
*/

#include "lua.h"

#include "lauxlib.h"
#include "llimits.h"


/*
** {======================================================
** Traceback
** =======================================================
*/


#define LEVELS1	10	/* size of the first part of the stack */
#define LEVELS2	11	/* size of the second part of the stack */



/*
** Search for 'objidx' in table at index -1. ('objidx' must be an
** absolute index.) Return 1 + string at top if it found a good name.
*/
static int findfield (lua_State *L, int objidx, int level) {
  if (level == 0 || !lua55_istable(L, -1))
    return 0;  /* not found */
  lua55_pushnil(L);  /* start 'next' loop */
  while (lua55_next(L, -2)) {  /* for each pair in table */
    if (lua55_type(L, -2) == LUA_TSTRING) {  /* ignore non-string keys */
      if (lua55_rawequal(L, objidx, -1)) {  /* found object? */
        lua55_pop(L, 1);  /* remove value (but keep name) */
        return 1;
      }
      else if (findfield(L, objidx, level - 1)) {  /* try recursively */
        /* stack: lib_name, lib_table, field_name (top) */
        lua55_pushliteral(L, ".");  /* place '.' between the two names */
        lua55_replace(L, -3);  /* (in the slot occupied by table) */
        lua55_concat(L, 3);  /* lib_name.field_name */
        return 1;
      }
    }
    lua55_pop(L, 1);  /* remove value */
  }
  return 0;  /* not found */
}


/*
** Search for a name for a function in all loaded modules
*/
static int pushglobalfuncname (lua_State *L, lua_Debug *ar) {
  int top = lua55_gettop(L);
  lua55_getinfo(L, "f", ar);  /* push function */
  lua55_getfield(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
  lua55L_checkstack(L, 6, "not enough stack");  /* slots for 'findfield' */
  if (findfield(L, top + 1, 2)) {
    const char *name = lua55_tostring(L, -1);
    if (strncmp(name, LUA_GNAME ".", 3) == 0) {  /* name start with '_G.'? */
      lua55_pushstring(L, name + 3);  /* push name without prefix */
      lua55_remove(L, -2);  /* remove original name */
    }
    lua55_copy(L, -1, top + 1);  /* copy name to proper place */
    lua55_settop(L, top + 1);  /* remove table "loaded" and name copy */
    return 1;
  }
  else {
    lua55_settop(L, top);  /* remove function and global table */
    return 0;
  }
}


static void pushfuncname (lua_State *L, lua_Debug *ar) {
  if (*ar->namewhat != '\0')  /* is there a name from code? */
    lua55_pushfstring(L, "%s '%s'", ar->namewhat, ar->name);  /* use it */
  else if (*ar->what == 'm')  /* main? */
      lua55_pushliteral(L, "main chunk");
  else if (pushglobalfuncname(L, ar)) {  /* try a global name */
    lua55_pushfstring(L, "function '%s'", lua55_tostring(L, -1));
    lua55_remove(L, -2);  /* remove name */
  }
  else if (*ar->what != 'C')  /* for Lua functions, use <file:line> */
    lua55_pushfstring(L, "function <%s:%d>", ar->short_src, ar->linedefined);
  else  /* nothing left... */
    lua55_pushliteral(L, "?");
}


static int lastlevel (lua_State *L) {
  lua_Debug ar;
  int li = 1, le = 1;
  /* find an upper bound */
  while (lua55_getstack(L, le, &ar)) { li = le; le *= 2; }
  /* do a binary search */
  while (li < le) {
    int m = (li + le)/2;
    if (lua55_getstack(L, m, &ar)) li = m + 1;
    else le = m;
  }
  return le - 1;
}


LUALIB_API void lua55L_traceback (lua_State *L, lua_State *L1,
                                const char *msg, int level) {
  luaL_Buffer b;
  lua_Debug ar;
  int last = lastlevel(L1);
  int limit2show = (last - level > LEVELS1 + LEVELS2) ? LEVELS1 : -1;
  lua55L_buffinit(L, &b);
  if (msg) {
    lua55L_addstring(&b, msg);
    lua55L_addchar(&b, '\n');
  }
  lua55L_addstring(&b, "stack traceback:");
  while (lua55_getstack(L1, level++, &ar)) {
    if (limit2show-- == 0) {  /* too many levels? */
      int n = last - level - LEVELS2 + 1;  /* number of levels to skip */
      lua55_pushfstring(L, "\n\t...\t(skipping %d levels)", n);
      lua55L_addvalue(&b);  /* add warning about skip */
      level += n;  /* and skip to last levels */
    }
    else {
      lua55_getinfo(L1, "Slnt", &ar);
      if (ar.currentline <= 0)
        lua55_pushfstring(L, "\n\t%s: in ", ar.short_src);
      else
        lua55_pushfstring(L, "\n\t%s:%d: in ", ar.short_src, ar.currentline);
      lua55L_addvalue(&b);
      pushfuncname(L, &ar);
      lua55L_addvalue(&b);
      if (ar.istailcall)
        lua55L_addstring(&b, "\n\t(...tail calls...)");
    }
  }
  lua55L_pushresult(&b);
}

/* }====================================================== */


/*
** {======================================================
** Error-report functions
** =======================================================
*/

LUALIB_API int lua55L_argerror (lua_State *L, int arg, const char *extramsg) {
  lua_Debug ar;
  const char *argword;
  if (!lua55_getstack(L, 0, &ar))  /* no stack frame? */
    return lua55L_error(L, "bad argument #%d (%s)", arg, extramsg);
  lua55_getinfo(L, "nt", &ar);
  if (arg <= ar.extraargs)  /* error in an extra argument? */
    argword =  "extra argument";
  else {
    arg -= ar.extraargs;  /* do not count extra arguments */
    if (strcmp(ar.namewhat, "method") == 0) {  /* colon syntax? */
      arg--;  /* do not count (extra) self argument */
      if (arg == 0)  /* error in self argument? */
        return lua55L_error(L, "calling '%s' on bad self (%s)",
                               ar.name, extramsg);
      /* else go through; error in a regular argument */
    }
    argword = "argument";
  }
  if (ar.name == NULL)
    ar.name = (pushglobalfuncname(L, &ar)) ? lua55_tostring(L, -1) : "?";
  return lua55L_error(L, "bad %s #%d to '%s' (%s)",
                       argword, arg, ar.name, extramsg);
}


LUALIB_API int lua55L_typeerror (lua_State *L, int arg, const char *tname) {
  const char *msg;
  const char *typearg;  /* name for the type of the actual argument */
  if (lua55L_getmetafield(L, arg, "__name") == LUA_TSTRING)
    typearg = lua55_tostring(L, -1);  /* use the given type name */
  else if (lua55_type(L, arg) == LUA_TLIGHTUSERDATA)
    typearg = "light userdata";  /* special name for messages */
  else
    typearg = lua55L_typename(L, arg);  /* standard name */
  msg = lua55_pushfstring(L, "%s expected, got %s", tname, typearg);
  return lua55L_argerror(L, arg, msg);
}


static void tag_error (lua_State *L, int arg, int tag) {
  lua55L_typeerror(L, arg, lua55_typename(L, tag));
}


/*
** The use of 'lua55_pushfstring' ensures this function does not
** need reserved stack space when called.
*/
LUALIB_API void lua55L_where (lua_State *L, int level) {
  lua_Debug ar;
  if (lua55_getstack(L, level, &ar)) {  /* check function at level */
    lua55_getinfo(L, "Sl", &ar);  /* get info about it */
    if (ar.currentline > 0) {  /* is there info? */
      lua55_pushfstring(L, "%s:%d: ", ar.short_src, ar.currentline);
      return;
    }
  }
  lua55_pushfstring(L, "");  /* else, no information available... */
}


/*
** Again, the use of 'lua55_pushvfstring' ensures this function does
** not need reserved stack space when called. (At worst, it generates
** a memory error instead of the given message.)
*/
LUALIB_API int lua55L_error (lua_State *L, const char *fmt, ...) {
  va_list argp;
  va_start(argp, fmt);
  lua55L_where(L, 1);
  lua55_pushvfstring(L, fmt, argp);
  va_end(argp);
  lua55_concat(L, 2);
  return lua55_error(L);
}


LUALIB_API int lua55L_fileresult (lua_State *L, int stat, const char *fname) {
  int en = errno;  /* calls to Lua API may change this value */
  if (stat) {
    lua55_pushboolean(L, 1);
    return 1;
  }
  else {
    const char *msg;
    lua55L_pushfail(L);
    msg = (en != 0) ? strerror(en) : "(no extra info)";
    if (fname)
      lua55_pushfstring(L, "%s: %s", fname, msg);
    else
      lua55_pushstring(L, msg);
    lua55_pushinteger(L, en);
    return 3;
  }
}


#if !defined(l_inspectstat)	/* { */

#if defined(LUA_USE_POSIX)

#include <sys/wait.h>

/*
** use appropriate macros to interpret 'pclose' return status
*/
#define l_inspectstat(stat,what)  \
   if (WIFEXITED(stat)) { stat = WEXITSTATUS(stat); } \
   else if (WIFSIGNALED(stat)) { stat = WTERMSIG(stat); what = "signal"; }

#else

#define l_inspectstat(stat,what)  /* no op */

#endif

#endif				/* } */


LUALIB_API int lua55L_execresult (lua_State *L, int stat) {
  if (stat != 0 && errno != 0)  /* error with an 'errno'? */
    return lua55L_fileresult(L, 0, NULL);
  else {
    const char *what = "exit";  /* type of termination */
    l_inspectstat(stat, what);  /* interpret result */
    if (*what == 'e' && stat == 0)  /* successful termination? */
      lua55_pushboolean(L, 1);
    else
      lua55L_pushfail(L);
    lua55_pushstring(L, what);
    lua55_pushinteger(L, stat);
    return 3;  /* return true/fail,what,code */
  }
}

/* }====================================================== */



/*
** {======================================================
** Userdata's metatable manipulation
** =======================================================
*/

LUALIB_API int lua55L_newmetatable (lua_State *L, const char *tname) {
  if (lua55L_getmetatable(L, tname) != LUA_TNIL)  /* name already in use? */
    return 0;  /* leave previous value on top, but return 0 */
  lua55_pop(L, 1);
  lua55_createtable(L, 0, 2);  /* create metatable */
  lua55_pushstring(L, tname);
  lua55_setfield(L, -2, "__name");  /* metatable.__name = tname */
  lua55_pushvalue(L, -1);
  lua55_setfield(L, LUA_REGISTRYINDEX, tname);  /* registry.name = metatable */
  return 1;
}


LUALIB_API void lua55L_setmetatable (lua_State *L, const char *tname) {
  lua55L_getmetatable(L, tname);
  lua55_setmetatable(L, -2);
}


LUALIB_API void *lua55L_testudata (lua_State *L, int ud, const char *tname) {
  void *p = lua55_touserdata(L, ud);
  if (p != NULL) {  /* value is a userdata? */
    if (lua55_getmetatable(L, ud)) {  /* does it have a metatable? */
      lua55L_getmetatable(L, tname);  /* get correct metatable */
      if (!lua55_rawequal(L, -1, -2))  /* not the same? */
        p = NULL;  /* value is a userdata with wrong metatable */
      lua55_pop(L, 2);  /* remove both metatables */
      return p;
    }
  }
  return NULL;  /* value is not a userdata with a metatable */
}


LUALIB_API void *lua55L_checkudata (lua_State *L, int ud, const char *tname) {
  void *p = lua55L_testudata(L, ud, tname);
  lua55L_argexpected(L, p != NULL, ud, tname);
  return p;
}

/* }====================================================== */


/*
** {======================================================
** Argument check functions
** =======================================================
*/

LUALIB_API int lua55L_checkoption (lua_State *L, int arg, const char *def,
                                 const char *const lst[]) {
  const char *name = (def) ? lua55L_optstring(L, arg, def) :
                             lua55L_checkstring(L, arg);
  int i;
  for (i=0; lst[i]; i++)
    if (strcmp(lst[i], name) == 0)
      return i;
  return lua55L_argerror(L, arg,
                       lua55_pushfstring(L, "invalid option '%s'", name));
}


/*
** Ensures the stack has at least 'space' extra slots, raising an error
** if it cannot fulfill the request. (The error handling needs a few
** extra slots to format the error message. In case of an error without
** this extra space, Lua will generate the same 'stack overflow' error,
** but without 'msg'.)
*/
LUALIB_API void lua55L_checkstack (lua_State *L, int space, const char *msg) {
  if (l_unlikely(!lua55_checkstack(L, space))) {
    if (msg)
      lua55L_error(L, "stack overflow (%s)", msg);
    else
      lua55L_error(L, "stack overflow");
  }
}


LUALIB_API void lua55L_checktype (lua_State *L, int arg, int t) {
  if (l_unlikely(lua55_type(L, arg) != t))
    tag_error(L, arg, t);
}


LUALIB_API void lua55L_checkany (lua_State *L, int arg) {
  if (l_unlikely(lua55_type(L, arg) == LUA_TNONE))
    lua55L_argerror(L, arg, "value expected");
}


LUALIB_API const char *lua55L_checklstring (lua_State *L, int arg, size_t *len) {
  const char *s = lua55_tolstring(L, arg, len);
  if (l_unlikely(!s)) tag_error(L, arg, LUA_TSTRING);
  return s;
}


LUALIB_API const char *lua55L_optlstring (lua_State *L, int arg,
                                        const char *def, size_t *len) {
  if (lua55_isnoneornil(L, arg)) {
    if (len)
      *len = (def ? strlen(def) : 0);
    return def;
  }
  else return lua55L_checklstring(L, arg, len);
}


LUALIB_API lua_Number lua55L_checknumber (lua_State *L, int arg) {
  int isnum;
  lua_Number d = lua55_tonumberx(L, arg, &isnum);
  if (l_unlikely(!isnum))
    tag_error(L, arg, LUA_TNUMBER);
  return d;
}


LUALIB_API lua_Number lua55L_optnumber (lua_State *L, int arg, lua_Number def) {
  return lua55L_opt(L, lua55L_checknumber, arg, def);
}


static void interror (lua_State *L, int arg) {
  if (lua55_isnumber(L, arg))
    lua55L_argerror(L, arg, "number has no integer representation");
  else
    tag_error(L, arg, LUA_TNUMBER);
}


LUALIB_API lua_Integer lua55L_checkinteger (lua_State *L, int arg) {
  int isnum;
  lua_Integer d = lua55_tointegerx(L, arg, &isnum);
  if (l_unlikely(!isnum)) {
    interror(L, arg);
  }
  return d;
}


LUALIB_API lua_Integer lua55L_optinteger (lua_State *L, int arg,
                                                      lua_Integer def) {
  return lua55L_opt(L, lua55L_checkinteger, arg, def);
}

/* }====================================================== */


/*
** {======================================================
** Generic Buffer manipulation
** =======================================================
*/

/* userdata to box arbitrary data */
typedef struct UBox {
  void *box;
  size_t bsize;
} UBox;


/* Resize the buffer used by a box. Optimize for the common case of
** resizing to the old size. (For instance, __gc will resize the box
** to 0 even after it was closed. 'pushresult' may also resize it to a
** final size that is equal to the one set when the buffer was created.)
*/
static void *resizebox (lua_State *L, int idx, size_t newsize) {
  UBox *box = (UBox *)lua55_touserdata(L, idx);
  if (box->bsize == newsize)  /* not changing size? */
    return box->box;  /* keep the buffer */
  else {
    void *ud;
    lua_Alloc allocf = lua55_getallocf(L, &ud);
    void *temp = allocf(ud, box->box, box->bsize, newsize);
    if (l_unlikely(temp == NULL && newsize > 0)) {  /* allocation error? */
      lua55_pushliteral(L, "not enough memory");
      lua55_error(L);  /* raise a memory error */
    }
    box->box = temp;
    box->bsize = newsize;
    return temp;
  }
}


static int boxgc (lua_State *L) {
  resizebox(L, 1, 0);
  return 0;
}


static const luaL_Reg boxmt[] = {  /* box metamethods */
  {"__gc", boxgc},
  {"__close", boxgc},
  {NULL, NULL}
};


static void newbox (lua_State *L) {
  UBox *box = (UBox *)lua55_newuserdatauv(L, sizeof(UBox), 0);
  box->box = NULL;
  box->bsize = 0;
  if (lua55L_newmetatable(L, "_UBOX*"))  /* creating metatable? */
    lua55L_setfuncs(L, boxmt, 0);  /* set its metamethods */
  lua55_setmetatable(L, -2);
}


/*
** check whether buffer is using a userdata on the stack as a temporary
** buffer
*/
#define buffonstack(B)	((B)->b != (B)->init.b)


/*
** Whenever buffer is accessed, slot 'idx' must either be a box (which
** cannot be NULL) or it is a placeholder for the buffer.
*/
#define checkbufferlevel(B,idx)  \
  lua_assert(buffonstack(B) ? lua55_touserdata(B->L, idx) != NULL  \
                            : lua55_touserdata(B->L, idx) == (void*)B)


/*
** Compute new size for buffer 'B', enough to accommodate extra 'sz'
** bytes plus one for a terminating zero.
*/
static size_t newbuffsize (luaL_Buffer *B, size_t sz) {
  size_t newsize = B->size;
  if (l_unlikely(sz >= MAX_SIZE - B->n))
    return cast_sizet(lua55L_error(B->L, "resulting string too large"));
  /* else  B->n + sz + 1 <= MAX_SIZE */
  if (newsize <= MAX_SIZE/3 * 2)  /* no overflow? */
    newsize += (newsize >> 1);  /* new size *= 1.5 */
  if (newsize < B->n + sz + 1)  /* not big enough? */
    newsize = B->n + sz + 1;
  return newsize;
}


/*
** Returns a pointer to a free area with at least 'sz' bytes in buffer
** 'B'. 'boxidx' is the relative position in the stack where is the
** buffer's box or its placeholder.
*/
static char *prepbuffsize (luaL_Buffer *B, size_t sz, int boxidx) {
  checkbufferlevel(B, boxidx);
  if (B->size - B->n >= sz)  /* enough space? */
    return B->b + B->n;
  else {
    lua_State *L = B->L;
    char *newbuff;
    size_t newsize = newbuffsize(B, sz);
    /* create larger buffer */
    if (buffonstack(B))  /* buffer already has a box? */
      newbuff = (char *)resizebox(L, boxidx, newsize);  /* resize it */
    else {  /* no box yet */
      lua55_remove(L, boxidx);  /* remove placeholder */
      newbox(L);  /* create a new box */
      lua55_insert(L, boxidx);  /* move box to its intended position */
      lua55_toclose(L, boxidx);
      newbuff = (char *)resizebox(L, boxidx, newsize);
      memcpy(newbuff, B->b, B->n * sizeof(char));  /* copy original content */
    }
    B->b = newbuff;
    B->size = newsize;
    return newbuff + B->n;
  }
}

/*
** returns a pointer to a free area with at least 'sz' bytes
*/
LUALIB_API char *lua55L_prepbuffsize (luaL_Buffer *B, size_t sz) {
  return prepbuffsize(B, sz, -1);
}


LUALIB_API void lua55L_addlstring (luaL_Buffer *B, const char *s, size_t l) {
  if (l > 0) {  /* avoid 'memcpy' when 's' can be NULL */
    char *b = prepbuffsize(B, l, -1);
    memcpy(b, s, l * sizeof(char));
    lua55L_addsize(B, l);
  }
}


LUALIB_API void lua55L_addstring (luaL_Buffer *B, const char *s) {
  lua55L_addlstring(B, s, strlen(s));
}


LUALIB_API void lua55L_pushresult (luaL_Buffer *B) {
  lua_State *L = B->L;
  checkbufferlevel(B, -1);
  if (!buffonstack(B))  /* using static buffer? */
    lua55_pushlstring(L, B->b, B->n);  /* save result as regular string */
  else {  /* reuse buffer already allocated */
    UBox *box = (UBox *)lua55_touserdata(L, -1);
    void *ud;
    lua_Alloc allocf = lua55_getallocf(L, &ud);  /* function to free buffer */
    size_t len = B->n;  /* final string length */
    char *s;
    resizebox(L, -1, len + 1);  /* adjust box size to content size */
    s = (char*)box->box;  /* final buffer address */
    s[len] = '\0';  /* add ending zero */
    /* clear box, as Lua will take control of the buffer */
    box->bsize = 0;  box->box = NULL;
    lua55_pushexternalstring(L, s, len, allocf, ud);
    lua55_closeslot(L, -2);  /* close the box */
    lua55_gc(L, LUA_GCSTEP, len);
  }
  lua55_remove(L, -2);  /* remove box or placeholder from the stack */
}


LUALIB_API void lua55L_pushresultsize (luaL_Buffer *B, size_t sz) {
  lua55L_addsize(B, sz);
  lua55L_pushresult(B);
}


/*
** 'lua55L_addvalue' is the only function in the Buffer system where the
** box (if existent) is not on the top of the stack. So, instead of
** calling 'lua55L_addlstring', it replicates the code using -2 as the
** last argument to 'prepbuffsize', signaling that the box is (or will
** be) below the string being added to the buffer. (Box creation can
** trigger an emergency GC, so we should not remove the string from the
** stack before we have the space guaranteed.)
*/
LUALIB_API void lua55L_addvalue (luaL_Buffer *B) {
  lua_State *L = B->L;
  size_t len;
  const char *s = lua55_tolstring(L, -1, &len);
  char *b = prepbuffsize(B, len, -2);
  memcpy(b, s, len * sizeof(char));
  lua55L_addsize(B, len);
  lua55_pop(L, 1);  /* pop string */
}


LUALIB_API void lua55L_buffinit (lua_State *L, luaL_Buffer *B) {
  B->L = L;
  B->b = B->init.b;
  B->n = 0;
  B->size = LUAL_BUFFERSIZE;
  lua55_pushlightuserdata(L, (void*)B);  /* push placeholder */
}


LUALIB_API char *lua55L_buffinitsize (lua_State *L, luaL_Buffer *B, size_t sz) {
  lua55L_buffinit(L, B);
  return prepbuffsize(B, sz, -1);
}

/* }====================================================== */


/*
** {======================================================
** Reference system
** =======================================================
*/

/*
** The previously freed references form a linked list: t[1] is the index
** of a first free index, t[t[1]] is the index of the second element,
** etc. A zero signals the end of the list.
*/
LUALIB_API int lua55L_ref (lua_State *L, int t) {
  int ref;
  if (lua55_isnil(L, -1)) {
    lua55_pop(L, 1);  /* remove from stack */
    return LUA_REFNIL;  /* 'nil' has a unique fixed reference */
  }
  t = lua55_absindex(L, t);
  if (lua55_rawgeti(L, t, 1) == LUA_TNUMBER)  /* already initialized? */
    ref = (int)lua55_tointeger(L, -1);  /* ref = t[1] */
  else {  /* first access */
    lua_assert(!lua55_toboolean(L, -1));  /* must be nil or false */
    ref = 0;  /* list is empty */
    lua55_pushinteger(L, 0);  /* initialize as an empty list */
    lua55_rawseti(L, t, 1);  /* ref = t[1] = 0 */
  }
  lua55_pop(L, 1);  /* remove element from stack */
  if (ref != 0) {  /* any free element? */
    lua55_rawgeti(L, t, ref);  /* remove it from list */
    lua55_rawseti(L, t, 1);  /* (t[1] = t[ref]) */
  }
  else  /* no free elements */
    ref = (int)lua55_rawlen(L, t) + 1;  /* get a new reference */
  lua55_rawseti(L, t, ref);
  return ref;
}


LUALIB_API void lua55L_unref (lua_State *L, int t, int ref) {
  if (ref >= 0) {
    t = lua55_absindex(L, t);
    lua55_rawgeti(L, t, 1);
    lua_assert(lua55_isinteger(L, -1));
    lua55_rawseti(L, t, ref);  /* t[ref] = t[1] */
    lua55_pushinteger(L, ref);
    lua55_rawseti(L, t, 1);  /* t[1] = ref */
  }
}

/* }====================================================== */


/*
** {======================================================
** Load functions
** =======================================================
*/

typedef struct LoadF {
  unsigned n;  /* number of pre-read characters */
  FILE *f;  /* file being read */
  char buff[BUFSIZ];  /* area for reading file */
} LoadF;


static const char *getF (lua_State *L, void *ud, size_t *size) {
  LoadF *lf = (LoadF *)ud;
  UNUSED(L);
  if (lf->n > 0) {  /* are there pre-read characters to be read? */
    *size = lf->n;  /* return them (chars already in buffer) */
    lf->n = 0;  /* no more pre-read characters */
  }
  else {  /* read a block from file */
    /* 'fread' can return > 0 *and* set the EOF flag. If next call to
       'getF' called 'fread', it might still wait for user input.
       The next check avoids this problem. */
    if (feof(lf->f)) return NULL;
    *size = fread(lf->buff, 1, sizeof(lf->buff), lf->f);  /* read block */
  }
  return lf->buff;
}


static int errfile (lua_State *L, const char *what, int fnameindex) {
  int err = errno;
  const char *filename = lua55_tostring(L, fnameindex) + 1;
  if (err != 0)
    lua55_pushfstring(L, "cannot %s %s: %s", what, filename, strerror(err));
  else
    lua55_pushfstring(L, "cannot %s %s", what, filename);
  lua55_remove(L, fnameindex);
  return LUA_ERRFILE;
}


/*
** Skip an optional BOM at the start of a stream. If there is an
** incomplete BOM (the first character is correct but the rest is
** not), returns the first character anyway to force an error
** (as no chunk can start with 0xEF).
*/
static int skipBOM (FILE *f) {
  int c = getc(f);  /* read first character */
  if (c == 0xEF && getc(f) == 0xBB && getc(f) == 0xBF)  /* correct BOM? */
    return getc(f);  /* ignore BOM and return next char */
  else  /* no (valid) BOM */
    return c;  /* return first character */
}


/*
** reads the first character of file 'f' and skips an optional BOM mark
** in its beginning plus its first line if it starts with '#'. Returns
** true if it skipped the first line.  In any case, '*cp' has the
** first "valid" character of the file (after the optional BOM and
** a first-line comment).
*/
static int skipcomment (FILE *f, int *cp) {
  int c = *cp = skipBOM(f);
  if (c == '#') {  /* first line is a comment (Unix exec. file)? */
    do {  /* skip first line */
      c = getc(f);
    } while (c != EOF && c != '\n');
    *cp = getc(f);  /* next character after comment, if present */
    return 1;  /* there was a comment */
  }
  else return 0;  /* no comment */
}


LUALIB_API int lua55L_loadfilex (lua_State *L, const char *filename,
                                             const char *mode) {
  LoadF lf;
  int status, readstatus;
  int c;
  int fnameindex = lua55_gettop(L) + 1;  /* index of filename on the stack */
  if (filename == NULL) {
    lua55_pushliteral(L, "=stdin");
    lf.f = stdin;
  }
  else {
    lua55_pushfstring(L, "@%s", filename);
    errno = 0;
    lf.f = fopen(filename, "r");
    if (lf.f == NULL) return errfile(L, "open", fnameindex);
  }
  lf.n = 0;
  if (skipcomment(lf.f, &c))  /* read initial portion */
    lf.buff[lf.n++] = '\n';  /* add newline to correct line numbers */
  if (c == LUA_SIGNATURE[0]) {  /* binary file? */
    lf.n = 0;  /* remove possible newline */
    if (filename) {  /* "real" file? */
      errno = 0;
      lf.f = freopen(filename, "rb", lf.f);  /* reopen in binary mode */
      if (lf.f == NULL) return errfile(L, "reopen", fnameindex);
      skipcomment(lf.f, &c);  /* re-read initial portion */
    }
  }
  if (c != EOF)
    lf.buff[lf.n++] = cast_char(c);  /* 'c' is the first character */
  status = lua55_load(L, getF, &lf, lua55_tostring(L, -1), mode);
  readstatus = ferror(lf.f);
  errno = 0;  /* no useful error number until here */
  if (filename) fclose(lf.f);  /* close file (even in case of errors) */
  if (readstatus) {
    lua55_settop(L, fnameindex);  /* ignore results from 'lua55_load' */
    return errfile(L, "read", fnameindex);
  }
  lua55_remove(L, fnameindex);
  return status;
}


typedef struct LoadS {
  const char *s;
  size_t size;
} LoadS;


static const char *getS (lua_State *L, void *ud, size_t *size) {
  LoadS *ls = (LoadS *)ud;
  UNUSED(L);
  if (ls->size == 0) return NULL;
  *size = ls->size;
  ls->size = 0;
  return ls->s;
}


LUALIB_API int lua55L_loadbufferx (lua_State *L, const char *buff, size_t size,
                                 const char *name, const char *mode) {
  LoadS ls;
  ls.s = buff;
  ls.size = size;
  return lua55_load(L, getS, &ls, name, mode);
}


LUALIB_API int lua55L_loadstring (lua_State *L, const char *s) {
  return lua55L_loadbuffer(L, s, strlen(s), s);
}

/* }====================================================== */



LUALIB_API int lua55L_getmetafield (lua_State *L, int obj, const char *event) {
  if (!lua55_getmetatable(L, obj))  /* no metatable? */
    return LUA_TNIL;
  else {
    int tt;
    lua55_pushstring(L, event);
    tt = lua55_rawget(L, -2);
    if (tt == LUA_TNIL)  /* is metafield nil? */
      lua55_pop(L, 2);  /* remove metatable and metafield */
    else
      lua55_remove(L, -2);  /* remove only metatable */
    return tt;  /* return metafield type */
  }
}


LUALIB_API int lua55L_callmeta (lua_State *L, int obj, const char *event) {
  obj = lua55_absindex(L, obj);
  if (lua55L_getmetafield(L, obj, event) == LUA_TNIL)  /* no metafield? */
    return 0;
  lua55_pushvalue(L, obj);
  lua55_call(L, 1, 1);
  return 1;
}


LUALIB_API lua_Integer lua55L_len (lua_State *L, int idx) {
  lua_Integer l;
  int isnum;
  lua55_len(L, idx);
  l = lua55_tointegerx(L, -1, &isnum);
  if (l_unlikely(!isnum))
    lua55L_error(L, "object length is not an integer");
  lua55_pop(L, 1);  /* remove object */
  return l;
}


LUALIB_API const char *lua55L_tolstring (lua_State *L, int idx, size_t *len) {
  idx = lua55_absindex(L,idx);
  if (lua55L_callmeta(L, idx, "__tostring")) {  /* metafield? */
    if (!lua55_isstring(L, -1))
      lua55L_error(L, "'__tostring' must return a string");
  }
  else {
    switch (lua55_type(L, idx)) {
      case LUA_TNUMBER: {
        char buff[LUA_N2SBUFFSZ];
        lua55_numbertocstring(L, idx, buff);
        lua55_pushstring(L, buff);
        break;
      }
      case LUA_TSTRING:
        lua55_pushvalue(L, idx);
        break;
      case LUA_TBOOLEAN:
        lua55_pushstring(L, (lua55_toboolean(L, idx) ? "true" : "false"));
        break;
      case LUA_TNIL:
        lua55_pushliteral(L, "nil");
        break;
      default: {
        int tt = lua55L_getmetafield(L, idx, "__name");  /* try name */
        const char *kind = (tt == LUA_TSTRING) ? lua55_tostring(L, -1) :
                                                 lua55L_typename(L, idx);
        lua55_pushfstring(L, "%s: %p", kind, lua55_topointer(L, idx));
        if (tt != LUA_TNIL)
          lua55_remove(L, -2);  /* remove '__name' */
        break;
      }
    }
  }
  return lua55_tolstring(L, -1, len);
}


/*
** set functions from list 'l' into table at top - 'nup'; each
** function gets the 'nup' elements at the top as upvalues.
** Returns with only the table at the stack.
*/
LUALIB_API void lua55L_setfuncs (lua_State *L, const luaL_Reg *l, int nup) {
  lua55L_checkstack(L, nup, "too many upvalues");
  for (; l->name != NULL; l++) {  /* fill the table with given functions */
    if (l->func == NULL)  /* placeholder? */
      lua55_pushboolean(L, 0);
    else {
      int i;
      for (i = 0; i < nup; i++)  /* copy upvalues to the top */
        lua55_pushvalue(L, -nup);
      lua55_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
    }
    lua55_setfield(L, -(nup + 2), l->name);
  }
  lua55_pop(L, nup);  /* remove upvalues */
}


/*
** ensure that stack[idx][fname] has a table and push that table
** into the stack
*/
LUALIB_API int lua55L_getsubtable (lua_State *L, int idx, const char *fname) {
  if (lua55_getfield(L, idx, fname) == LUA_TTABLE)
    return 1;  /* table already there */
  else {
    lua55_pop(L, 1);  /* remove previous result */
    idx = lua55_absindex(L, idx);
    lua55_newtable(L);
    lua55_pushvalue(L, -1);  /* copy to be left at top */
    lua55_setfield(L, idx, fname);  /* assign new table to field */
    return 0;  /* false, because did not find table there */
  }
}


/*
** Stripped-down 'require': After checking "loaded" table, calls 'openf'
** to open a module, registers the result in 'package.loaded' table and,
** if 'glb' is true, also registers the result in the global table.
** Leaves resulting module on the top.
*/
LUALIB_API void lua55L_requiref (lua_State *L, const char *modname,
                               lua_CFunction openf, int glb) {
  lua55L_getsubtable(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
  lua55_getfield(L, -1, modname);  /* LOADED[modname] */
  if (!lua55_toboolean(L, -1)) {  /* package not already loaded? */
    lua55_pop(L, 1);  /* remove field */
    lua55_pushcfunction(L, openf);
    lua55_pushstring(L, modname);  /* argument to open function */
    lua55_call(L, 1, 1);  /* call 'openf' to open module */
    lua55_pushvalue(L, -1);  /* make copy of module (call result) */
    lua55_setfield(L, -3, modname);  /* LOADED[modname] = module */
  }
  lua55_remove(L, -2);  /* remove LOADED table */
  if (glb) {
    lua55_pushvalue(L, -1);  /* copy of module */
    lua55_setglobal(L, modname);  /* _G[modname] = module */
  }
}


LUALIB_API void lua55L_addgsub (luaL_Buffer *b, const char *s,
                                     const char *p, const char *r) {
  const char *wild;
  size_t l = strlen(p);
  while ((wild = strstr(s, p)) != NULL) {
    lua55L_addlstring(b, s, ct_diff2sz(wild - s));  /* push prefix */
    lua55L_addstring(b, r);  /* push replacement in place of pattern */
    s = wild + l;  /* continue after 'p' */
  }
  lua55L_addstring(b, s);  /* push last suffix */
}


LUALIB_API const char *lua55L_gsub (lua_State *L, const char *s,
                                  const char *p, const char *r) {
  luaL_Buffer b;
  lua55L_buffinit(L, &b);
  lua55L_addgsub(&b, s, p, r);
  lua55L_pushresult(&b);
  return lua55_tostring(L, -1);
}


void *lua55L_alloc (void *ud, void *ptr, size_t osize, size_t nsize) {
  UNUSED(ud); UNUSED(osize);
  if (nsize == 0) {
    free(ptr);
    return NULL;
  }
  else
    return realloc(ptr, nsize);
}


/*
** Standard panic function just prints an error message. The test
** with 'lua55_type' avoids possible memory errors in 'lua55_tostring'.
*/
static int panic (lua_State *L) {
  const char *msg = (lua55_type(L, -1) == LUA_TSTRING)
                  ? lua55_tostring(L, -1)
                  : "error object is not a string";
  lua_writestringerror("PANIC: unprotected error in call to Lua API (%s)\n",
                        msg);
  return 0;  /* return to Lua to abort */
}


/*
** Warning functions:
** warnfoff: warning system is off
** warnfon: ready to start a new message
** warnfcont: previous message is to be continued
*/
static void warnfoff (void *ud, const char *message, int tocont);
static void warnfon (void *ud, const char *message, int tocont);
static void warnfcont (void *ud, const char *message, int tocont);


/*
** Check whether message is a control message. If so, execute the
** control or ignore it if unknown.
*/
static int checkcontrol (lua_State *L, const char *message, int tocont) {
  if (tocont || *(message++) != '@')  /* not a control message? */
    return 0;
  else {
    if (strcmp(message, "off") == 0)
      lua55_setwarnf(L, warnfoff, L);  /* turn warnings off */
    else if (strcmp(message, "on") == 0)
      lua55_setwarnf(L, warnfon, L);   /* turn warnings on */
    return 1;  /* it was a control message */
  }
}


static void warnfoff (void *ud, const char *message, int tocont) {
  checkcontrol((lua_State *)ud, message, tocont);
}


/*
** Writes the message and handle 'tocont', finishing the message
** if needed and setting the next warn function.
*/
static void warnfcont (void *ud, const char *message, int tocont) {
  lua_State *L = (lua_State *)ud;
  lua_writestringerror("%s", message);  /* write message */
  if (tocont)  /* not the last part? */
    lua55_setwarnf(L, warnfcont, L);  /* to be continued */
  else {  /* last part */
    lua_writestringerror("%s", "\n");  /* finish message with end-of-line */
    lua55_setwarnf(L, warnfon, L);  /* next call is a new message */
  }
}


static void warnfon (void *ud, const char *message, int tocont) {
  if (checkcontrol((lua_State *)ud, message, tocont))  /* control message? */
    return;  /* nothing else to be done */
  lua_writestringerror("%s", "Lua warning: ");  /* start a new warning */
  warnfcont(ud, message, tocont);  /* finish processing */
}



/*
** A function to compute an unsigned int with some level of
** randomness. Rely on Address Space Layout Randomization (if present)
** and the current time.
*/
#if !defined(luai_makeseed)

#include <time.h>


/* Size for the buffer, in bytes */
#define BUFSEEDB	(sizeof(void*) + sizeof(time_t))

/* Size for the buffer in int's, rounded up */
#define BUFSEED		((BUFSEEDB + sizeof(int) - 1) / sizeof(int))

/*
** Copy the contents of variable 'v' into the buffer pointed by 'b'.
** (The '&b[0]' disguises 'b' to fix an absurd warning from clang.)
*/
#define addbuff(b,v)	(memcpy(&b[0], &(v), sizeof(v)), b += sizeof(v))


static unsigned int luai_makeseed (void) {
  unsigned int buff[BUFSEED];
  unsigned int res;
  unsigned int i;
  time_t t = time(NULL);
  char *b = (char*)buff;
  addbuff(b, b);  /* local variable's address */
  addbuff(b, t);  /* time */
  /* fill (rare but possible) remain of the buffer with zeros */
  memset(b, 0, sizeof(buff) - BUFSEEDB);
  res = buff[0];
  for (i = 1; i < BUFSEED; i++)
    res ^= (res >> 3) + (res << 7) + buff[i];
  return res;
}

#endif


LUALIB_API unsigned int lua55L_makeseed (lua_State *L) {
  UNUSED(L);
  return luai_makeseed();
}


/*
** Use the name with parentheses so that headers can redefine it
** as a macro.
*/
LUALIB_API lua_State *(lua55L_newstate) (void) {
  lua_State *L = lua55_newstate(lua55L_alloc, NULL, lua55L_makeseed(NULL));
  if (l_likely(L)) {
    lua55_atpanic(L, &panic);
    lua55_setwarnf(L, warnfon, L);
  }
  return L;
}


LUALIB_API void lua55L_checkversion_ (lua_State *L, lua_Number ver, size_t sz) {
  lua_Number v = lua55_version(L);
  if (sz != LUAL_NUMSIZES)  /* check numeric types */
    lua55L_error(L, "core and library have incompatible numeric types");
  else if (v != ver)
    lua55L_error(L, "version mismatch: app. needs %f, Lua core provides %f",
                  (LUAI_UACNUMBER)ver, (LUAI_UACNUMBER)v);
}

