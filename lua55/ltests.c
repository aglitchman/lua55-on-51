/*
** $Id: ltests.c $
** Internal Module for Debugging of the Lua Implementation
** See Copyright Notice in lua.h
*/

#define ltests_c
#define LUA_CORE

#include "lprefix.h"


#include <limits.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"

#include "lapi.h"
#include "lauxlib.h"
#include "lcode.h"
#include "lctype.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lmem.h"
#include "lopcodes.h"
#include "lopnames.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "lualib.h"



/*
** The whole module only makes sense with LUA_DEBUG on
*/
#if defined(LUA_DEBUG)


void *l_Trick = 0;


#define obj_at(L,k)	s2v(L->ci->func.p + (k))


static int runC (lua55_State *L, lua55_State *L1, const char *pc);


static void setnameval (lua55_State *L, const char *name, int val) {
  lua55_pushinteger(L, val);
  lua55_setfield(L, -2, name);
}


static void pushobject (lua55_State *L, const TValue *o) {
  setobj2s(L, L->top.p, o);
  api_incr_top(L);
}


static void badexit (const char *fmt, const char *s1, const char *s2) {
  fprintf(stderr, fmt, s1);
  if (s2)
    fprintf(stderr, "extra info: %s\n", s2);
  /* avoid assertion failures when exiting */
  l_memcontrol.numblocks = l_memcontrol.total = 0;
  exit(EXIT_FAILURE);
}


static int tpanic (lua55_State *L) {
  const char *msg = (lua55_type(L, -1) == LUA_TSTRING)
                  ? lua55_tostring(L, -1)
                  : "error object is not a string";
  return (badexit("PANIC: unprotected error in call to Lua API (%s)\n",
                   msg, NULL),
          0);  /* do not return to Lua */
}


/*
** Warning function for tests. First, it concatenates all parts of
** a warning in buffer 'buff'. Then, it has three modes:
** - 0.normal: messages starting with '#' are shown on standard output;
** - other messages abort the tests (they represent real warning
** conditions; the standard tests should not generate these conditions
** unexpectedly);
** - 1.allow: all messages are shown;
** - 2.store: all warnings go to the global '_WARN';
*/
static void warnf (void *ud, const char *msg, int tocont) {
  lua55_State *L = cast(lua55_State *, ud);
  static char buff[200] = "";  /* should be enough for tests... */
  static int onoff = 0;
  static int mode = 0;  /* start in normal mode */
  static int lasttocont = 0;
  if (!lasttocont && !tocont && *msg == '@') {  /* control message? */
    if (buff[0] != '\0')
      badexit("Control warning during warning: %s\naborting...\n", msg, buff);
    if (strcmp(msg, "@off") == 0)
      onoff = 0;
    else if (strcmp(msg, "@on") == 0)
      onoff = 1;
    else if (strcmp(msg, "@normal") == 0)
      mode = 0;
    else if (strcmp(msg, "@allow") == 0)
      mode = 1;
    else if (strcmp(msg, "@store") == 0)
      mode = 2;
    else
      badexit("Invalid control warning in test mode: %s\naborting...\n",
              msg, NULL);
    return;
  }
  lasttocont = tocont;
  if (strlen(msg) >= sizeof(buff) - strlen(buff))
    badexit("warnf-buffer overflow (%s)\n", msg, buff);
  strcat(buff, msg);  /* add new message to current warning */
  if (!tocont) {  /* message finished? */
    lua_unlock(L);
    lua55L_checkstack(L, 1, "warn stack space");
    lua55_getglobal(L, "_WARN");
    if (!lua55_toboolean(L, -1))
      lua55_pop(L, 1);  /* ok, no previous unexpected warning */
    else {
      badexit("Unhandled warning in store mode: %s\naborting...\n",
              lua55_tostring(L, -1), buff);
    }
    lua_lock(L);
    switch (mode) {
      case 0: {  /* normal */
        if (buff[0] != '#' && onoff)  /* unexpected warning? */
          badexit("Unexpected warning in test mode: %s\naborting...\n",
                  buff, NULL);
      }  /* FALLTHROUGH */
      case 1: {  /* allow */
        if (onoff)
          fprintf(stderr, "Lua warning: %s\n", buff);  /* print warning */
        break;
      }
      case 2: {  /* store */
        lua_unlock(L);
        lua55L_checkstack(L, 1, "warn stack space");
        lua55_pushstring(L, buff);
        lua55_setglobal(L, "_WARN");  /* assign message to global '_WARN' */
        lua_lock(L);
        break;
      }
    }
    buff[0] = '\0';  /* prepare buffer for next warning */
  }
}


/*
** {======================================================================
** Controlled version for realloc.
** =======================================================================
*/

#define MARK		0x55  /* 01010101 (a nice pattern) */

typedef union memHeader {
  LUAI_MAXALIGN;
  struct {
    size_t size;
    int type;
  } d;
} memHeader;


#if !defined(EXTERNMEMCHECK)

/* full memory check */
#define MARKSIZE	16  /* size of marks after each block */
#define fillmem(mem,size)	memset(mem, -MARK, size)

#else

/* external memory check: don't do it twice */
#define MARKSIZE	0
#define fillmem(mem,size)	/* empty */

#endif


Memcontrol l_memcontrol =
  {0, 0UL, 0UL, 0UL, 0UL, (~0UL),
   {0UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0UL}};


static void freeblock (Memcontrol *mc, memHeader *block) {
  if (block) {
    size_t size = block->d.size;
    int i;
    for (i = 0; i < MARKSIZE; i++)  /* check marks after block */
      lua_assert(*(cast_charp(block + 1) + size + i) == MARK);
    mc->objcount[block->d.type]--;
    fillmem(block, sizeof(memHeader) + size + MARKSIZE);  /* erase block */
    free(block);  /* actually free block */
    mc->numblocks--;  /* update counts */
    mc->total -= size;
  }
}


void *debug_realloc (void *ud, void *b, size_t oldsize, size_t size) {
  Memcontrol *mc = cast(Memcontrol *, ud);
  memHeader *block = cast(memHeader *, b);
  int type;
  if (mc->memlimit == 0) {  /* first time? */
    char *limit = getenv("MEMLIMIT");  /* initialize memory limit */
    mc->memlimit = limit ? strtoul(limit, NULL, 10) : ULONG_MAX;
  }
  if (block == NULL) {
    type = (oldsize < LUA_NUMTYPES) ? cast_int(oldsize) : 0;
    oldsize = 0;
  }
  else {
    block--;  /* go to real header */
    type = block->d.type;
    lua_assert(oldsize == block->d.size);
  }
  if (size == 0) {
    freeblock(mc, block);
    return NULL;
  }
  if (mc->failnext) {
    mc->failnext = 0;
    return NULL;  /* fake a single memory allocation error */
  }
  if (mc->countlimit != ~0UL && size != oldsize) {  /* count limit in use? */
    if (mc->countlimit == 0)
      return NULL;  /* fake a memory allocation error */
    mc->countlimit--;
  }
  if (size > oldsize && mc->total+size-oldsize > mc->memlimit)
    return NULL;  /* fake a memory allocation error */
  else {
    memHeader *newblock;
    int i;
    size_t commonsize = (oldsize < size) ? oldsize : size;
    size_t realsize = sizeof(memHeader) + size + MARKSIZE;
    if (realsize < size) return NULL;  /* arithmetic overflow! */
    newblock = cast(memHeader *, malloc(realsize));  /* alloc a new block */
    if (newblock == NULL)
      return NULL;  /* really out of memory? */
    if (block) {
      memcpy(newblock + 1, block + 1, commonsize);  /* copy old contents */
      freeblock(mc, block);  /* erase (and check) old copy */
    }
    /* initialize new part of the block with something weird */
    fillmem(cast_charp(newblock + 1) + commonsize, size - commonsize);
    /* initialize marks after block */
    for (i = 0; i < MARKSIZE; i++)
      *(cast_charp(newblock + 1) + size + i) = MARK;
    newblock->d.size = size;
    newblock->d.type = type;
    mc->total += size;
    if (mc->total > mc->maxmem)
      mc->maxmem = mc->total;
    mc->numblocks++;
    mc->objcount[type]++;
    return newblock + 1;
  }
}


/* }====================================================================== */



/*
** {=====================================================================
** Functions to check memory consistency.
** Most of these checks are done through asserts, so this code does
** not make sense with asserts off. For this reason, it uses 'assert'
** directly, instead of 'lua_assert'.
** ======================================================================
*/

#include <assert.h>

/*
** Check GC invariants. For incremental mode, a black object cannot
** point to a white one. For generational mode, really old objects
** cannot point to young objects. Both old1 and touched2 objects
** cannot point to new objects (but can point to survivals).
** (Threads and open upvalues, despite being marked "really old",
** continue to be visited in all collections, and therefore can point to
** new objects. They, and only they, are old but gray.)
*/
static int testobjref1 (global_State *g, GCObject *f, GCObject *t) {
  if (isdead(g,t)) return 0;
  if (issweepphase(g))
    return 1;  /* no invariants */
  else if (g->gckind != KGC_GENMINOR)
    return !(isblack(f) && iswhite(t));  /* basic incremental invariant */
  else {  /* generational mode */
    if ((getage(f) == G_OLD && isblack(f)) && !isold(t))
      return 0;
    if ((getage(f) == G_OLD1 || getage(f) == G_TOUCHED2) &&
         getage(t) == G_NEW)
      return 0;
    return 1;
  }
}


static void printobj (global_State *g, GCObject *o) {
  printf("||%s(%p)-%c%c(%02X)||",
           ttypename(novariant(o->tt)), (void *)o,
           isdead(g,o) ? 'd' : isblack(o) ? 'b' : iswhite(o) ? 'w' : 'g',
           "ns01oTt"[getage(o)], o->marked);
  if (o->tt == LUA_VSHRSTR || o->tt == LUA_VLNGSTR)
    printf(" '%s'", getstr(gco2ts(o)));
}


void lua_printobj (lua55_State *L, struct GCObject *o) {
  printobj(G(L), o);
}


void lua_printvalue (TValue *v) {
  switch (ttypetag(v)) {
    case LUA_VNUMINT: case LUA_VNUMFLT: {
      char buff[LUA_N2SBUFFSZ];
      unsigned len = luaO_tostringbuff(v, buff);
      buff[len] = '\0';
      printf("%s", buff);
      break;
    }
    case LUA_VSHRSTR:
      printf("'%s'", getstr(tsvalue(v))); break;
    case LUA_VLNGSTR:
      printf("'%.30s...'", getstr(tsvalue(v))); break;
    case LUA_VFALSE:
      printf("%s", "false"); break;
    case LUA_VTRUE:
      printf("%s", "true"); break;
    case LUA_VLIGHTUSERDATA:
      printf("light udata: %p", pvalue(v)); break;
    case LUA_VUSERDATA:
      printf("full udata: %p", uvalue(v)); break;
    case LUA_VNIL:
      printf("nil"); break;
    case LUA_VLCF:
      printf("light C function: %p", fvalue(v)); break;
    case LUA_VCCL:
      printf("C closure: %p", clCvalue(v)); break;
    case LUA_VLCL:
      printf("Lua function: %p", clLvalue(v)); break;
    case LUA_VTHREAD:
      printf("thread: %p", thvalue(v)); break;
    case LUA_VTABLE:
      printf("table: %p", hvalue(v)); break;
    default:
      lua_assert(0);
  }
}


static int testobjref (global_State *g, GCObject *f, GCObject *t) {
  int r1 = testobjref1(g, f, t);
  if (!r1) {
    printf("%d(%02X) - ", g->gcstate, g->currentwhite);
    printobj(g, f);
    printf("  ->  ");
    printobj(g, t);
    printf("\n");
  }
  return r1;
}


static void checkobjref (global_State *g, GCObject *f, GCObject *t) {
    assert(testobjref(g, f, t));
}


/*
** Version where 't' can be NULL. In that case, it should not apply the
** macro 'obj2gco' over the object. ('t' may have several types, so this
** definition must be a macro.)  Most checks need this version, because
** the check may run while an object is still being created.
*/
#define checkobjrefN(g,f,t)	{ if (t) checkobjref(g,f,obj2gco(t)); }


static void checkvalref (global_State *g, GCObject *f, const TValue *t) {
  assert(!iscollectable(t) || (righttt(t) && testobjref(g, f, gcvalue(t))));
}


static void checktable (global_State *g, Table *h) {
  unsigned int i;
  unsigned int asize = h->asize;
  Node *n, *limit = gnode(h, sizenode(h));
  GCObject *hgc = obj2gco(h);
  checkobjrefN(g, hgc, h->metatable);
  for (i = 0; i < asize; i++) {
    TValue aux;
    arr2obj(h, i, &aux);
    checkvalref(g, hgc, &aux);
  }
  for (n = gnode(h, 0); n < limit; n++) {
    if (!isempty(gval(n))) {
      TValue k;
      getnodekey(mainthread(g), &k, n);
      assert(!keyisnil(n));
      checkvalref(g, hgc, &k);
      checkvalref(g, hgc, gval(n));
    }
  }
}


static void checkudata (global_State *g, Udata *u) {
  int i;
  GCObject *hgc = obj2gco(u);
  checkobjrefN(g, hgc, u->metatable);
  for (i = 0; i < u->nuvalue; i++)
    checkvalref(g, hgc, &u->uv[i].uv);
}


static void checkproto (global_State *g, Proto *f) {
  int i;
  GCObject *fgc = obj2gco(f);
  checkobjrefN(g, fgc, f->source);
  for (i=0; i<f->sizek; i++) {
    if (iscollectable(f->k + i))
      checkobjref(g, fgc, gcvalue(f->k + i));
  }
  for (i=0; i<f->sizeupvalues; i++)
    checkobjrefN(g, fgc, f->upvalues[i].name);
  for (i=0; i<f->sizep; i++)
    checkobjrefN(g, fgc, f->p[i]);
  for (i=0; i<f->sizelocvars; i++)
    checkobjrefN(g, fgc, f->locvars[i].varname);
}


static void checkCclosure (global_State *g, CClosure *cl) {
  GCObject *clgc = obj2gco(cl);
  int i;
  for (i = 0; i < cl->nupvalues; i++)
    checkvalref(g, clgc, &cl->upvalue[i]);
}


static void checkLclosure (global_State *g, LClosure *cl) {
  GCObject *clgc = obj2gco(cl);
  int i;
  checkobjrefN(g, clgc, cl->p);
  for (i=0; i<cl->nupvalues; i++) {
    UpVal *uv = cl->upvals[i];
    if (uv) {
      checkobjrefN(g, clgc, uv);
      if (!upisopen(uv))
        checkvalref(g, obj2gco(uv), uv->v.p);
    }
  }
}


static int lua_checkpc (CallInfo *ci) {
  if (!isLua(ci)) return 1;
  else {
    StkId f = ci->func.p;
    Proto *p = clLvalue(s2v(f))->p;
    return p->code <= ci->u.l.savedpc &&
           ci->u.l.savedpc <= p->code + p->sizecode;
  }
}


static void check_stack (global_State *g, lua55_State *L1) {
  StkId o;
  CallInfo *ci;
  UpVal *uv;
  assert(!isdead(g, L1));
  if (L1->stack.p == NULL) {  /* incomplete thread? */
    assert(L1->openupval == NULL && L1->ci == NULL);
    return;
  }
  for (uv = L1->openupval; uv != NULL; uv = uv->u.open.next)
    assert(upisopen(uv));  /* must be open */
  assert(L1->top.p <= L1->stack_last.p);
  assert(L1->tbclist.p <= L1->top.p);
  for (ci = L1->ci; ci != NULL; ci = ci->previous) {
    assert(ci->top.p <= L1->stack_last.p);
    assert(lua_checkpc(ci));
  }
  for (o = L1->stack.p; o < L1->stack_last.p; o++)
    checkliveness(L1, s2v(o));  /* entire stack must have valid values */
}


static void checkrefs (global_State *g, GCObject *o) {
  switch (o->tt) {
    case LUA_VUSERDATA: {
      checkudata(g, gco2u(o));
      break;
    }
    case LUA_VUPVAL: {
      checkvalref(g, o, gco2upv(o)->v.p);
      break;
    }
    case LUA_VTABLE: {
      checktable(g, gco2t(o));
      break;
    }
    case LUA_VTHREAD: {
      check_stack(g, gco2th(o));
      break;
    }
    case LUA_VLCL: {
      checkLclosure(g, gco2lcl(o));
      break;
    }
    case LUA_VCCL: {
      checkCclosure(g, gco2ccl(o));
      break;
    }
    case LUA_VPROTO: {
      checkproto(g, gco2p(o));
      break;
    }
    case LUA_VSHRSTR:
    case LUA_VLNGSTR: {
      assert(!isgray(o));  /* strings are never gray */
      break;
    }
    default: assert(0);
  }
}


/*
** Check consistency of an object:
** - Dead objects can only happen in the 'allgc' list during a sweep
** phase (controlled by the caller through 'maybedead').
** - During pause, all objects must be white.
** - In generational mode:
**   * objects must be old enough for their lists ('listage').
**   * old objects cannot be white.
**   * old objects must be black, except for 'touched1', 'old0',
**     threads, and open upvalues.
**   * 'touched1' objects must be gray.
*/
static void checkobject (global_State *g, GCObject *o, int maybedead,
                         int listage) {
  if (isdead(g, o))
    assert(maybedead);
  else {
    assert(g->gcstate != GCSpause || iswhite(o));
    if (g->gckind == KGC_GENMINOR) {  /* generational mode? */
      assert(getage(o) >= listage);
      if (isold(o)) {
        assert(!iswhite(o));
        assert(isblack(o) ||
        getage(o) == G_TOUCHED1 ||
        getage(o) == G_OLD0 ||
        o->tt == LUA_VTHREAD ||
        (o->tt == LUA_VUPVAL && upisopen(gco2upv(o))));
      }
      assert(getage(o) != G_TOUCHED1 || isgray(o));
    }
    checkrefs(g, o);
  }
}


static l_mem checkgraylist (global_State *g, GCObject *o) {
  int total = 0;  /* count number of elements in the list */
  cast_void(g);  /* better to keep it if we need to print an object */
  while (o) {
    assert(!!isgray(o) ^ (getage(o) == G_TOUCHED2));
    assert(!testbit(o->marked, TESTBIT));
    if (keepinvariant(g))
      l_setbit(o->marked, TESTBIT);  /* mark that object is in a gray list */
    total++;
    switch (o->tt) {
      case LUA_VTABLE: o = gco2t(o)->gclist; break;
      case LUA_VLCL: o = gco2lcl(o)->gclist; break;
      case LUA_VCCL: o = gco2ccl(o)->gclist; break;
      case LUA_VTHREAD: o = gco2th(o)->gclist; break;
      case LUA_VPROTO: o = gco2p(o)->gclist; break;
      case LUA_VUSERDATA:
        assert(gco2u(o)->nuvalue > 0);
        o = gco2u(o)->gclist;
        break;
      default: assert(0);  /* other objects cannot be in a gray list */
    }
  }
  return total;
}


/*
** Check objects in gray lists.
*/
static l_mem checkgrays (global_State *g) {
  l_mem total = 0;  /* count number of elements in all lists */
  if (!keepinvariant(g)) return total;
  total += checkgraylist(g, g->gray);
  total += checkgraylist(g, g->grayagain);
  total += checkgraylist(g, g->weak);
  total += checkgraylist(g, g->allweak);
  total += checkgraylist(g, g->ephemeron);
  return total;
}


/*
** Check whether 'o' should be in a gray list. If so, increment
** 'count' and check its TESTBIT. (It must have been previously set by
** 'checkgraylist'.)
*/
static void incifingray (global_State *g, GCObject *o, l_mem *count) {
  if (!keepinvariant(g))
    return;  /* gray lists not being kept in these phases */
  if (o->tt == LUA_VUPVAL) {
    /* only open upvalues can be gray */
    assert(!isgray(o) || upisopen(gco2upv(o)));
    return;  /* upvalues are never in gray lists */
  }
  /* these are the ones that must be in gray lists */
  if (isgray(o) || getage(o) == G_TOUCHED2) {
    (*count)++;
    assert(testbit(o->marked, TESTBIT));
    resetbit(o->marked, TESTBIT);  /* prepare for next cycle */
  }
}


static l_mem checklist (global_State *g, int maybedead, int tof,
  GCObject *newl, GCObject *survival, GCObject *old, GCObject *reallyold) {
  GCObject *o;
  l_mem total = 0;  /* number of object that should be in  gray lists */
  for (o = newl; o != survival; o = o->next) {
    checkobject(g, o, maybedead, G_NEW);
    incifingray(g, o, &total);
    assert(!tof == !tofinalize(o));
  }
  for (o = survival; o != old; o = o->next) {
    checkobject(g, o, 0, G_SURVIVAL);
    incifingray(g, o, &total);
    assert(!tof == !tofinalize(o));
  }
  for (o = old; o != reallyold; o = o->next) {
    checkobject(g, o, 0, G_OLD1);
    incifingray(g, o, &total);
    assert(!tof == !tofinalize(o));
  }
  for (o = reallyold; o != NULL; o = o->next) {
    checkobject(g, o, 0, G_OLD);
    incifingray(g, o, &total);
    assert(!tof == !tofinalize(o));
  }
  return total;
}


int lua_checkmemory (lua55_State *L) {
  global_State *g = G(L);
  GCObject *o;
  int maybedead;
  l_mem totalin;  /* total of objects that are in gray lists */
  l_mem totalshould;  /* total of objects that should be in gray lists */
  if (keepinvariant(g)) {
    assert(!iswhite(mainthread(g)));
    assert(!iswhite(gcvalue(&g->l_registry)));
  }
  assert(!isdead(g, gcvalue(&g->l_registry)));
  assert(g->sweepgc == NULL || issweepphase(g));
  totalin = checkgrays(g);

  /* check 'fixedgc' list */
  for (o = g->fixedgc; o != NULL; o = o->next) {
    assert(o->tt == LUA_VSHRSTR && isgray(o) && getage(o) == G_OLD);
  }

  /* check 'allgc' list */
  maybedead = (GCSatomic < g->gcstate && g->gcstate <= GCSswpallgc);
  totalshould = checklist(g, maybedead, 0, g->allgc,
                             g->survival, g->old1, g->reallyold);

  /* check 'finobj' list */
  totalshould += checklist(g, 0, 1, g->finobj,
                              g->finobjsur, g->finobjold1, g->finobjrold);

  /* check 'tobefnz' list */
  for (o = g->tobefnz; o != NULL; o = o->next) {
    checkobject(g, o, 0, G_NEW);
    incifingray(g, o, &totalshould);
    assert(tofinalize(o));
    assert(o->tt == LUA_VUSERDATA || o->tt == LUA_VTABLE);
  }
  if (keepinvariant(g))
    assert(totalin == totalshould);
  return 0;
}

/* }====================================================== */



/*
** {======================================================
** Disassembler
** =======================================================
*/


static char *buildop (Proto *p, int pc, char *buff) {
  char *obuff = buff;
  Instruction i = p->code[pc];
  OpCode o = GET_OPCODE(i);
  const char *name = opnames[o];
  int line = luaG_getfuncline(p, pc);
  int lineinfo = (p->lineinfo != NULL) ? p->lineinfo[pc] : 0;
  if (lineinfo == ABSLINEINFO)
    buff += sprintf(buff, "(__");
  else
    buff += sprintf(buff, "(%2d", lineinfo);
  buff += sprintf(buff, " - %4d) %4d - ", line, pc);
  switch (getOpMode(o)) {
    case iABC:
      sprintf(buff, "%-12s%4d %4d %4d%s", name,
              GETARG_A(i), GETARG_B(i), GETARG_C(i),
              GETARG_k(i) ? " (k)" : "");
      break;
    case ivABC:
      sprintf(buff, "%-12s%4d %4d %4d%s", name,
              GETARG_A(i), GETARG_vB(i), GETARG_vC(i),
              GETARG_k(i) ? " (k)" : "");
      break;
    case iABx:
      sprintf(buff, "%-12s%4d %4d", name, GETARG_A(i), GETARG_Bx(i));
      break;
    case iAsBx:
      sprintf(buff, "%-12s%4d %4d", name, GETARG_A(i), GETARG_sBx(i));
      break;
    case iAx:
      sprintf(buff, "%-12s%4d", name, GETARG_Ax(i));
      break;
    case isJ:
      sprintf(buff, "%-12s%4d", name, GETARG_sJ(i));
      break;
  }
  return obuff;
}


#if 0
void luaI_printcode (Proto *pt, int size) {
  int pc;
  for (pc=0; pc<size; pc++) {
    char buff[100];
    printf("%s\n", buildop(pt, pc, buff));
  }
  printf("-------\n");
}


void luaI_printinst (Proto *pt, int pc) {
  char buff[100];
  printf("%s\n", buildop(pt, pc, buff));
}
#endif


static int listcode (lua55_State *L) {
  int pc;
  Proto *p;
  lua55L_argcheck(L, lua55_isfunction(L, 1) && !lua55_iscfunction(L, 1),
                 1, "Lua function expected");
  p = getproto(obj_at(L, 1));
  lua55_newtable(L);
  setnameval(L, "maxstack", p->maxstacksize);
  setnameval(L, "numparams", p->numparams);
  for (pc=0; pc<p->sizecode; pc++) {
    char buff[100];
    lua55_pushinteger(L, pc+1);
    lua55_pushstring(L, buildop(p, pc, buff));
    lua55_settable(L, -3);
  }
  return 1;
}


static int printcode (lua55_State *L) {
  int pc;
  Proto *p;
  lua55L_argcheck(L, lua55_isfunction(L, 1) && !lua55_iscfunction(L, 1),
                 1, "Lua function expected");
  p = getproto(obj_at(L, 1));
  printf("maxstack: %d\n", p->maxstacksize);
  printf("numparams: %d\n", p->numparams);
  for (pc=0; pc<p->sizecode; pc++) {
    char buff[100];
    printf("%s\n", buildop(p, pc, buff));
  }
  return 0;
}


static int listk (lua55_State *L) {
  Proto *p;
  int i;
  lua55L_argcheck(L, lua55_isfunction(L, 1) && !lua55_iscfunction(L, 1),
                 1, "Lua function expected");
  p = getproto(obj_at(L, 1));
  lua55_createtable(L, p->sizek, 0);
  for (i=0; i<p->sizek; i++) {
    pushobject(L, p->k+i);
    lua55_rawseti(L, -2, i+1);
  }
  return 1;
}


static int listabslineinfo (lua55_State *L) {
  Proto *p;
  int i;
  lua55L_argcheck(L, lua55_isfunction(L, 1) && !lua55_iscfunction(L, 1),
                 1, "Lua function expected");
  p = getproto(obj_at(L, 1));
  lua55L_argcheck(L, p->abslineinfo != NULL, 1, "function has no debug info");
  lua55_createtable(L, 2 * p->sizeabslineinfo, 0);
  for (i=0; i < p->sizeabslineinfo; i++) {
    lua55_pushinteger(L, p->abslineinfo[i].pc);
    lua55_rawseti(L, -2, 2 * i + 1);
    lua55_pushinteger(L, p->abslineinfo[i].line);
    lua55_rawseti(L, -2, 2 * i + 2);
  }
  return 1;
}


static int listlocals (lua55_State *L) {
  Proto *p;
  int pc = cast_int(lua55L_checkinteger(L, 2)) - 1;
  int i = 0;
  const char *name;
  lua55L_argcheck(L, lua55_isfunction(L, 1) && !lua55_iscfunction(L, 1),
                 1, "Lua function expected");
  p = getproto(obj_at(L, 1));
  while ((name = luaF_getlocalname(p, ++i, pc)) != NULL)
    lua55_pushstring(L, name);
  return i-1;
}

/* }====================================================== */



void lua_printstack (lua55_State *L) {
  int i;
  int n = lua55_gettop(L);
  printf("stack: >>\n");
  for (i = 1; i <= n; i++) {
    printf("%3d: ", i);
    lua_printvalue(s2v(L->ci->func.p + i));
    printf("\n");
  }
  printf("<<\n");
}


int lua_printallstack (lua55_State *L) {
  StkId p;
  int i = 1;
  CallInfo *ci = &L->base_ci;
  printf("stack: >>\n");
  for (p = L->stack.p; p < L->top.p; p++) {
    if (ci != NULL && p == ci->func.p) {
      printf("  ---\n");
      if (ci == L->ci)
        ci = NULL;  /* printed last frame */
      else
        ci = ci->next;
    }
    printf("%3d: ", i++);
    lua_printvalue(s2v(p));
    printf("\n");
  }
  printf("<<\n");
  return 0;
}


static int get_limits (lua55_State *L) {
  lua55_createtable(L, 0, 5);
  setnameval(L, "IS32INT", LUAI_IS32INT);
  setnameval(L, "MAXARG_Ax", MAXARG_Ax);
  setnameval(L, "MAXARG_Bx", MAXARG_Bx);
  setnameval(L, "OFFSET_sBx", OFFSET_sBx);
  setnameval(L, "NUM_OPCODES", NUM_OPCODES);
  return 1;
}


static int get_sizes (lua55_State *L) {
  lua55_newtable(L);
  setnameval(L, "Lua state", sizeof(lua55_State));
  setnameval(L, "global state", sizeof(global_State));
  setnameval(L, "TValue", sizeof(TValue));
  setnameval(L, "Node", sizeof(Node));
  setnameval(L, "stack Value", sizeof(StackValue));
  return 1;
}


static int mem_query (lua55_State *L) {
  if (lua55_isnone(L, 1)) {
    lua55_pushinteger(L, cast_Integer(l_memcontrol.total));
    lua55_pushinteger(L, cast_Integer(l_memcontrol.numblocks));
    lua55_pushinteger(L, cast_Integer(l_memcontrol.maxmem));
    return 3;
  }
  else if (lua55_isnumber(L, 1)) {
    unsigned long limit = cast(unsigned long, lua55L_checkinteger(L, 1));
    if (limit == 0) limit = ULONG_MAX;
    l_memcontrol.memlimit = limit;
    return 0;
  }
  else {
    const char *t = lua55L_checkstring(L, 1);
    int i;
    for (i = LUA_NUMTYPES - 1; i >= 0; i--) {
      if (strcmp(t, ttypename(i)) == 0) {
        lua55_pushinteger(L, cast_Integer(l_memcontrol.objcount[i]));
        return 1;
      }
    }
    return lua55L_error(L, "unknown type '%s'", t);
  }
}


static int alloc_count (lua55_State *L) {
  if (lua55_isnone(L, 1))
    l_memcontrol.countlimit = cast(unsigned long, ~0L);
  else
    l_memcontrol.countlimit = cast(unsigned long, lua55L_checkinteger(L, 1));
  return 0;
}


static int alloc_failnext (lua55_State *L) {
  UNUSED(L);
  l_memcontrol.failnext = 1;
  return 0;
}


static int settrick (lua55_State *L) {
  if (ttisnil(obj_at(L, 1)))
    l_Trick = NULL;
  else
    l_Trick = gcvalue(obj_at(L, 1));
  return 0;
}


static int gc_color (lua55_State *L) {
  TValue *o;
  lua55L_checkany(L, 1);
  o = obj_at(L, 1);
  if (!iscollectable(o))
    lua55_pushstring(L, "no collectable");
  else {
    GCObject *obj = gcvalue(o);
    lua55_pushstring(L, isdead(G(L), obj) ? "dead" :
                      iswhite(obj) ? "white" :
                      isblack(obj) ? "black" : "gray");
  }
  return 1;
}


static int gc_age (lua55_State *L) {
  TValue *o;
  lua55L_checkany(L, 1);
  o = obj_at(L, 1);
  if (!iscollectable(o))
    lua55_pushstring(L, "no collectable");
  else {
    static const char *gennames[] = {"new", "survival", "old0", "old1",
                                     "old", "touched1", "touched2"};
    GCObject *obj = gcvalue(o);
    lua55_pushstring(L, gennames[getage(obj)]);
  }
  return 1;
}


static int gc_printobj (lua55_State *L) {
  TValue *o;
  lua55L_checkany(L, 1);
  o = obj_at(L, 1);
  if (!iscollectable(o))
    printf("no collectable\n");
  else {
    GCObject *obj = gcvalue(o);
    printobj(G(L), obj);
    printf("\n");
  }
  return 0;
}


static const char *const statenames[] = {
  "propagate", "enteratomic", "atomic", "sweepallgc", "sweepfinobj",
  "sweeptobefnz", "sweepend", "callfin", "pause", ""};

static int gc_state (lua55_State *L) {
  static const int states[] = {
    GCSpropagate, GCSenteratomic, GCSatomic, GCSswpallgc, GCSswpfinobj,
    GCSswptobefnz, GCSswpend, GCScallfin, GCSpause, -1};
  int option = states[lua55L_checkoption(L, 1, "", statenames)];
  global_State *g = G(L);
  if (option == -1) {
    lua55_pushstring(L, statenames[g->gcstate]);
    return 1;
  }
  else {
    if (g->gckind != KGC_INC)
      lua55L_error(L, "cannot change states in generational mode");
    lua_lock(L);
    if (option < g->gcstate) {  /* must cross 'pause'? */
      luaC_runtilstate(L, GCSpause, 1);  /* run until pause */
    }
    luaC_runtilstate(L, option, 0);  /* do not skip propagation state */
    lua_assert(g->gcstate == option);
    lua_unlock(L);
    return 0;
  }
}


static int tracinggc = 0;
void luai_tracegctest (lua55_State *L, int first) {
  if (!tracinggc) return;
  else {
    global_State *g = G(L);
    lua_unlock(L);
    g->gcstp = GCSTPGC;
    lua55_checkstack(L, 10);
    lua55_getfield(L, LUA_REGISTRYINDEX, "tracegc");
    lua55_pushboolean(L, first);
    lua55_call(L, 1, 0);
    g->gcstp = 0;
    lua_lock(L);
  }
}


static int tracegc (lua55_State *L) {
  if (lua55_isnil(L, 1))
    tracinggc = 0;
  else {
    tracinggc = 1;
    lua55_setfield(L, LUA_REGISTRYINDEX, "tracegc");
  }
  return 0;
}


static int hash_query (lua55_State *L) {
  if (lua55_isnone(L, 2)) {
    TString *ts;
    lua55L_argcheck(L, lua55_type(L, 1) == LUA_TSTRING, 1, "string expected");
    ts = tsvalue(obj_at(L, 1));
    if (ts->tt == LUA_VLNGSTR)
      luaS_hashlongstr(ts);  /* make sure long string has a hash */
    lua55_pushinteger(L, cast_int(ts->hash));
  }
  else {
    TValue *o = obj_at(L, 1);
    Table *t;
    lua55L_checktype(L, 2, LUA_TTABLE);
    t = hvalue(obj_at(L, 2));
    lua55_pushinteger(L, cast_Integer(luaH_mainposition(t, o) - t->node));
  }
  return 1;
}


static int stacklevel (lua55_State *L) {
  int a = 0;
  lua55_pushinteger(L, cast_Integer(L->top.p - L->stack.p));
  lua55_pushinteger(L, stacksize(L));
  lua55_pushinteger(L, cast_Integer(L->nCcalls));
  lua55_pushinteger(L, L->nci);
  lua55_pushinteger(L, (lua_Integer)(size_t)&a);
  return 5;
}


static int table_query (lua55_State *L) {
  const Table *t;
  int i = cast_int(lua55L_optinteger(L, 2, -1));
  unsigned int asize;
  lua55L_checktype(L, 1, LUA_TTABLE);
  t = hvalue(obj_at(L, 1));
  asize = t->asize;
  if (i == -1) {
    lua55_pushinteger(L, cast_Integer(asize));
    lua55_pushinteger(L, cast_Integer(allocsizenode(t)));
    lua55_pushinteger(L, cast_Integer(asize > 0 ? *lenhint(t) : 0));
    return 3;
  }
  else if (cast_uint(i) < asize) {
    lua55_pushinteger(L, i);
    if (!tagisempty(*getArrTag(t, i)))
      arr2obj(t, cast_uint(i), s2v(L->top.p));
    else
      setnilvalue(s2v(L->top.p));
    api_incr_top(L);
    lua55_pushnil(L);
  }
  else if (cast_uint(i -= cast_int(asize)) < sizenode(t)) {
    TValue k;
    getnodekey(L, &k, gnode(t, i));
    if (!isempty(gval(gnode(t, i))) ||
        ttisnil(&k) ||
        ttisnumber(&k)) {
      pushobject(L, &k);
    }
    else
      lua55_pushliteral(L, "<undef>");
    if (!isempty(gval(gnode(t, i))))
      pushobject(L, gval(gnode(t, i)));
    else
      lua55_pushnil(L);
    lua55_pushinteger(L, gnext(&t->node[i]));
  }
  return 3;
}


static int gc_query (lua55_State *L) {
  global_State *g = G(L);
  lua55_pushstring(L, g->gckind == KGC_INC ? "inc"
                  : g->gckind == KGC_GENMAJOR ? "genmajor"
                  : "genminor");
  lua55_pushstring(L, statenames[g->gcstate]);
  lua55_pushinteger(L, cast_st2S(gettotalbytes(g)));
  lua55_pushinteger(L, cast_st2S(g->GCdebt));
  lua55_pushinteger(L, cast_st2S(g->GCmarked));
  lua55_pushinteger(L, cast_st2S(g->GCmajorminor));
  return 6;
}


static int test_codeparam (lua55_State *L) {
  lua_Integer p = lua55L_checkinteger(L, 1);
  lua55_pushinteger(L, luaO_codeparam(cast_uint(p)));
  return 1;
}


static int test_applyparam (lua55_State *L) {
  lua_Integer p = lua55L_checkinteger(L, 1);
  lua_Integer x = lua55L_checkinteger(L, 2);
  lua55_pushinteger(L, cast_Integer(luaO_applyparam(cast_byte(p), x)));
  return 1;
}


static int string_query (lua55_State *L) {
  stringtable *tb = &G(L)->strt;
  int s = cast_int(lua55L_optinteger(L, 1, 0)) - 1;
  if (s == -1) {
    lua55_pushinteger(L ,tb->size);
    lua55_pushinteger(L ,tb->nuse);
    return 2;
  }
  else if (s < tb->size) {
    TString *ts;
    int n = 0;
    for (ts = tb->hash[s]; ts != NULL; ts = ts->u.hnext) {
      setsvalue2s(L, L->top.p, ts);
      api_incr_top(L);
      n++;
    }
    return n;
  }
  else return 0;
}


static int getreftable (lua55_State *L) {
  if (lua55_istable(L, 2))  /* is there a table as second argument? */
    return 2;  /* use it as the table */
  else
    return LUA_REGISTRYINDEX;  /* default is to use the register */
}


static int tref (lua55_State *L) {
  int t = getreftable(L);
  int level = lua55_gettop(L);
  lua55L_checkany(L, 1);
  lua55_pushvalue(L, 1);
  lua55_pushinteger(L, lua55L_ref(L, t));
  cast_void(level);  /* to avoid warnings */
  lua_assert(lua55_gettop(L) == level+1);  /* +1 for result */
  return 1;
}


static int getref (lua55_State *L) {
  int t = getreftable(L);
  int level = lua55_gettop(L);
  lua55_rawgeti(L, t, lua55L_checkinteger(L, 1));
  cast_void(level);  /* to avoid warnings */
  lua_assert(lua55_gettop(L) == level+1);
  return 1;
}

static int unref (lua55_State *L) {
  int t = getreftable(L);
  int level = lua55_gettop(L);
  lua55L_unref(L, t, cast_int(lua55L_checkinteger(L, 1)));
  cast_void(level);  /* to avoid warnings */
  lua_assert(lua55_gettop(L) == level);
  return 0;
}


static int upvalue (lua55_State *L) {
  int n = cast_int(lua55L_checkinteger(L, 2));
  lua55L_checktype(L, 1, LUA_TFUNCTION);
  if (lua55_isnone(L, 3)) {
    const char *name = lua55_getupvalue(L, 1, n);
    if (name == NULL) return 0;
    lua55_pushstring(L, name);
    return 2;
  }
  else {
    const char *name = lua55_setupvalue(L, 1, n);
    lua55_pushstring(L, name);
    return 1;
  }
}


static int newuserdata (lua55_State *L) {
  size_t size = cast_sizet(lua55L_optinteger(L, 1, 0));
  int nuv = cast_int(lua55L_optinteger(L, 2, 0));
  char *p = cast_charp(lua55_newuserdatauv(L, size, nuv));
  while (size--) *p++ = '\0';
  return 1;
}


static int pushuserdata (lua55_State *L) {
  lua_Integer u = lua55L_checkinteger(L, 1);
  lua55_pushlightuserdata(L, cast_voidp(cast_sizet(u)));
  return 1;
}


static int udataval (lua55_State *L) {
  lua55_pushinteger(L, cast_st2S(cast_sizet(lua55_touserdata(L, 1))));
  return 1;
}


static int doonnewstack (lua55_State *L) {
  lua55_State *L1 = lua55_newthread(L);
  size_t l;
  const char *s = lua55L_checklstring(L, 1, &l);
  int status = lua55L_loadbuffer(L1, s, l, s);
  if (status == LUA_OK)
    status = lua55_pcall(L1, 0, 0, 0);
  lua55_pushinteger(L, status);
  return 1;
}


static int s2d (lua55_State *L) {
  lua55_pushnumber(L, cast_num(*cast(const double *, lua55L_checkstring(L, 1))));
  return 1;
}


static int d2s (lua55_State *L) {
  double d = cast(double, lua55L_checknumber(L, 1));
  lua55_pushlstring(L, cast_charp(&d), sizeof(d));
  return 1;
}


static int num2int (lua55_State *L) {
  lua55_pushinteger(L, lua55_tointeger(L, 1));
  return 1;
}


static int makeseed (lua55_State *L) {
  lua55_pushinteger(L, cast_Integer(lua55L_makeseed(L)));
  return 1;
}


static int newstate (lua55_State *L) {
  void *ud;
  lua_Alloc f = lua55_getallocf(L, &ud);
  lua55_State *L1 = lua55_newstate(f, ud, 0);
  if (L1) {
    lua55_atpanic(L1, tpanic);
    lua55_pushlightuserdata(L, L1);
  }
  else
    lua55_pushnil(L);
  return 1;
}


static lua55_State *getstate (lua55_State *L) {
  lua55_State *L1 = cast(lua55_State *, lua55_touserdata(L, 1));
  lua55L_argcheck(L, L1 != NULL, 1, "state expected");
  return L1;
}


static int loadlib (lua55_State *L) {
  lua55_State *L1 = getstate(L);
  int load = cast_int(lua55L_checkinteger(L, 2));
  int preload = cast_int(lua55L_checkinteger(L, 3));
  lua55L_openselectedlibs(L1, load, preload);
  lua55L_requiref(L1, "T", luaB_opentests, 0);
  lua_assert(lua55_type(L1, -1) == LUA_TTABLE);
  /* 'requiref' should not reload module already loaded... */
  lua55L_requiref(L1, "T", NULL, 1);  /* seg. fault if it reloads */
  /* ...but should return the same module */
  lua_assert(lua55_compare(L1, -1, -2, LUA_OPEQ));
  return 0;
}

static int closestate (lua55_State *L) {
  lua55_State *L1 = getstate(L);
  lua55_close(L1);
  return 0;
}

static int doremote (lua55_State *L) {
  lua55_State *L1 = getstate(L);
  size_t lcode;
  const char *code = lua55L_checklstring(L, 2, &lcode);
  int status;
  lua55_settop(L1, 0);
  status = lua55L_loadbuffer(L1, code, lcode, code);
  if (status == LUA_OK)
    status = lua55_pcall(L1, 0, LUA_MULTRET, 0);
  if (status != LUA_OK) {
    lua55_pushnil(L);
    lua55_pushstring(L, lua55_tostring(L1, -1));
    lua55_pushinteger(L, status);
    return 3;
  }
  else {
    int i = 0;
    while (!lua55_isnone(L1, ++i))
      lua55_pushstring(L, lua55_tostring(L1, i));
    lua55_pop(L1, i-1);
    return i-1;
  }
}


static int log2_aux (lua55_State *L) {
  unsigned int x = (unsigned int)lua55L_checkinteger(L, 1);
  lua55_pushinteger(L, luaO_ceillog2(x));
  return 1;
}


struct Aux { jmp_buf jb; const char *paniccode; lua55_State *L; };

/*
** does a long-jump back to "main program".
*/
static int panicback (lua55_State *L) {
  struct Aux *b;
  lua55_checkstack(L, 1);  /* open space for 'Aux' struct */
  lua55_getfield(L, LUA_REGISTRYINDEX, "_jmpbuf");  /* get 'Aux' struct */
  b = (struct Aux *)lua55_touserdata(L, -1);
  lua55_pop(L, 1);  /* remove 'Aux' struct */
  runC(b->L, L, b->paniccode);  /* run optional panic code */
  longjmp(b->jb, 1);
  return 1;  /* to avoid warnings */
}

static int checkpanic (lua55_State *L) {
  struct Aux b;
  void *ud;
  lua55_State *L1;
  const char *code = lua55L_checkstring(L, 1);
  lua_Alloc f = lua55_getallocf(L, &ud);
  b.paniccode = lua55L_optstring(L, 2, "");
  b.L = L;
  L1 = lua55_newstate(f, ud, 0);  /* create new state */
  if (L1 == NULL) {  /* error? */
    lua55_pushstring(L, MEMERRMSG);
    return 1;
  }
  lua55_atpanic(L1, panicback);  /* set its panic function */
  lua55_pushlightuserdata(L1, &b);
  lua55_setfield(L1, LUA_REGISTRYINDEX, "_jmpbuf");  /* store 'Aux' struct */
  if (setjmp(b.jb) == 0) {  /* set jump buffer */
    runC(L, L1, code);  /* run code unprotected */
    lua55_pushliteral(L, "no errors");
  }
  else {  /* error handling */
    /* move error message to original state */
    lua55_pushstring(L, lua55_tostring(L1, -1));
  }
  lua55_close(L1);
  return 1;
}


static int externKstr (lua55_State *L) {
  size_t len;
  const char *s = lua55L_checklstring(L, 1, &len);
  lua55_pushexternalstring(L, s, len, NULL, NULL);
  return 1;
}


/*
** Create a buffer with the content of a given string and then
** create an external string using that buffer. Use the allocation
** function from Lua to create and free the buffer.
*/
static int externstr (lua55_State *L) {
  size_t len;
  const char *s = lua55L_checklstring(L, 1, &len);
  void *ud;
  lua_Alloc allocf = lua55_getallocf(L, &ud);  /* get allocation function */
  /* create the buffer */
  char *buff = cast_charp((*allocf)(ud, NULL, 0, len + 1));
  if (buff == NULL) {  /* memory error? */
    lua55_pushliteral(L, "not enough memory");
    lua55_error(L);  /* raise a memory error */
  }
  /* copy string content to buffer, including ending 0 */
  memcpy(buff, s, (len + 1) * sizeof(char));
  /* create external string */
  lua55_pushexternalstring(L, buff, len, allocf, ud);
  return 1;
}


/*
** {====================================================================
** function to test the API with C. It interprets a kind of assembler
** language with calls to the API, so the test can be driven by Lua code
** =====================================================================
*/


static void sethookaux (lua55_State *L, int mask, int count, const char *code);

static const char *const delimits = " \t\n,;";

static void skip (const char **pc) {
  for (;;) {
    if (**pc != '\0' && strchr(delimits, **pc)) (*pc)++;
    else if (**pc == '#') {  /* comment? */
      while (**pc != '\n' && **pc != '\0') (*pc)++;  /* until end-of-line */
    }
    else break;
  }
}

static int getnum_aux (lua55_State *L, lua55_State *L1, const char **pc) {
  int res = 0;
  int sig = 1;
  skip(pc);
  if (**pc == '.') {
    res = cast_int(lua55_tointeger(L1, -1));
    lua55_pop(L1, 1);
    (*pc)++;
    return res;
  }
  else if (**pc == '*') {
    res = lua55_gettop(L1);
    (*pc)++;
    return res;
  }
  else if (**pc == '!') {
    (*pc)++;
    if (**pc == 'G')
      res = LUA_RIDX_GLOBALS;
    else if (**pc == 'M')
      res = LUA_RIDX_MAINTHREAD;
    else lua_assert(0);
    (*pc)++;
    return res;
  }
  else if (**pc == '-') {
    sig = -1;
    (*pc)++;
  }
  if (!lisdigit(cast_uchar(**pc)))
    lua55L_error(L, "number expected (%s)", *pc);
  while (lisdigit(cast_uchar(**pc))) res = res*10 + (*(*pc)++) - '0';
  return sig*res;
}

static const char *getstring_aux (lua55_State *L, char *buff, const char **pc) {
  int i = 0;
  skip(pc);
  if (**pc == '"' || **pc == '\'') {  /* quoted string? */
    int quote = *(*pc)++;
    while (**pc != quote) {
      if (**pc == '\0') lua55L_error(L, "unfinished string in C script");
      buff[i++] = *(*pc)++;
    }
    (*pc)++;
  }
  else {
    while (**pc != '\0' && !strchr(delimits, **pc))
      buff[i++] = *(*pc)++;
  }
  buff[i] = '\0';
  return buff;
}


static int getindex_aux (lua55_State *L, lua55_State *L1, const char **pc) {
  skip(pc);
  switch (*(*pc)++) {
    case 'R': return LUA_REGISTRYINDEX;
    case 'U': return lua55_upvalueindex(getnum_aux(L, L1, pc));
    default: {
      int n;
      (*pc)--;  /* to read again */
      n = getnum_aux(L, L1, pc);
      if (n == 0) return 0;
      else return lua55_absindex(L1, n);
    }
  }
}


static const char *const statcodes[] = {"OK", "YIELD", "ERRRUN",
    "ERRSYNTAX", MEMERRMSG, "ERRERR"};

/*
** Avoid these stat codes from being collected, to avoid possible
** memory error when pushing them.
*/
static void regcodes (lua55_State *L) {
  unsigned int i;
  for (i = 0; i < sizeof(statcodes) / sizeof(statcodes[0]); i++) {
    lua55_pushboolean(L, 1);
    lua55_setfield(L, LUA_REGISTRYINDEX, statcodes[i]);
  }
}


#define EQ(s1)	(strcmp(s1, inst) == 0)

#define getnum		(getnum_aux(L, L1, &pc))
#define getstring	(getstring_aux(L, buff, &pc))
#define getindex	(getindex_aux(L, L1, &pc))


static int testC (lua55_State *L);
static int Cfunck (lua55_State *L, int status, lua_KContext ctx);

/*
** arithmetic operation encoding for 'arith' instruction
** LUA_OPIDIV  -> \
** LUA_OPSHL   -> <
** LUA_OPSHR   -> >
** LUA_OPUNM   -> _
** LUA_OPBNOT  -> !
*/
static const char ops[] = "+-*%^/\\&|~<>_!";

static int runC (lua55_State *L, lua55_State *L1, const char *pc) {
  char buff[300];
  int status = 0;
  if (pc == NULL) return lua55L_error(L, "attempt to runC null script");
  for (;;) {
    const char *inst = getstring;
    if EQ("") return 0;
    else if EQ("absindex") {
      lua55_pushinteger(L1, getindex);
    }
    else if EQ("append") {
      int t = getindex;
      int i = cast_int(lua55_rawlen(L1, t));
      lua55_rawseti(L1, t, i + 1);
    }
    else if EQ("arith") {
      int op;
      skip(&pc);
      op = cast_int(strchr(ops, *pc++) - ops);
      lua55_arith(L1, op);
    }
    else if EQ("call") {
      int narg = getnum;
      int nres = getnum;
      lua55_call(L1, narg, nres);
    }
    else if EQ("callk") {
      int narg = getnum;
      int nres = getnum;
      int i = getindex;
      lua55_callk(L1, narg, nres, i, Cfunck);
    }
    else if EQ("checkstack") {
      int sz = getnum;
      const char *msg = getstring;
      if (*msg == '\0')
        msg = NULL;  /* to test 'lua55L_checkstack' with no message */
      lua55L_checkstack(L1, sz, msg);
    }
    else if EQ("rawcheckstack") {
      int sz = getnum;
      lua55_pushboolean(L1, lua55_checkstack(L1, sz));
    }
    else if EQ("compare") {
      const char *opt = getstring;  /* EQ, LT, or LE */
      int op = (opt[0] == 'E') ? LUA_OPEQ
                               : (opt[1] == 'T') ? LUA_OPLT : LUA_OPLE;
      int a = getindex;
      int b = getindex;
      lua55_pushboolean(L1, lua55_compare(L1, a, b, op));
    }
    else if EQ("concat") {
      lua55_concat(L1, getnum);
    }
    else if EQ("copy") {
      int f = getindex;
      lua55_copy(L1, f, getindex);
    }
    else if EQ("func2num") {
      lua_CFunction func = lua55_tocfunction(L1, getindex);
      lua55_pushinteger(L1, cast_st2S(cast_sizet(func)));
    }
    else if EQ("getfield") {
      int t = getindex;
      int tp = lua55_getfield(L1, t, getstring);
      lua_assert(tp == lua55_type(L1, -1));
    }
    else if EQ("getglobal") {
      lua55_getglobal(L1, getstring);
    }
    else if EQ("getmetatable") {
      if (lua55_getmetatable(L1, getindex) == 0)
        lua55_pushnil(L1);
    }
    else if EQ("gettable") {
      int tp = lua55_gettable(L1, getindex);
      lua_assert(tp == lua55_type(L1, -1));
    }
    else if EQ("gettop") {
      lua55_pushinteger(L1, lua55_gettop(L1));
    }
    else if EQ("gsub") {
      int a = getnum; int b = getnum; int c = getnum;
      lua55L_gsub(L1, lua55_tostring(L1, a),
                    lua55_tostring(L1, b),
                    lua55_tostring(L1, c));
    }
    else if EQ("insert") {
      lua55_insert(L1, getnum);
    }
    else if EQ("iscfunction") {
      lua55_pushboolean(L1, lua55_iscfunction(L1, getindex));
    }
    else if EQ("isfunction") {
      lua55_pushboolean(L1, lua55_isfunction(L1, getindex));
    }
    else if EQ("isnil") {
      lua55_pushboolean(L1, lua55_isnil(L1, getindex));
    }
    else if EQ("isnull") {
      lua55_pushboolean(L1, lua55_isnone(L1, getindex));
    }
    else if EQ("isnumber") {
      lua55_pushboolean(L1, lua55_isnumber(L1, getindex));
    }
    else if EQ("isstring") {
      lua55_pushboolean(L1, lua55_isstring(L1, getindex));
    }
    else if EQ("istable") {
      lua55_pushboolean(L1, lua55_istable(L1, getindex));
    }
    else if EQ("isudataval") {
      lua55_pushboolean(L1, lua55_islightuserdata(L1, getindex));
    }
    else if EQ("isuserdata") {
      lua55_pushboolean(L1, lua55_isuserdata(L1, getindex));
    }
    else if EQ("len") {
      lua55_len(L1, getindex);
    }
    else if EQ("Llen") {
      lua55_pushinteger(L1, lua55L_len(L1, getindex));
    }
    else if EQ("loadfile") {
      lua55L_loadfile(L1, lua55L_checkstring(L1, getnum));
    }
    else if EQ("loadstring") {
      size_t slen;
      const char *s = lua55L_checklstring(L1, getnum, &slen);
      const char *name = getstring;
      const char *mode = getstring;
      lua55L_loadbufferx(L1, s, slen, name, mode);
    }
    else if EQ("newmetatable") {
      lua55_pushboolean(L1, lua55L_newmetatable(L1, getstring));
    }
    else if EQ("newtable") {
      lua55_newtable(L1);
    }
    else if EQ("newthread") {
      lua55_newthread(L1);
    }
    else if EQ("resetthread") {
      lua55_pushinteger(L1, lua55_resetthread(L1));  /* deprecated */
    }
    else if EQ("newuserdata") {
      lua55_newuserdata(L1, cast_sizet(getnum));
    }
    else if EQ("next") {
      lua55_next(L1, -2);
    }
    else if EQ("objsize") {
      lua55_pushinteger(L1, l_castU2S(lua55_rawlen(L1, getindex)));
    }
    else if EQ("pcall") {
      int narg = getnum;
      int nres = getnum;
      status = lua55_pcall(L1, narg, nres, getnum);
    }
    else if EQ("pcallk") {
      int narg = getnum;
      int nres = getnum;
      int i = getindex;
      status = lua55_pcallk(L1, narg, nres, 0, i, Cfunck);
    }
    else if EQ("pop") {
      lua55_pop(L1, getnum);
    }
    else if EQ("printstack") {
      int n = getnum;
      if (n != 0) {
        lua_printvalue(s2v(L->ci->func.p + n));
        printf("\n");
      }
      else lua_printstack(L1);
    }
    else if EQ("print") {
      const char *msg = getstring;
      printf("%s\n", msg);
    }
    else if EQ("warningC") {
      const char *msg = getstring;
      lua55_warning(L1, msg, 1);
    }
    else if EQ("warning") {
      const char *msg = getstring;
      lua55_warning(L1, msg, 0);
    }
    else if EQ("pushbool") {
      lua55_pushboolean(L1, getnum);
    }
    else if EQ("pushcclosure") {
      lua55_pushcclosure(L1, testC, getnum);
    }
    else if EQ("pushint") {
      lua55_pushinteger(L1, getnum);
    }
    else if EQ("pushnil") {
      lua55_pushnil(L1);
    }
    else if EQ("pushnum") {
      lua55_pushnumber(L1, (lua_Number)getnum);
    }
    else if EQ("pushstatus") {
      lua55_pushstring(L1, statcodes[status]);
    }
    else if EQ("pushstring") {
      lua55_pushstring(L1, getstring);
    }
    else if EQ("pushupvalueindex") {
      lua55_pushinteger(L1, lua55_upvalueindex(getnum));
    }
    else if EQ("pushvalue") {
      lua55_pushvalue(L1, getindex);
    }
    else if EQ("pushfstringI") {
      lua55_pushfstring(L1, lua55_tostring(L, -2), (int)lua55_tointeger(L, -1));
    }
    else if EQ("pushfstringS") {
      lua55_pushfstring(L1, lua55_tostring(L, -2), lua55_tostring(L, -1));
    }
    else if EQ("pushfstringP") {
      lua55_pushfstring(L1, lua55_tostring(L, -2), lua55_topointer(L, -1));
    }
    else if EQ("rawget") {
      int t = getindex;
      lua55_rawget(L1, t);
    }
    else if EQ("rawgeti") {
      int t = getindex;
      lua55_rawgeti(L1, t, getnum);
    }
    else if EQ("rawgetp") {
      int t = getindex;
      lua55_rawgetp(L1, t, cast_voidp(cast_sizet(getnum)));
    }
    else if EQ("rawset") {
      int t = getindex;
      lua55_rawset(L1, t);
    }
    else if EQ("rawseti") {
      int t = getindex;
      lua55_rawseti(L1, t, getnum);
    }
    else if EQ("rawsetp") {
      int t = getindex;
      lua55_rawsetp(L1, t, cast_voidp(cast_sizet(getnum)));
    }
    else if EQ("remove") {
      lua55_remove(L1, getnum);
    }
    else if EQ("replace") {
      lua55_replace(L1, getindex);
    }
    else if EQ("resume") {
      int i = getindex;
      int nres;
      status = lua55_resume(lua55_tothread(L1, i), L, getnum, &nres);
    }
    else if EQ("traceback") {
      const char *msg = getstring;
      int level = getnum;
      lua55L_traceback(L1, L1, msg, level);
    }
    else if EQ("threadstatus") {
      lua55_pushstring(L1, statcodes[lua55_status(L1)]);
    }
    else if EQ("alloccount") {
      l_memcontrol.countlimit = cast_uint(getnum);
    }
    else if EQ("return") {
      int n = getnum;
      if (L1 != L) {
        int i;
        for (i = 0; i < n; i++) {
          int idx = -(n - i);
          switch (lua55_type(L1, idx)) {
            case LUA_TBOOLEAN:
              lua55_pushboolean(L, lua55_toboolean(L1, idx));
              break;
            default:
              lua55_pushstring(L, lua55_tostring(L1, idx));
              break;
          }
        }
      }
      return n;
    }
    else if EQ("rotate") {
      int i = getindex;
      lua55_rotate(L1, i, getnum);
    }
    else if EQ("setfield") {
      int t = getindex;
      const char *s = getstring;
      lua55_setfield(L1, t, s);
    }
    else if EQ("seti") {
      int t = getindex;
      lua55_seti(L1, t, getnum);
    }
    else if EQ("setglobal") {
      const char *s = getstring;
      lua55_setglobal(L1, s);
    }
    else if EQ("sethook") {
      int mask = getnum;
      int count = getnum;
      const char *s = getstring;
      sethookaux(L1, mask, count, s);
    }
    else if EQ("setmetatable") {
      int idx = getindex;
      lua55_setmetatable(L1, idx);
    }
    else if EQ("settable") {
      lua55_settable(L1, getindex);
    }
    else if EQ("settop") {
      lua55_settop(L1, getnum);
    }
    else if EQ("testudata") {
      int i = getindex;
      lua55_pushboolean(L1, lua55L_testudata(L1, i, getstring) != NULL);
    }
    else if EQ("error") {
      lua55_error(L1);
    }
    else if EQ("abort") {
      abort();
    }
    else if EQ("throw") {
#if defined(__cplusplus)
static struct X { int x; } x;
      throw x;
#else
      lua55L_error(L1, "C++");
#endif
      break;
    }
    else if EQ("tobool") {
      lua55_pushboolean(L1, lua55_toboolean(L1, getindex));
    }
    else if EQ("tocfunction") {
      lua55_pushcfunction(L1, lua55_tocfunction(L1, getindex));
    }
    else if EQ("tointeger") {
      lua55_pushinteger(L1, lua55_tointeger(L1, getindex));
    }
    else if EQ("tonumber") {
      lua55_pushnumber(L1, lua55_tonumber(L1, getindex));
    }
    else if EQ("topointer") {
      lua55_pushlightuserdata(L1, cast_voidp(lua55_topointer(L1, getindex)));
    }
    else if EQ("touserdata") {
      lua55_pushlightuserdata(L1, lua55_touserdata(L1, getindex));
    }
    else if EQ("tostring") {
      const char *s = lua55_tostring(L1, getindex);
      const char *s1 = lua55_pushstring(L1, s);
      cast_void(s1);  /* to avoid warnings */
      lua_longassert((s == NULL && s1 == NULL) || strcmp(s, s1) == 0);
    }
    else if EQ("Ltolstring") {
      lua55L_tolstring(L1, getindex, NULL);
    }
    else if EQ("type") {
      lua55_pushstring(L1, lua55L_typename(L1, getnum));
    }
    else if EQ("xmove") {
      int f = getindex;
      int t = getindex;
      lua55_State *fs = (f == 0) ? L1 : lua55_tothread(L1, f);
      lua55_State *ts = (t == 0) ? L1 : lua55_tothread(L1, t);
      int n = getnum;
      if (n == 0) n = lua55_gettop(fs);
      lua55_xmove(fs, ts, n);
    }
    else if EQ("isyieldable") {
      lua55_pushboolean(L1, lua55_isyieldable(lua55_tothread(L1, getindex)));
    }
    else if EQ("yield") {
      return lua55_yield(L1, getnum);
    }
    else if EQ("yieldk") {
      int nres = getnum;
      int i = getindex;
      return lua55_yieldk(L1, nres, i, Cfunck);
    }
    else if EQ("toclose") {
      lua55_toclose(L1, getnum);
    }
    else if EQ("closeslot") {
      lua55_closeslot(L1, getnum);
    }
    else if EQ("argerror") {
      int arg = getnum;
      lua55L_argerror(L1, arg, getstring);
    }
    else lua55L_error(L, "unknown instruction %s", buff);
  }
  return 0;
}


static int testC (lua55_State *L) {
  lua55_State *L1;
  const char *pc;
  if (lua55_isuserdata(L, 1)) {
    L1 = getstate(L);
    pc = lua55L_checkstring(L, 2);
  }
  else if (lua55_isthread(L, 1)) {
    L1 = lua55_tothread(L, 1);
    pc = lua55L_checkstring(L, 2);
  }
  else {
    L1 = L;
    pc = lua55L_checkstring(L, 1);
  }
  return runC(L, L1, pc);
}


static int Cfunc (lua55_State *L) {
  return runC(L, L, lua55_tostring(L, lua55_upvalueindex(1)));
}


static int Cfunck (lua55_State *L, int status, lua_KContext ctx) {
  lua55_pushstring(L, statcodes[status]);
  lua55_setglobal(L, "status");
  lua55_pushinteger(L, cast_Integer(ctx));
  lua55_setglobal(L, "ctx");
  return runC(L, L, lua55_tostring(L, cast_int(ctx)));
}


static int makeCfunc (lua55_State *L) {
  lua55L_checkstring(L, 1);
  lua55_pushcclosure(L, Cfunc, lua55_gettop(L));
  return 1;
}


/* }====================================================== */


/*
** {======================================================
** tests for C hooks
** =======================================================
*/

/*
** C hook that runs the C script stored in registry.C_HOOK[L]
*/
static void Chook (lua55_State *L, lua_Debug *ar) {
  const char *scpt;
  const char *const events [] = {"call", "ret", "line", "count", "tailcall"};
  lua55_getfield(L, LUA_REGISTRYINDEX, "C_HOOK");
  lua55_pushlightuserdata(L, L);
  lua55_gettable(L, -2);  /* get C_HOOK[L] (script saved by sethookaux) */
  scpt = lua55_tostring(L, -1);  /* not very religious (string will be popped) */
  lua55_pop(L, 2);  /* remove C_HOOK and script */
  lua55_pushstring(L, events[ar->event]);  /* may be used by script */
  lua55_pushinteger(L, ar->currentline);  /* may be used by script */
  runC(L, L, scpt);  /* run script from C_HOOK[L] */
}


/*
** sets 'registry.C_HOOK[L] = scpt' and sets 'Chook' as a hook
*/
static void sethookaux (lua55_State *L, int mask, int count, const char *scpt) {
  if (*scpt == '\0') {  /* no script? */
    lua55_sethook(L, NULL, 0, 0);  /* turn off hooks */
    return;
  }
  lua55_getfield(L, LUA_REGISTRYINDEX, "C_HOOK");  /* get C_HOOK table */
  if (!lua55_istable(L, -1)) {  /* no hook table? */
    lua55_pop(L, 1);  /* remove previous value */
    lua55_newtable(L);  /* create new C_HOOK table */
    lua55_pushvalue(L, -1);
    lua55_setfield(L, LUA_REGISTRYINDEX, "C_HOOK");  /* register it */
  }
  lua55_pushlightuserdata(L, L);
  lua55_pushstring(L, scpt);
  lua55_settable(L, -3);  /* C_HOOK[L] = script */
  lua55_sethook(L, Chook, mask, count);
}


static int sethook (lua55_State *L) {
  if (lua55_isnoneornil(L, 1))
    lua55_sethook(L, NULL, 0, 0);  /* turn off hooks */
  else {
    const char *scpt = lua55L_checkstring(L, 1);
    const char *smask = lua55L_checkstring(L, 2);
    int count = cast_int(lua55L_optinteger(L, 3, 0));
    int mask = 0;
    if (strchr(smask, 'c')) mask |= LUA_MASKCALL;
    if (strchr(smask, 'r')) mask |= LUA_MASKRET;
    if (strchr(smask, 'l')) mask |= LUA_MASKLINE;
    if (count > 0) mask |= LUA_MASKCOUNT;
    sethookaux(L, mask, count, scpt);
  }
  return 0;
}


static int coresume (lua55_State *L) {
  int status, nres;
  lua55_State *co = lua55_tothread(L, 1);
  lua55L_argcheck(L, co, 1, "coroutine expected");
  status = lua55_resume(co, L, 0, &nres);
  if (status != LUA_OK && status != LUA_YIELD) {
    lua55_pushboolean(L, 0);
    lua55_insert(L, -2);
    return 2;  /* return false + error message */
  }
  else {
    lua55_pushboolean(L, 1);
    return 1;
  }
}

#if !defined(LUA_USE_POSIX)

#define nonblock	NULL

#else

#include <unistd.h>
#include <fcntl.h>

static int nonblock (lua55_State *L) {
  FILE *f = cast(luaL_Stream*, lua55L_checkudata(L, 1, LUA_FILEHANDLE))->f;
  int fd = fileno(f);
  int flags = fcntl(fd, F_GETFL, 0);
  flags |= O_NONBLOCK;
  fcntl(fd, F_SETFL, flags);
  return 0;
}
#endif

/* }====================================================== */



static const struct luaL_Reg tests_funcs[] = {
  {"checkmemory", lua_checkmemory},
  {"closestate", closestate},
  {"d2s", d2s},
  {"doonnewstack", doonnewstack},
  {"doremote", doremote},
  {"gccolor", gc_color},
  {"gcage", gc_age},
  {"gcstate", gc_state},
  {"tracegc", tracegc},
  {"pobj", gc_printobj},
  {"getref", getref},
  {"hash", hash_query},
  {"log2", log2_aux},
  {"limits", get_limits},
  {"listcode", listcode},
  {"printcode", printcode},
  {"printallstack", lua_printallstack},
  {"listk", listk},
  {"listabslineinfo", listabslineinfo},
  {"listlocals", listlocals},
  {"loadlib", loadlib},
  {"checkpanic", checkpanic},
  {"newstate", newstate},
  {"newuserdata", newuserdata},
  {"num2int", num2int},
  {"makeseed", makeseed},
  {"pushuserdata", pushuserdata},
  {"gcquery", gc_query},
  {"querystr", string_query},
  {"querytab", table_query},
  {"codeparam", test_codeparam},
  {"applyparam", test_applyparam},
  {"ref", tref},
  {"resume", coresume},
  {"s2d", s2d},
  {"sethook", sethook},
  {"stacklevel", stacklevel},
  {"sizes", get_sizes},
  {"testC", testC},
  {"makeCfunc", makeCfunc},
  {"totalmem", mem_query},
  {"alloccount", alloc_count},
  {"allocfailnext", alloc_failnext},
  {"trick", settrick},
  {"udataval", udataval},
  {"unref", unref},
  {"upvalue", upvalue},
  {"externKstr", externKstr},
  {"externstr", externstr},
  {"nonblock", nonblock},
  {NULL, NULL}
};


static void checkfinalmem (void) {
  lua_assert(l_memcontrol.numblocks == 0);
  lua_assert(l_memcontrol.total == 0);
}


int luaB_opentests (lua55_State *L) {
  void *ud;
  lua_Alloc f = lua55_getallocf(L, &ud);
  lua55_atpanic(L, &tpanic);
  lua55_setwarnf(L, &warnf, L);
  lua55_pushboolean(L, 0);
  lua55_setglobal(L, "_WARN");  /* _WARN = false */
  regcodes(L);
  atexit(checkfinalmem);
  lua_assert(f == debug_realloc && ud == cast_voidp(&l_memcontrol));
  lua55_setallocf(L, f, ud);  /* exercise this function */
  lua55L_newlib(L, tests_funcs);
  return 1;
}

#endif

