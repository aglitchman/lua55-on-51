/*
** $Id: lua.h $
** Lua - A Scripting Language
** Lua.org, PUC-Rio, Brazil (www.lua.org)
** See Copyright Notice at the end of this file
*/


#ifndef lua55_h
#define lua55_h

#include <stdarg.h>
#include <stddef.h>


#define LUA_COPYRIGHT	LUA_RELEASE "  Copyright (C) 1994-2025 Lua.org, PUC-Rio"
#define LUA_AUTHORS	"R. Ierusalimschy, L. H. de Figueiredo, W. Celes"


#define LUA_VERSION_MAJOR_N	5
#define LUA_VERSION_MINOR_N	5
#define LUA_VERSION_RELEASE_N	0

#define LUA_VERSION_NUM  (LUA_VERSION_MAJOR_N * 100 + LUA_VERSION_MINOR_N)
#define LUA_VERSION_RELEASE_NUM  (LUA_VERSION_NUM * 100 + LUA_VERSION_RELEASE_N)


#include "luaconf.h"


/* mark for precompiled code ('<esc>Lua') */
#define LUA_SIGNATURE	"\x1bLua"

/* option for multiple returns in 'lua_pcall' and 'lua_call' */
#define LUA_MULTRET	(-1)


/*
** Pseudo-indices
** (The stack size is limited to INT_MAX/2; we keep some free empty
** space after that to help overflow detection.)
*/
#define LUA_REGISTRYINDEX	(-(INT_MAX/2 + 1000))
#define lua55_upvalueindex(i)	(LUA_REGISTRYINDEX - (i))


/* thread status */
#define LUA_OK		0
#define LUA_YIELD	1
#define LUA_ERRRUN	2
#define LUA_ERRSYNTAX	3
#define LUA_ERRMEM	4
#define LUA_ERRERR	5


typedef struct lua_State lua55_State;


/* type of numbers in Lua */
typedef LUA_NUMBER lua55_Number;


/* type for integer functions */
typedef LUA_INTEGER lua55_Integer;

/* unsigned integer type */
typedef LUA_UNSIGNED lua55_Unsigned;

/* type for continuation-function contexts */
typedef LUA_KCONTEXT lua55_KContext;


/*
** Type for C functions registered with Lua
*/
typedef int (*lua55_CFunction) (lua55_State *L);

/*
** Type for continuation functions
*/
typedef int (*lua55_KFunction) (lua55_State *L, int status, lua55_KContext ctx);


/*
** Type for functions that read/write blocks when loading/dumping Lua chunks
*/
typedef const char * (*lua55_Reader) (lua55_State *L, void *ud, size_t *sz);

typedef int (*lua55_Writer) (lua55_State *L, const void *p, size_t sz, void *ud);


/*
** Type for memory-allocation functions
*/
typedef void * (*lua55_Alloc) (void *ud, void *ptr, size_t osize, size_t nsize);


/*
** Type for warning functions
*/
typedef void (*lua55_WarnFunction) (void *ud, const char *msg, int tocont);


/*
** Type used by the debug API to collect debug information
*/
typedef struct lua55_Debug lua55_Debug;


/*
** Functions to be called by the debugger in specific events
*/
typedef void (*lua55_Hook) (lua55_State *L, lua55_Debug *ar);



/*
** basic types
*/
#define LUA_TNONE		(-1)

#define LUA_TNIL		0
#define LUA_TBOOLEAN		1
#define LUA_TLIGHTUSERDATA	2
#define LUA_TNUMBER		3
#define LUA_TSTRING		4
#define LUA_TTABLE		5
#define LUA_TFUNCTION		6
#define LUA_TUSERDATA		7
#define LUA_TTHREAD		8

#define LUA_NUMTYPES		9



/* minimum Lua stack available to a C function */
#define LUA_MINSTACK	20


/* predefined values in the registry */
/* index 1 is reserved for the reference mechanism */
#define LUA_RIDX_GLOBALS	2
#define LUA_RIDX_MAINTHREAD	3
#define LUA_RIDX_LAST		3


/* type of numbers in Lua */
typedef LUA_NUMBER lua55_Number;


/* type for integer functions */
typedef LUA_INTEGER lua55_Integer;

/* unsigned integer type */
typedef LUA_UNSIGNED lua55_Unsigned;

/* type for continuation-function contexts */
typedef LUA_KCONTEXT lua55_KContext;


/*
** Type for C functions registered with Lua
*/
typedef int (*lua55_CFunction) (lua55_State *L);

/*
** Type for continuation functions
*/
typedef int (*lua55_KFunction) (lua55_State *L, int status, lua55_KContext ctx);


/*
** Type for functions that read/write blocks when loading/dumping Lua chunks
*/
typedef const char * (*lua55_Reader) (lua55_State *L, void *ud, size_t *sz);

typedef int (*lua55_Writer) (lua55_State *L, const void *p, size_t sz, void *ud);


/*
** Type for memory-allocation functions
*/
typedef void * (*lua55_Alloc) (void *ud, void *ptr, size_t osize, size_t nsize);


/*
** Type for warning functions
*/
typedef void (*lua55_WarnFunction) (void *ud, const char *msg, int tocont);


/*
** Type used by the debug API to collect debug information
*/
typedef struct lua55_Debug lua55_Debug;


/*
** Functions to be called by the debugger in specific events
*/
typedef void (*lua55_Hook) (lua55_State *L, lua55_Debug *ar);






/*
** generic extra include file
*/
#if defined(LUA_USER_H)
#include LUA_USER_H
#endif


/*
** RCS ident string
*/
extern const char lua55_ident[];


/*
** state manipulation
*/
LUA_API lua55_State *(lua55_newstate) (lua55_Alloc f, void *ud, unsigned seed);
LUA_API void       (lua55_close) (lua55_State *L);
LUA_API lua55_State *(lua55_newthread) (lua55_State *L);
LUA_API int        (lua55_closethread) (lua55_State *L, lua55_State *from);

LUA_API lua55_CFunction (lua55_atpanic) (lua55_State *L, lua55_CFunction panicf);


LUA_API lua55_Number (lua55_version) (lua55_State *L);


/*
** basic stack manipulation
*/
LUA_API int   (lua55_absindex) (lua55_State *L, int idx);
LUA_API int   (lua55_gettop) (lua55_State *L);
LUA_API void  (lua55_settop) (lua55_State *L, int idx);
LUA_API void  (lua55_pushvalue) (lua55_State *L, int idx);
LUA_API void  (lua55_rotate) (lua55_State *L, int idx, int n);
LUA_API void  (lua55_copy) (lua55_State *L, int fromidx, int toidx);
LUA_API int   (lua55_checkstack) (lua55_State *L, int n);

LUA_API void  (lua55_xmove) (lua55_State *from, lua55_State *to, int n);


/*
** access functions (stack -> C)
*/

LUA_API int             (lua55_isnumber) (lua55_State *L, int idx);
LUA_API int             (lua55_isstring) (lua55_State *L, int idx);
LUA_API int             (lua55_iscfunction) (lua55_State *L, int idx);
LUA_API int             (lua55_isinteger) (lua55_State *L, int idx);
LUA_API int             (lua55_isuserdata) (lua55_State *L, int idx);
LUA_API int             (lua55_type) (lua55_State *L, int idx);
LUA_API const char     *(lua55_typename) (lua55_State *L, int tp);

LUA_API lua55_Number      (lua55_tonumberx) (lua55_State *L, int idx, int *isnum);
LUA_API lua55_Integer     (lua55_tointegerx) (lua55_State *L, int idx, int *isnum);
LUA_API int             (lua55_toboolean) (lua55_State *L, int idx);
LUA_API const char     *(lua55_tolstring) (lua55_State *L, int idx, size_t *len);
LUA_API lua55_Unsigned    (lua55_rawlen) (lua55_State *L, int idx);
LUA_API lua55_CFunction   (lua55_tocfunction) (lua55_State *L, int idx);
LUA_API void	       *(lua55_touserdata) (lua55_State *L, int idx);
LUA_API lua55_State      *(lua55_tothread) (lua55_State *L, int idx);
LUA_API const void     *(lua55_topointer) (lua55_State *L, int idx);


/*
** Comparison and arithmetic functions
*/

#define LUA_OPADD	0	/* ORDER TM, ORDER OP */
#define LUA_OPSUB	1
#define LUA_OPMUL	2
#define LUA_OPMOD	3
#define LUA_OPPOW	4
#define LUA_OPDIV	5
#define LUA_OPIDIV	6
#define LUA_OPBAND	7
#define LUA_OPBOR	8
#define LUA_OPBXOR	9
#define LUA_OPSHL	10
#define LUA_OPSHR	11
#define LUA_OPUNM	12
#define LUA_OPBNOT	13

LUA_API void  (lua55_arith) (lua55_State *L, int op);

#define LUA_OPEQ	0
#define LUA_OPLT	1
#define LUA_OPLE	2

LUA_API int   (lua55_rawequal) (lua55_State *L, int idx1, int idx2);
LUA_API int   (lua55_compare) (lua55_State *L, int idx1, int idx2, int op);


/*
** push functions (C -> stack)
*/
LUA_API void        (lua55_pushnil) (lua55_State *L);
LUA_API void        (lua55_pushnumber) (lua55_State *L, lua55_Number n);
LUA_API void        (lua55_pushinteger) (lua55_State *L, lua55_Integer n);
LUA_API const char *(lua55_pushlstring) (lua55_State *L, const char *s, size_t len);
LUA_API const char *(lua55_pushexternalstring) (lua55_State *L,
		const char *s, size_t len, lua55_Alloc falloc, void *ud);
LUA_API const char *(lua55_pushstring) (lua55_State *L, const char *s);
LUA_API const char *(lua55_pushvfstring) (lua55_State *L, const char *fmt,
                                                      va_list argp);
LUA_API const char *(lua55_pushfstring) (lua55_State *L, const char *fmt, ...);
LUA_API void  (lua55_pushcclosure) (lua55_State *L, lua55_CFunction fn, int n);
LUA_API void  (lua55_pushboolean) (lua55_State *L, int b);
LUA_API void  (lua55_pushlightuserdata) (lua55_State *L, void *p);
LUA_API int   (lua55_pushthread) (lua55_State *L);


/*
** get functions (Lua -> stack)
*/
LUA_API int (lua55_getglobal) (lua55_State *L, const char *name);
LUA_API int (lua55_gettable) (lua55_State *L, int idx);
LUA_API int (lua55_getfield) (lua55_State *L, int idx, const char *k);
LUA_API int (lua55_geti) (lua55_State *L, int idx, lua55_Integer n);
LUA_API int (lua55_rawget) (lua55_State *L, int idx);
LUA_API int (lua55_rawgeti) (lua55_State *L, int idx, lua55_Integer n);
LUA_API int (lua55_rawgetp) (lua55_State *L, int idx, const void *p);

LUA_API void  (lua55_createtable) (lua55_State *L, int narr, int nrec);
LUA_API void *(lua55_newuserdatauv) (lua55_State *L, size_t sz, int nuvalue);
LUA_API int   (lua55_getmetatable) (lua55_State *L, int objindex);
LUA_API int  (lua55_getiuservalue) (lua55_State *L, int idx, int n);


/*
** set functions (stack -> Lua)
*/
LUA_API void  (lua55_setglobal) (lua55_State *L, const char *name);
LUA_API void  (lua55_settable) (lua55_State *L, int idx);
LUA_API void  (lua55_setfield) (lua55_State *L, int idx, const char *k);
LUA_API void  (lua55_seti) (lua55_State *L, int idx, lua55_Integer n);
LUA_API void  (lua55_rawset) (lua55_State *L, int idx);
LUA_API void  (lua55_rawseti) (lua55_State *L, int idx, lua55_Integer n);
LUA_API void  (lua55_rawsetp) (lua55_State *L, int idx, const void *p);
LUA_API int   (lua55_setmetatable) (lua55_State *L, int objindex);
LUA_API int   (lua55_setiuservalue) (lua55_State *L, int idx, int n);


/*
** 'load' and 'call' functions (load and run Lua code)
*/
LUA_API void  (lua55_callk) (lua55_State *L, int nargs, int nresults,
                           lua55_KContext ctx, lua55_KFunction k);
#define lua55_call(L,n,r)		lua55_callk(L, (n), (r), 0, NULL)

LUA_API int   (lua55_pcallk) (lua55_State *L, int nargs, int nresults, int errfunc,
                            lua55_KContext ctx, lua55_KFunction k);
#define lua55_pcall(L,n,r,f)	lua55_pcallk(L, (n), (r), (f), 0, NULL)

LUA_API int   (lua55_load) (lua55_State *L, lua55_Reader reader, void *dt,
                          const char *chunkname, const char *mode);

LUA_API int (lua55_dump) (lua55_State *L, lua55_Writer writer, void *data, int strip);


/*
** coroutine functions
*/
LUA_API int  (lua55_yieldk)     (lua55_State *L, int nresults, lua55_KContext ctx,
                               lua55_KFunction k);
LUA_API int  (lua55_resume)     (lua55_State *L, lua55_State *from, int narg,
                               int *nres);
LUA_API int  (lua55_status)     (lua55_State *L);
LUA_API int (lua55_isyieldable) (lua55_State *L);

#define lua55_yield(L,n)		lua55_yieldk(L, (n), 0, NULL)


/*
** Warning-related functions
*/
LUA_API void (lua55_setwarnf) (lua55_State *L, lua55_WarnFunction f, void *ud);
LUA_API void (lua55_warning)  (lua55_State *L, const char *msg, int tocont);


/*
** garbage-collection options
*/

#define LUA_GCSTOP		0
#define LUA_GCRESTART		1
#define LUA_GCCOLLECT		2
#define LUA_GCCOUNT		3
#define LUA_GCCOUNTB		4
#define LUA_GCSTEP		5
#define LUA_GCISRUNNING		6
#define LUA_GCGEN		7
#define LUA_GCINC		8
#define LUA_GCPARAM		9


/*
** garbage-collection parameters
*/
/* parameters for generational mode */
#define LUA_GCPMINORMUL		0  /* control minor collections */
#define LUA_GCPMAJORMINOR	1  /* control shift major->minor */
#define LUA_GCPMINORMAJOR	2  /* control shift minor->major */

/* parameters for incremental mode */
#define LUA_GCPPAUSE		3  /* size of pause between successive GCs */
#define LUA_GCPSTEPMUL		4  /* GC "speed" */
#define LUA_GCPSTEPSIZE		5  /* GC granularity */

/* number of parameters */
#define LUA_GCPN		6


LUA_API int (lua55_gc) (lua55_State *L, int what, ...);


/*
** miscellaneous functions
*/

LUA_API int   (lua55_error) (lua55_State *L);

LUA_API int   (lua55_next) (lua55_State *L, int idx);

LUA_API void  (lua55_concat) (lua55_State *L, int n);
LUA_API void  (lua55_len)    (lua55_State *L, int idx);

#define LUA_N2SBUFFSZ	64
LUA_API unsigned  (lua55_numbertocstring) (lua55_State *L, int idx, char *buff);
LUA_API size_t  (lua55_stringtonumber) (lua55_State *L, const char *s);

LUA_API lua55_Alloc (lua55_getallocf) (lua55_State *L, void **ud);
LUA_API void      (lua55_setallocf) (lua55_State *L, lua55_Alloc f, void *ud);

LUA_API void (lua55_toclose) (lua55_State *L, int idx);
LUA_API void (lua55_closeslot) (lua55_State *L, int idx);


/*
** {==============================================================
** some useful macros
** ===============================================================
*/

#define lua55_getextraspace(L)	((void *)((char *)(L) - LUA_EXTRASPACE))

#define lua55_tonumber(L,i)	lua55_tonumberx(L,(i),NULL)
#define lua55_tointeger(L,i)	lua55_tointegerx(L,(i),NULL)

#define lua55_pop(L,n)		lua55_settop(L, -(n)-1)

#define lua55_newtable(L)		lua55_createtable(L, 0, 0)

#define lua55_register(L,n,f) (lua55_pushcfunction(L, (f)), lua55_setglobal(L, (n)))

#define lua55_pushcfunction(L,f)	lua55_pushcclosure(L, (f), 0)

#define lua55_isfunction(L,n)	(lua55_type(L, (n)) == LUA_TFUNCTION)
#define lua55_istable(L,n)	(lua55_type(L, (n)) == LUA_TTABLE)
#define lua55_islightuserdata(L,n)	(lua55_type(L, (n)) == LUA_TLIGHTUSERDATA)
#define lua55_isnil(L,n)		(lua55_type(L, (n)) == LUA_TNIL)
#define lua55_isboolean(L,n)	(lua55_type(L, (n)) == LUA_TBOOLEAN)
#define lua55_isthread(L,n)	(lua55_type(L, (n)) == LUA_TTHREAD)
#define lua55_isnone(L,n)		(lua55_type(L, (n)) == LUA_TNONE)
#define lua55_isnoneornil(L, n)	(lua55_type(L, (n)) <= 0)

#define lua55_pushliteral(L, s)	lua55_pushstring(L, "" s)

#define lua55_pushglobaltable(L)  \
	((void)lua55_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS))

#define lua55_tostring(L,i)	lua55_tolstring(L, (i), NULL)


#define lua55_insert(L,idx)	lua55_rotate(L, (idx), 1)

#define lua55_remove(L,idx)	(lua55_rotate(L, (idx), -1), lua55_pop(L, 1))

#define lua55_replace(L,idx)	(lua55_copy(L, -1, (idx)), lua55_pop(L, 1))

/* }============================================================== */


/*
** {==============================================================
** compatibility macros
** ===============================================================
*/

#define lua55_newuserdata(L,s)	lua55_newuserdatauv(L,s,1)
#define lua55_getuservalue(L,idx)	lua55_getiuservalue(L,idx,1)
#define lua55_setuservalue(L,idx)	lua55_setiuservalue(L,idx,1)

#define lua55_resetthread(L)	lua55_closethread(L,NULL)

/* }============================================================== */

/*
** {======================================================================
** Debug API
** =======================================================================
*/


/*
** Event codes
*/
#define LUA_HOOKCALL	0
#define LUA_HOOKRET	1
#define LUA_HOOKLINE	2
#define LUA_HOOKCOUNT	3
#define LUA_HOOKTAILCALL 4


/*
** Event masks
*/
#define LUA_MASKCALL	(1 << LUA_HOOKCALL)
#define LUA_MASKRET	(1 << LUA_HOOKRET)
#define LUA_MASKLINE	(1 << LUA_HOOKLINE)
#define LUA_MASKCOUNT	(1 << LUA_HOOKCOUNT)


LUA_API int (lua55_getstack) (lua55_State *L, int level, lua55_Debug *ar);
LUA_API int (lua55_getinfo) (lua55_State *L, const char *what, lua55_Debug *ar);
LUA_API const char *(lua55_getlocal) (lua55_State *L, const lua55_Debug *ar, int n);
LUA_API const char *(lua55_setlocal) (lua55_State *L, const lua55_Debug *ar, int n);
LUA_API const char *(lua55_getupvalue) (lua55_State *L, int funcindex, int n);
LUA_API const char *(lua55_setupvalue) (lua55_State *L, int funcindex, int n);

LUA_API void *(lua55_upvalueid) (lua55_State *L, int fidx, int n);
LUA_API void  (lua55_upvaluejoin) (lua55_State *L, int fidx1, int n1,
                                               int fidx2, int n2);

LUA_API void (lua55_sethook) (lua55_State *L, lua55_Hook func, int mask, int count);
LUA_API lua55_Hook (lua55_gethook) (lua55_State *L);
LUA_API int (lua55_gethookmask) (lua55_State *L);
LUA_API int (lua55_gethookcount) (lua55_State *L);


struct lua55_Debug {
  int event;
  const char *name;	/* (n) */
  const char *namewhat;	/* (n) 'global', 'local', 'field', 'method' */
  const char *what;	/* (S) 'Lua', 'C', 'main', 'tail' */
  const char *source;	/* (S) */
  size_t srclen;	/* (S) */
  int currentline;	/* (l) */
  int linedefined;	/* (S) */
  int lastlinedefined;	/* (S) */
  unsigned char nups;	/* (u) number of upvalues */
  unsigned char nparams;/* (u) number of parameters */
  char isvararg;        /* (u) */
  unsigned char extraargs;  /* (t) number of extra arguments */
  char istailcall;	/* (t) */
  int ftransfer;   /* (r) index of first value transferred */
  int ntransfer;   /* (r) number of transferred values */
  char short_src[LUA_IDSIZE]; /* (S) */
  /* private part */
  struct CallInfo *i_ci;  /* active function */
};

/* }====================================================================== */


#define LUAI_TOSTRAUX(x)	#x
#define LUAI_TOSTR(x)		LUAI_TOSTRAUX(x)

#define LUA_VERSION_MAJOR	LUAI_TOSTR(LUA_VERSION_MAJOR_N)
#define LUA_VERSION_MINOR	LUAI_TOSTR(LUA_VERSION_MINOR_N)
#define LUA_VERSION_RELEASE	LUAI_TOSTR(LUA_VERSION_RELEASE_N)

#define LUA_VERSION	"Lua " LUA_VERSION_MAJOR "." LUA_VERSION_MINOR
#define LUA_RELEASE	LUA_VERSION "." LUA_VERSION_RELEASE


/******************************************************************************
* Copyright (C) 1994-2025 Lua.org, PUC-Rio.
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 ******************************************************************************/


/* Internal compatibility - allow existing code to compile
** These are just aliases to the lua55_ prefixed versions
*/
typedef struct lua_State lua_State;
typedef lua55_Number lua_Number;
typedef lua55_Integer lua_Integer;
typedef lua55_Unsigned lua_Unsigned;
typedef lua55_KContext lua_KContext;
typedef lua55_CFunction lua_CFunction;
typedef lua55_KFunction lua_KFunction;
typedef lua55_Reader lua_Reader;
typedef lua55_Writer lua_Writer;
typedef lua55_Alloc lua_Alloc;
typedef lua55_WarnFunction lua_WarnFunction;
typedef lua55_Debug lua_Debug;
typedef lua55_Hook lua_Hook;


#endif
