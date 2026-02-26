/*
** $Id: loadlib.c $
** Dynamic library loader for Lua
** See Copyright Notice in lua.h
**
** This module contains an implementation of loadlib for Unix systems
** that have dlfcn, an implementation for Windows, and a stub for other
** systems.
*/

#define loadlib_c
#define LUA_LIB

#include "lprefix.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"
#include "llimits.h"


/*
** LUA_CSUBSEP is the character that replaces dots in submodule names
** when searching for a C loader.
** LUA_LSUBSEP is the character that replaces dots in submodule names
** when searching for a Lua loader.
*/
#if !defined(LUA_CSUBSEP)
#define LUA_CSUBSEP		LUA_DIRSEP
#endif

#if !defined(LUA_LSUBSEP)
#define LUA_LSUBSEP		LUA_DIRSEP
#endif


/* prefix for open functions in C libraries */
#define LUA_POF		"luaopen_"

/* separator for open functions in C libraries */
#define LUA_OFSEP	"_"


/*
** key for table in the registry that keeps handles
** for all loaded C libraries
*/
static const char *const CLIBS = "_CLIBS";

#define LIB_FAIL	"open"


#define setprogdir(L)           ((void)0)


/* cast void* to a Lua function */
#define cast_Lfunc(p)	cast(lua_CFunction, cast_func(p))


/*
** system-dependent functions
*/

/*
** unload library 'lib'
*/
static void lsys_unloadlib (void *lib);

/*
** load C library in file 'path'. If 'seeglb', load with all names in
** the library global.
** Returns the library; in case of error, returns NULL plus an
** error string in the stack.
*/
static void *lsys_load (lua55_State *L, const char *path, int seeglb);

/*
** Try to find a function named 'sym' in library 'lib'.
** Returns the function; in case of error, returns NULL plus an
** error string in the stack.
*/
static lua_CFunction lsys_sym (lua55_State *L, void *lib, const char *sym);




#if defined(LUA_USE_DLOPEN)	/* { */
/*
** {========================================================================
** This is an implementation of loadlib based on the dlfcn interface,
** which is available in all POSIX systems.
** =========================================================================
*/

#include <dlfcn.h>


static void lsys_unloadlib (void *lib) {
  dlclose(lib);
}


static void *lsys_load (lua55_State *L, const char *path, int seeglb) {
  void *lib = dlopen(path, RTLD_NOW | (seeglb ? RTLD_GLOBAL : RTLD_LOCAL));
  if (l_unlikely(lib == NULL))
    lua55_pushstring(L, dlerror());
  return lib;
}


static lua_CFunction lsys_sym (lua55_State *L, void *lib, const char *sym) {
  lua_CFunction f = cast_Lfunc(dlsym(lib, sym));
  if (l_unlikely(f == NULL))
    lua55_pushstring(L, dlerror());
  return f;
}

/* }====================================================== */



#elif defined(LUA_DL_DLL)	/* }{ */
/*
** {======================================================================
** This is an implementation of loadlib for Windows using native functions.
** =======================================================================
*/

#include <windows.h>


/*
** optional flags for LoadLibraryEx
*/
#if !defined(LUA_LLE_FLAGS)
#define LUA_LLE_FLAGS	0
#endif


#undef setprogdir


/*
** Replace in the path (on the top of the stack) any occurrence
** of LUA_EXEC_DIR with the executable's path.
*/
static void setprogdir (lua55_State *L) {
  char buff[MAX_PATH + 1];
  char *lb;
  DWORD nsize = sizeof(buff)/sizeof(char);
  DWORD n = GetModuleFileNameA(NULL, buff, nsize);  /* get exec. name */
  if (n == 0 || n == nsize || (lb = strrchr(buff, '\\')) == NULL)
    lua55L_error(L, "unable to get ModuleFileName");
  else {
    *lb = '\0';  /* cut name on the last '\\' to get the path */
    lua55L_gsub(L, lua55_tostring(L, -1), LUA_EXEC_DIR, buff);
    lua55_remove(L, -2);  /* remove original string */
  }
}




static void pusherror (lua55_State *L) {
  int error = GetLastError();
  char buffer[128];
  if (FormatMessageA(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
      NULL, error, 0, buffer, sizeof(buffer)/sizeof(char), NULL))
    lua55_pushstring(L, buffer);
  else
    lua55_pushfstring(L, "system error %d\n", error);
}

static void lsys_unloadlib (void *lib) {
  FreeLibrary((HMODULE)lib);
}


static void *lsys_load (lua55_State *L, const char *path, int seeglb) {
  HMODULE lib = LoadLibraryExA(path, NULL, LUA_LLE_FLAGS);
  (void)(seeglb);  /* not used: symbols are 'global' by default */
  if (lib == NULL) pusherror(L);
  return lib;
}


static lua_CFunction lsys_sym (lua55_State *L, void *lib, const char *sym) {
  lua_CFunction f = cast_Lfunc(GetProcAddress((HMODULE)lib, sym));
  if (f == NULL) pusherror(L);
  return f;
}

/* }====================================================== */


#else				/* }{ */
/*
** {======================================================
** Fallback for other systems
** =======================================================
*/

#undef LIB_FAIL
#define LIB_FAIL	"absent"


#define DLMSG	"dynamic libraries not enabled; check your Lua installation"


static void lsys_unloadlib (void *lib) {
  (void)(lib);  /* not used */
}


static void *lsys_load (lua55_State *L, const char *path, int seeglb) {
  (void)(path); (void)(seeglb);  /* not used */
  lua55_pushliteral(L, DLMSG);
  return NULL;
}


static lua_CFunction lsys_sym (lua55_State *L, void *lib, const char *sym) {
  (void)(lib); (void)(sym);  /* not used */
  lua55_pushliteral(L, DLMSG);
  return NULL;
}

/* }====================================================== */
#endif				/* } */


/*
** {==================================================================
** Set Paths
** ===================================================================
*/

/*
** LUA_PATH_VAR and LUA_CPATH_VAR are the names of the environment
** variables that Lua check to set its paths.
*/
#if !defined(LUA_PATH_VAR)
#define LUA_PATH_VAR    "LUA_PATH"
#endif

#if !defined(LUA_CPATH_VAR)
#define LUA_CPATH_VAR   "LUA_CPATH"
#endif



/*
** return registry.LUA_NOENV as a boolean
*/
static int noenv (lua55_State *L) {
  int b;
  lua55_getfield(L, LUA_REGISTRYINDEX, "LUA_NOENV");
  b = lua55_toboolean(L, -1);
  lua55_pop(L, 1);  /* remove value */
  return b;
}


/*
** Set a path. (If using the default path, assume it is a string
** literal in C and create it as an external string.)
*/
static void setpath (lua55_State *L, const char *fieldname,
                                   const char *envname,
                                   const char *dft) {
  const char *dftmark;
  const char *nver = lua55_pushfstring(L, "%s%s", envname, LUA_VERSUFFIX);
  const char *path = getenv(nver);  /* try versioned name */
  if (path == NULL)  /* no versioned environment variable? */
    path = getenv(envname);  /* try unversioned name */
  if (path == NULL || noenv(L))  /* no environment variable? */
    lua55_pushexternalstring(L, dft, strlen(dft), NULL, NULL);  /* use default */
  else if ((dftmark = strstr(path, LUA_PATH_SEP LUA_PATH_SEP)) == NULL)
    lua55_pushstring(L, path);  /* nothing to change */
  else {  /* path contains a ";;": insert default path in its place */
    size_t len = strlen(path);
    luaL_Buffer b;
    lua55L_buffinit(L, &b);
    if (path < dftmark) {  /* is there a prefix before ';;'? */
      lua55L_addlstring(&b, path, ct_diff2sz(dftmark - path));  /* add it */
      lua55L_addchar(&b, *LUA_PATH_SEP);
    }
    lua55L_addstring(&b, dft);  /* add default */
    if (dftmark < path + len - 2) {  /* is there a suffix after ';;'? */
      lua55L_addchar(&b, *LUA_PATH_SEP);
      lua55L_addlstring(&b, dftmark + 2, ct_diff2sz((path + len - 2) - dftmark));
    }
    lua55L_pushresult(&b);
  }
  setprogdir(L);
  lua55_setfield(L, -3, fieldname);  /* package[fieldname] = path value */
  lua55_pop(L, 1);  /* pop versioned variable name ('nver') */
}

/* }================================================================== */


/*
** External strings created by DLLs may need the DLL code to be
** deallocated. This implies that a DLL can only be unloaded after all
** its strings were deallocated. To ensure that, we create a 'library
** string' to represent each DLL, and when this string is deallocated
** it closes its corresponding DLL.
** (The string itself is irrelevant; its userdata is the DLL pointer.)
*/


/*
** return registry.CLIBS[path]
*/
static void *checkclib (lua55_State *L, const char *path) {
  void *plib;
  lua55_getfield(L, LUA_REGISTRYINDEX, CLIBS);
  lua55_getfield(L, -1, path);
  plib = lua55_touserdata(L, -1);  /* plib = CLIBS[path] */
  lua55_pop(L, 2);  /* pop CLIBS table and 'plib' */
  return plib;
}


/*
** Deallocate function for library strings.
** Unload the DLL associated with the string being deallocated.
*/
static void *freelib (void *ud, void *ptr, size_t osize, size_t nsize) {
  /* string itself is irrelevant and static */
  (void)ptr; (void)osize; (void)nsize;
  lsys_unloadlib(ud);  /* unload library represented by the string */
  return NULL;
}


/*
** Create a library string that, when deallocated, will unload 'plib'
*/
static void createlibstr (lua55_State *L, void *plib) {
  /* common content for all library strings */
  static const char dummy[] = "01234567890";
  lua55_pushexternalstring(L, dummy, sizeof(dummy) - 1, freelib, plib);
}


/*
** registry.CLIBS[path] = plib          -- for queries.
** Also create a reference to strlib, so that the library string will
** only be collected when registry.CLIBS is collected.
*/
static void addtoclib (lua55_State *L, const char *path, void *plib) {
  lua55_getfield(L, LUA_REGISTRYINDEX, CLIBS);
  lua55_pushlightuserdata(L, plib);
  lua55_setfield(L, -2, path);  /* CLIBS[path] = plib */
  createlibstr(L, plib);
  lua55L_ref(L, -2);  /* keep library string in CLIBS */
  lua55_pop(L, 1);  /* pop CLIBS table */
}


/* error codes for 'lookforfunc' */
#define ERRLIB		1
#define ERRFUNC		2

/*
** Look for a C function named 'sym' in a dynamically loaded library
** 'path'.
** First, check whether the library is already loaded; if not, try
** to load it.
** Then, if 'sym' is '*', return true (as library has been loaded).
** Otherwise, look for symbol 'sym' in the library and push a
** C function with that symbol.
** Return 0 with 'true' or a function in the stack; in case of
** errors, return an error code with an error message in the stack.
*/
static int lookforfunc (lua55_State *L, const char *path, const char *sym) {
  void *reg = checkclib(L, path);  /* check loaded C libraries */
  if (reg == NULL) {  /* must load library? */
    reg = lsys_load(L, path, *sym == '*');  /* global symbols if 'sym'=='*' */
    if (reg == NULL) return ERRLIB;  /* unable to load library */
    addtoclib(L, path, reg);
  }
  if (*sym == '*') {  /* loading only library (no function)? */
    lua55_pushboolean(L, 1);  /* return 'true' */
    return 0;  /* no errors */
  }
  else {
    lua_CFunction f = lsys_sym(L, reg, sym);
    if (f == NULL)
      return ERRFUNC;  /* unable to find function */
    lua55_pushcfunction(L, f);  /* else create new function */
    return 0;  /* no errors */
  }
}


static int ll_loadlib (lua55_State *L) {
  const char *path = lua55L_checkstring(L, 1);
  const char *init = lua55L_checkstring(L, 2);
  int stat = lookforfunc(L, path, init);
  if (l_likely(stat == 0))  /* no errors? */
    return 1;  /* return the loaded function */
  else {  /* error; error message is on stack top */
    lua55L_pushfail(L);
    lua55_insert(L, -2);
    lua55_pushstring(L, (stat == ERRLIB) ?  LIB_FAIL : "init");
    return 3;  /* return fail, error message, and where */
  }
}



/*
** {======================================================
** 'require' function
** =======================================================
*/


static int readable (const char *filename) {
  FILE *f = fopen(filename, "r");  /* try to open file */
  if (f == NULL) return 0;  /* open failed */
  fclose(f);
  return 1;
}


/*
** Get the next name in '*path' = 'name1;name2;name3;...', changing
** the ending ';' to '\0' to create a zero-terminated string. Return
** NULL when list ends.
*/
static const char *getnextfilename (char **path, char *end) {
  char *sep;
  char *name = *path;
  if (name == end)
    return NULL;  /* no more names */
  else if (*name == '\0') {  /* from previous iteration? */
    *name = *LUA_PATH_SEP;  /* restore separator */
    name++;  /* skip it */
  }
  sep = strchr(name, *LUA_PATH_SEP);  /* find next separator */
  if (sep == NULL)  /* separator not found? */
    sep = end;  /* name goes until the end */
  *sep = '\0';  /* finish file name */
  *path = sep;  /* will start next search from here */
  return name;
}


/*
** Given a path such as ";blabla.so;blublu.so", pushes the string
**
** no file 'blabla.so'
**	no file 'blublu.so'
*/
static void pusherrornotfound (lua55_State *L, const char *path) {
  luaL_Buffer b;
  lua55L_buffinit(L, &b);
  lua55L_addstring(&b, "no file '");
  lua55L_addgsub(&b, path, LUA_PATH_SEP, "'\n\tno file '");
  lua55L_addstring(&b, "'");
  lua55L_pushresult(&b);
}


static const char *searchpath (lua55_State *L, const char *name,
                                             const char *path,
                                             const char *sep,
                                             const char *dirsep) {
  luaL_Buffer buff;
  char *pathname;  /* path with name inserted */
  char *endpathname;  /* its end */
  const char *filename;
  /* separator is non-empty and appears in 'name'? */
  if (*sep != '\0' && strchr(name, *sep) != NULL)
    name = lua55L_gsub(L, name, sep, dirsep);  /* replace it by 'dirsep' */
  lua55L_buffinit(L, &buff);
  /* add path to the buffer, replacing marks ('?') with the file name */
  lua55L_addgsub(&buff, path, LUA_PATH_MARK, name);
  lua55L_addchar(&buff, '\0');
  pathname = lua55L_buffaddr(&buff);  /* writable list of file names */
  endpathname = pathname + lua55L_bufflen(&buff) - 1;
  while ((filename = getnextfilename(&pathname, endpathname)) != NULL) {
    if (readable(filename))  /* does file exist and is readable? */
      return lua55_pushstring(L, filename);  /* save and return name */
  }
  lua55L_pushresult(&buff);  /* push path to create error message */
  pusherrornotfound(L, lua55_tostring(L, -1));  /* create error message */
  return NULL;  /* not found */
}


static int ll_searchpath (lua55_State *L) {
  const char *f = searchpath(L, lua55L_checkstring(L, 1),
                                lua55L_checkstring(L, 2),
                                lua55L_optstring(L, 3, "."),
                                lua55L_optstring(L, 4, LUA_DIRSEP));
  if (f != NULL) return 1;
  else {  /* error message is on top of the stack */
    lua55L_pushfail(L);
    lua55_insert(L, -2);
    return 2;  /* return fail + error message */
  }
}


static const char *findfile (lua55_State *L, const char *name,
                                           const char *pname,
                                           const char *dirsep) {
  const char *path;
  lua55_getfield(L, lua55_upvalueindex(1), pname);
  path = lua55_tostring(L, -1);
  if (l_unlikely(path == NULL))
    lua55L_error(L, "'package.%s' must be a string", pname);
  return searchpath(L, name, path, ".", dirsep);
}


static int checkload (lua55_State *L, int stat, const char *filename) {
  if (l_likely(stat)) {  /* module loaded successfully? */
    lua55_pushstring(L, filename);  /* will be 2nd argument to module */
    return 2;  /* return open function and file name */
  }
  else
    return lua55L_error(L, "error loading module '%s' from file '%s':\n\t%s",
                          lua55_tostring(L, 1), filename, lua55_tostring(L, -1));
}


static int searcher_Lua (lua55_State *L) {
  const char *filename;
  const char *name = lua55L_checkstring(L, 1);
  filename = findfile(L, name, "path", LUA_LSUBSEP);
  if (filename == NULL) return 1;  /* module not found in this path */
  return checkload(L, (lua55L_loadfile(L, filename) == LUA_OK), filename);
}


/*
** Try to find a load function for module 'modname' at file 'filename'.
** First, change '.' to '_' in 'modname'; then, if 'modname' has
** the form X-Y (that is, it has an "ignore mark"), build a function
** name "luaopen_X" and look for it. (For compatibility, if that
** fails, it also tries "luaopen_Y".) If there is no ignore mark,
** look for a function named "luaopen_modname".
*/
static int loadfunc (lua55_State *L, const char *filename, const char *modname) {
  const char *openfunc;
  const char *mark;
  modname = lua55L_gsub(L, modname, ".", LUA_OFSEP);
  mark = strchr(modname, *LUA_IGMARK);
  if (mark) {
    int stat;
    openfunc = lua55_pushlstring(L, modname, ct_diff2sz(mark - modname));
    openfunc = lua55_pushfstring(L, LUA_POF"%s", openfunc);
    stat = lookforfunc(L, filename, openfunc);
    if (stat != ERRFUNC) return stat;
    modname = mark + 1;  /* else go ahead and try old-style name */
  }
  openfunc = lua55_pushfstring(L, LUA_POF"%s", modname);
  return lookforfunc(L, filename, openfunc);
}


static int searcher_C (lua55_State *L) {
  const char *name = lua55L_checkstring(L, 1);
  const char *filename = findfile(L, name, "cpath", LUA_CSUBSEP);
  if (filename == NULL) return 1;  /* module not found in this path */
  return checkload(L, (loadfunc(L, filename, name) == 0), filename);
}


static int searcher_Croot (lua55_State *L) {
  const char *filename;
  const char *name = lua55L_checkstring(L, 1);
  const char *p = strchr(name, '.');
  int stat;
  if (p == NULL) return 0;  /* is root */
  lua55_pushlstring(L, name, ct_diff2sz(p - name));
  filename = findfile(L, lua55_tostring(L, -1), "cpath", LUA_CSUBSEP);
  if (filename == NULL) return 1;  /* root not found */
  if ((stat = loadfunc(L, filename, name)) != 0) {
    if (stat != ERRFUNC)
      return checkload(L, 0, filename);  /* real error */
    else {  /* open function not found */
      lua55_pushfstring(L, "no module '%s' in file '%s'", name, filename);
      return 1;
    }
  }
  lua55_pushstring(L, filename);  /* will be 2nd argument to module */
  return 2;
}


static int searcher_preload (lua55_State *L) {
  const char *name = lua55L_checkstring(L, 1);
  lua55_getfield(L, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);
  if (lua55_getfield(L, -1, name) == LUA_TNIL) {  /* not found? */
    lua55_pushfstring(L, "no field package.preload['%s']", name);
    return 1;
  }
  else {
    lua55_pushliteral(L, ":preload:");
    return 2;
  }
}


static void findloader (lua55_State *L, const char *name) {
  int i;
  luaL_Buffer msg;  /* to build error message */
  /* push 'package.searchers' to index 3 in the stack */
  if (l_unlikely(lua55_getfield(L, lua55_upvalueindex(1), "searchers")
                 != LUA_TTABLE))
    lua55L_error(L, "'package.searchers' must be a table");
  lua55L_buffinit(L, &msg);
  lua55L_addstring(&msg, "\n\t");  /* error-message prefix for first message */
  /*  iterate over available searchers to find a loader */
  for (i = 1; ; i++) {
    if (l_unlikely(lua55_rawgeti(L, 3, i) == LUA_TNIL)) {  /* no more searchers? */
      lua55_pop(L, 1);  /* remove nil */
      lua55L_buffsub(&msg, 2);  /* remove last prefix */
      lua55L_pushresult(&msg);  /* create error message */
      lua55L_error(L, "module '%s' not found:%s", name, lua55_tostring(L, -1));
    }
    lua55_pushstring(L, name);
    lua55_call(L, 1, 2);  /* call it */
    if (lua55_isfunction(L, -2))  /* did it find a loader? */
      return;  /* module loader found */
    else if (lua55_isstring(L, -2)) {  /* searcher returned error message? */
      lua55_pop(L, 1);  /* remove extra return */
      lua55L_addvalue(&msg);  /* concatenate error message */
      lua55L_addstring(&msg, "\n\t");  /* prefix for next message */
    }
    else  /* no error message */
      lua55_pop(L, 2);  /* remove both returns */
  }
}


static int ll_require (lua55_State *L) {
  const char *name = lua55L_checkstring(L, 1);
  lua55_settop(L, 1);  /* LOADED table will be at index 2 */
  lua55_getfield(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
  lua55_getfield(L, 2, name);  /* LOADED[name] */
  if (lua55_toboolean(L, -1))  /* is it there? */
    return 1;  /* package is already loaded */
  /* else must load package */
  lua55_pop(L, 1);  /* remove 'getfield' result */
  findloader(L, name);
  lua55_rotate(L, -2, 1);  /* function <-> loader data */
  lua55_pushvalue(L, 1);  /* name is 1st argument to module loader */
  lua55_pushvalue(L, -3);  /* loader data is 2nd argument */
  /* stack: ...; loader data; loader function; mod. name; loader data */
  lua55_call(L, 2, 1);  /* run loader to load module */
  /* stack: ...; loader data; result from loader */
  if (!lua55_isnil(L, -1))  /* non-nil return? */
    lua55_setfield(L, 2, name);  /* LOADED[name] = returned value */
  else
    lua55_pop(L, 1);  /* pop nil */
  if (lua55_getfield(L, 2, name) == LUA_TNIL) {   /* module set no value? */
    lua55_pushboolean(L, 1);  /* use true as result */
    lua55_copy(L, -1, -2);  /* replace loader result */
    lua55_setfield(L, 2, name);  /* LOADED[name] = true */
  }
  lua55_rotate(L, -2, 1);  /* loader data <-> module result  */
  return 2;  /* return module result and loader data */
}

/* }====================================================== */




static const luaL_Reg pk_funcs[] = {
  {"loadlib", ll_loadlib},
  {"searchpath", ll_searchpath},
  /* placeholders */
  {"preload", NULL},
  {"cpath", NULL},
  {"path", NULL},
  {"searchers", NULL},
  {"loaded", NULL},
  {NULL, NULL}
};


static const luaL_Reg ll_funcs[] = {
  {"require", ll_require},
  {NULL, NULL}
};


static void createsearcherstable (lua55_State *L) {
  static const lua_CFunction searchers[] = {
    searcher_preload,
    searcher_Lua,
    searcher_C,
    searcher_Croot,
    NULL
  };
  int i;
  /* create 'searchers' table */
  lua55_createtable(L, sizeof(searchers)/sizeof(searchers[0]) - 1, 0);
  /* fill it with predefined searchers */
  for (i=0; searchers[i] != NULL; i++) {
    lua55_pushvalue(L, -2);  /* set 'package' as upvalue for all searchers */
    lua55_pushcclosure(L, searchers[i], 1);
    lua55_rawseti(L, -2, i+1);
  }
  lua55_setfield(L, -2, "searchers");  /* put it in field 'searchers' */
}


LUAMOD_API int lua55open_package (lua55_State *L) {
  lua55L_getsubtable(L, LUA_REGISTRYINDEX, CLIBS);  /* create CLIBS table */
  lua55_pop(L, 1);  /* will not use it now */
  lua55L_newlib(L, pk_funcs);  /* create 'package' table */
  createsearcherstable(L);
  /* set paths */
  setpath(L, "path", LUA_PATH_VAR, LUA_PATH_DEFAULT);
  setpath(L, "cpath", LUA_CPATH_VAR, LUA_CPATH_DEFAULT);
  /* store config information */
  lua55_pushliteral(L, LUA_DIRSEP "\n" LUA_PATH_SEP "\n" LUA_PATH_MARK "\n"
                     LUA_EXEC_DIR "\n" LUA_IGMARK "\n");
  lua55_setfield(L, -2, "config");
  /* set field 'loaded' */
  lua55L_getsubtable(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
  lua55_setfield(L, -2, "loaded");
  /* set field 'preload' */
  lua55L_getsubtable(L, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);
  lua55_setfield(L, -2, "preload");
  lua55_pushglobaltable(L);
  lua55_pushvalue(L, -2);  /* set 'package' as upvalue for next lib */
  lua55L_setfuncs(L, ll_funcs, 1);  /* open lib into global table */
  lua55_pop(L, 1);  /* pop global table */
  return 1;  /* return 'package' table */
}

