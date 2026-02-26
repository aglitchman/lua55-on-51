/*
** $Id: lauxlib.h $
** Auxiliary functions for building Lua libraries
** See Copyright Notice in lua.h
*/


#ifndef lua55auxlib_h
#define lua55auxlib_h


#include <stddef.h>
#include <stdio.h>

#include "luaconf.h"
#include "lua.h"


/* global table */
#define LUA_GNAME	"_G"


typedef struct lua55L_Buffer lua55L_Buffer;


/* extra error code for 'lua55L_loadfilex' */
#define LUA_ERRFILE     (LUA_ERRERR+1)


/* key, in the registry, for table of loaded modules */
#define LUA_LOADED_TABLE	"_LOADED"


/* key, in the registry, for table of preloaded loaders */
#define LUA_PRELOAD_TABLE	"_PRELOAD"


typedef struct lua55L_Reg {
  const char *name;
  lua55_CFunction func;
} lua55L_Reg;




#define LUAL_NUMSIZES	(sizeof(lua55_Integer)*16 + sizeof(lua55_Number))

LUALIB_API void (lua55L_checkversion_) (lua55_State *L, lua55_Number ver, size_t sz);
#define lua55L_checkversion(L)  \
	  lua55L_checkversion_(L, LUA_VERSION_NUM, LUAL_NUMSIZES)

LUALIB_API int (lua55L_getmetafield) (lua55_State *L, int obj, const char *e);
LUALIB_API int (lua55L_callmeta) (lua55_State *L, int obj, const char *e);
LUALIB_API const char *(lua55L_tolstring) (lua55_State *L, int idx, size_t *len);
LUALIB_API int (lua55L_argerror) (lua55_State *L, int arg, const char *extramsg);
LUALIB_API int (lua55L_typeerror) (lua55_State *L, int arg, const char *tname);
LUALIB_API const char *(lua55L_checklstring) (lua55_State *L, int arg,
                                                          size_t *l);
LUALIB_API const char *(lua55L_optlstring) (lua55_State *L, int arg,
                                          const char *def, size_t *l);
LUALIB_API lua55_Number (lua55L_checknumber) (lua55_State *L, int arg);
LUALIB_API lua55_Number (lua55L_optnumber) (lua55_State *L, int arg, lua55_Number def);

LUALIB_API lua55_Integer (lua55L_checkinteger) (lua55_State *L, int arg);
LUALIB_API lua55_Integer (lua55L_optinteger) (lua55_State *L, int arg,
                                          lua55_Integer def);

LUALIB_API void (lua55L_checkstack) (lua55_State *L, int sz, const char *msg);
LUALIB_API void (lua55L_checktype) (lua55_State *L, int arg, int t);
LUALIB_API void (lua55L_checkany) (lua55_State *L, int arg);

LUALIB_API int   (lua55L_newmetatable) (lua55_State *L, const char *tname);
LUALIB_API void  (lua55L_setmetatable) (lua55_State *L, const char *tname);
LUALIB_API void *(lua55L_testudata) (lua55_State *L, int ud, const char *tname);
LUALIB_API void *(lua55L_checkudata) (lua55_State *L, int ud, const char *tname);

LUALIB_API void (lua55L_where) (lua55_State *L, int lvl);
LUALIB_API int (lua55L_error) (lua55_State *L, const char *fmt, ...);

LUALIB_API int (lua55L_checkoption) (lua55_State *L, int arg, const char *def,
                                   const char *const lst[]);

LUALIB_API int (lua55L_fileresult) (lua55_State *L, int stat, const char *fname);
LUALIB_API int (lua55L_execresult) (lua55_State *L, int stat);

LUALIB_API void *lua55L_alloc (void *ud, void *ptr, size_t osize,
                                                  size_t nsize);


/* predefined references */
#define LUA_NOREF       (-2)
#define LUA_REFNIL      (-1)

LUALIB_API int (lua55L_ref) (lua55_State *L, int t);
LUALIB_API void (lua55L_unref) (lua55_State *L, int t, int ref);

LUALIB_API int (lua55L_loadfilex) (lua55_State *L, const char *filename,
                                               const char *mode);

#define lua55L_loadfile(L,f)	lua55L_loadfilex(L,f,NULL)

LUALIB_API int (lua55L_loadbufferx) (lua55_State *L, const char *buff, size_t sz,
                                   const char *name, const char *mode);
LUALIB_API int (lua55L_loadstring) (lua55_State *L, const char *s);

LUALIB_API lua55_State *(lua55L_newstate) (void);

LUALIB_API unsigned lua55L_makeseed (lua55_State *L);

LUALIB_API lua55_Integer (lua55L_len) (lua55_State *L, int idx);

LUALIB_API void (lua55L_addgsub) (lua55L_Buffer *b, const char *s,
                                     const char *p, const char *r);
LUALIB_API const char *(lua55L_gsub) (lua55_State *L, const char *s,
                                     const char *p, const char *r);

LUALIB_API void (lua55L_setfuncs) (lua55_State *L, const lua55L_Reg *l, int nup);

LUALIB_API int (lua55L_getsubtable) (lua55_State *L, int idx, const char *fname);

LUALIB_API void (lua55L_traceback) (lua55_State *L, lua55_State *L1,
                                  const char *msg, int level);

LUALIB_API void (lua55L_requiref) (lua55_State *L, const char *modname,
                                 lua55_CFunction openf, int glb);

/*
** ===============================================================
** some useful macros
** ===============================================================
*/


#define lua55L_newlibtable(L,l)	\
  lua55_createtable(L, 0, sizeof(l)/sizeof((l)[0]) - 1)

#define lua55L_newlib(L,l)  \
  (lua55L_checkversion(L), lua55L_newlibtable(L,l), lua55L_setfuncs(L,l,0))

#define lua55L_argcheck(L, cond,arg,extramsg)	\
	((void)(luai_likely(cond) || lua55L_argerror(L, (arg), (extramsg))))

#define lua55L_argexpected(L,cond,arg,tname)	\
	((void)(luai_likely(cond) || lua55L_typeerror(L, (arg), (tname))))

#define lua55L_checkstring(L,n)	(lua55L_checklstring(L, (n), NULL))
#define lua55L_optstring(L,n,d)	(lua55L_optlstring(L, (n), (d), NULL))

#define lua55L_typename(L,i)	lua55_typename(L, lua55_type(L,(i)))

#define lua55L_dofile(L, fn) \
	(lua55L_loadfile(L, fn) || lua55_pcall(L, 0, LUA_MULTRET, 0))

#define lua55L_dostring(L, s) \
	(lua55L_loadstring(L, s) || lua55_pcall(L, 0, LUA_MULTRET, 0))

#define lua55L_getmetatable(L,n)	(lua55_getfield(L, LUA_REGISTRYINDEX, (n)))

#define lua55L_opt(L,f,n,d)	(lua55_isnoneornil(L,(n)) ? (d) : f(L,(n)))

#define lua55L_loadbuffer(L,s,sz,n)	lua55L_loadbufferx(L,s,sz,n,NULL)


/*
** Perform arithmetic operations on lua55_Integer values with wrap-around
** semantics, as the Lua core does.
*/
#define lua55L_intop(op,v1,v2)  \
	((lua55_Integer)((lua55_Unsigned)(v1) op (lua55_Unsigned)(v2)))


/* push the value used to represent failure/error */
#if defined(LUA_FAILISFALSE)
#define lua55L_pushfail(L)	lua55_pushboolean(L, 0)
#else
#define lua55L_pushfail(L)	lua55_pushnil(L)
#endif



/*
** {======================================================
** Generic Buffer manipulation
** =======================================================
*/

struct lua55L_Buffer {
  char *b;  /* buffer address */
  size_t size;  /* buffer size */
  size_t n;  /* number of characters in buffer */
  lua55_State *L;
  union {
    LUAI_MAXALIGN;  /* ensure maximum alignment for buffer */
    char b[LUAL_BUFFERSIZE];  /* initial buffer */
  } init;
};


#define lua55L_bufflen(bf)	((bf)->n)
#define lua55L_buffaddr(bf)	((bf)->b)


#define lua55L_addchar(B,c) \
  ((void)((B)->n < (B)->size || lua55L_prepbuffsize((B), 1)), \
   ((B)->b[(B)->n++] = (c)))

#define lua55L_addsize(B,s)	((B)->n += (s))

#define lua55L_buffsub(B,s)	((B)->n -= (s))

LUALIB_API void (lua55L_buffinit) (lua55_State *L, lua55L_Buffer *B);
LUALIB_API char *(lua55L_prepbuffsize) (lua55L_Buffer *B, size_t sz);
LUALIB_API void (lua55L_addlstring) (lua55L_Buffer *B, const char *s, size_t l);
LUALIB_API void (lua55L_addstring) (lua55L_Buffer *B, const char *s);
LUALIB_API void (lua55L_addvalue) (lua55L_Buffer *B);
LUALIB_API void (lua55L_pushresult) (lua55L_Buffer *B);
LUALIB_API void (lua55L_pushresultsize) (lua55L_Buffer *B, size_t sz);
LUALIB_API char *(lua55L_buffinitsize) (lua55_State *L, lua55L_Buffer *B, size_t sz);

#define lua55L_prepbuffer(B)	lua55L_prepbuffsize(B, LUAL_BUFFERSIZE)

/* }====================================================== */



/*
** {======================================================
** File handles for IO library
** =======================================================
*/

/*
** A file handle is a userdata with metatable 'LUA_FILEHANDLE' and
** initial structure 'lua55L_Stream' (it may contain other fields
** after that initial structure).
*/

#define LUA_FILEHANDLE          "FILE*"


typedef struct lua55L_Stream {
  FILE *f;  /* stream (NULL for incompletely created streams) */
  lua55_CFunction closef;  /* to close stream (NULL for closed streams) */
} lua55L_Stream;

/* }====================================================== */


/*
** {============================================================
** Compatibility with deprecated conversions
** =============================================================
*/
#if defined(LUA_COMPAT_APIINTCASTS)

#define lua55L_checkunsigned(L,a)	((lua55_Unsigned)lua55L_checkinteger(L,a))
#define lua55L_optunsigned(L,a,d)	\
	((lua55_Unsigned)lua55L_optinteger(L,a,(lua55_Integer)(d)))

#define lua55L_checkint(L,n)	((int)lua55L_checkinteger(L, (n)))
#define lua55L_optint(L,n,d)	((int)lua55L_optinteger(L, (n), (d)))

#define lua55L_checklong(L,n)	((long)lua55L_checkinteger(L, (n)))
#define lua55L_optlong(L,n,d)	((long)lua55L_optinteger(L, (n), (d)))

#endif

/* Internal compatibility - allow existing code to compile */
typedef lua55L_Buffer luaL_Buffer;
typedef lua55L_Reg luaL_Reg;
typedef lua55L_Stream luaL_Stream;


#endif

