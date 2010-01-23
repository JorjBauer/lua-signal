/*
 * lsignal.c -- Signal Handler Library for Lua
 *
 * Copyright (C) 2010  Patrick J. Donnelly (batrick@batbytes.com)
 *
 * This software is distributed under the same license as Lua 5.0:
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE. 
*/

#define LIB_NAME          "signal"
#define LUA_SIGNAL_ERROR  1

#define INCLUDE_KILL (defined(_POSIX_SOURCE) || define(sun) || defined(__sun))

#include <lua.h>
#include <lauxlib.h>

#include <errno.h>
#include <signal.h>
#include <string.h>

struct lua_signal
{
  char *name; /* name of the signal */
  int sig; /* the signal */
};

/*
struct signal_stack_element {
  lua_Hook hook;
  int mask;
  int count;
  int sig;
};
*/

static const struct lua_signal lua_signals[] = {
  /* ANSI C signals */
#ifdef SIGABRT
  {"SIGABRT", SIGABRT},
#endif
#ifdef SIGFPE
  {"SIGFPE", SIGFPE},
#endif
#ifdef SIGILL
  {"SIGILL", SIGILL},
#endif
#ifdef SIGINT
  {"SIGINT", SIGINT},
#endif
#ifdef SIGSEGV
  {"SIGSEGV", SIGSEGV},
#endif
#ifdef SIGTERM
  {"SIGTERM", SIGTERM},
#endif
  /* posix signals */
#ifdef SIGHUP
  {"SIGHUP", SIGHUP},
#endif
#ifdef SIGQUIT
  {"SIGQUIT", SIGQUIT},
#endif
#ifdef SIGTRAP
  {"SIGTRAP", SIGTRAP},
#endif
#ifdef SIGKILL
  {"SIGKILL", SIGKILL},
#endif
#ifdef SIGUSR1
  {"SIGUSR1", SIGUSR1},
#endif
#ifdef SIGUSR2
  {"SIGUSR2", SIGUSR2},
#endif
#ifdef SIGPIPE
  {"SIGPIPE", SIGPIPE},
#endif
#ifdef SIGALRM
  {"SIGALRM", SIGALRM},
#endif
#ifdef SIGCHLD
  {"SIGCHLD", SIGCHLD},
#endif
#ifdef SIGCONT
  {"SIGCONT", SIGCONT},
#endif
#ifdef SIGSTOP
  {"SIGSTOP", SIGSTOP},
#endif
#ifdef SIGTTIN
  {"SIGTTIN", SIGTTIN},
#endif
#ifdef SIGTTOU
  {"SIGTTOU", SIGTTOU},
#endif
  /* some BSD signals */
#ifdef SIGIOT
  {"SIGIOT", SIGIOT},
#endif
#ifdef SIGBUS
  {"SIGBUS", SIGBUS},
#endif
#ifdef SIGCLD
  {"SIGCLD", SIGCLD},
#endif
#ifdef SIGURG
  {"SIGURG", SIGURG},
#endif
#ifdef SIGXCPU
  {"SIGXCPU", SIGXCPU},
#endif
#ifdef SIGXFSZ
  {"SIGXFSZ", SIGXFSZ},
#endif
#ifdef SIGVTALRM
  {"SIGVTALRM", SIGVTALRM},
#endif
#ifdef SIGPROF
  {"SIGPROF", SIGPROF},
#endif
#ifdef SIGWINCH
  {"SIGWINCH", SIGWINCH},
#endif
#ifdef SIGPOLL
  {"SIGPOLL", SIGPOLL},
#endif
#ifdef SIGIO
  {"SIGIO", SIGIO},
#endif
  /* add odd signals */
#ifdef SIGSTKFLT
  {"SIGSTKFLT", SIGSTKFLT}, /* stack fault */
#endif
#ifdef SIGSYS
  {"SIGSYS", SIGSYS},
#endif
  {NULL, 0}
};

static int signal_stack[4096];
/*
static int *signal_stack = NULL;
static size_t signal_stack_n = 0;

static void *srealloc (void *pold, size_t nsize)
{
  void *pnew = realloc(pold, nsize);
  if (pnew == NULL && nsize > 0)
    exit(LUA_SIGNAL_ERROR);
  return pnew;
}
*/

static void hook (lua_State *L, lua_Debug *ar)
{
  lua_pushstring(L, LUA_SIGNAL);
  lua_gettable(L, LUA_REGISTRYINDEX);
  lua_pushnumber(L, Nsig);
  lua_gettable(L, -2);

  lua_call(L, 0, 0);

  /* set the old hook */
  lua_sethook(L, Hsig, Hmask, Hcount);
}

static void handle (int sig)
{
  signal_stack = srealloc(signal_stack,
      sizeof(signal_stack_element) * ++signal_stack_n);

  signal_stack[signal_stack_n-1].hook = lua_gethook(Lsig);
  signal_stack[signal_stack_n-1].mask = lua_gethookmask(Lsig);
  signal_stack[signal_stack_n-1].count = lua_gethookcount(Lsig);
  signal_stack[signal_stack_n-1].sig = sig;

  lua_sethook(Lsig, hook, LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT, 1);
}

static int get_signal (lua_State *L, int idx)
{
  switch (lua_type(L, idx))
  {
    case LUA_TNUMBER:
      return (int) lua_tonumber(L, idx);
    case LUA_TSTRING:
      lua_pushvalue(L, idx);
      lua_rawget(L, LUA_ENVIRONINDEX);
      if (!lua_isnumber(L, -1))
        return luaL_argerror(L, idx, "invalid signal string");
      lua_replace(L, idx);
      return (int) lua_tonumber(L, idx);
    default:
      return luaL_argerror(L, idx, "expected signal string/number");
  }
}

/*
 * l_signal == signal(signal [, func])
 *
 * signal = signal number or string
 * func = Lua function to call
*/  

static int l_signal (lua_State *L)
{
  int sig = get_signal(L, 1);

  lua_pushvalue(L, 1);
  lua_rawget(L, LUA_ENVIRONINDEX); /* return old handler */

  /* set handler */
  if (lua_isnoneornil(L, 2)) /* clear handler */
  {
    lua_pushvalue(L, 1);
    lua_pushnil(L);
    lua_rawset(L, LUA_ENVIRONINDEX);
    signal(sig, SIG_DFL);
  } else
  {
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 1);
    lua_pushvalue(L, 2);
    lua_rawset(L, LUA_ENVIRONINDEX);

    if (signal(sig, handle) == SIG_ERR)
    {
      lua_pushnil(L);
      lua_pushstring(L, strerror(errno));
      return 2;
    }

    Lsig = L; /* Set the state for the handler */
  }
  return 1;
}

/*
 * l_raise == raise(signal)
 *
 * signal = signal number or string
*/  

static int l_raise (lua_State *L)
{
  lua_pushnumber(L, raise(get_signal(L, 1)));
  return 1;
}

#if INCLUDE_KILL

/* define some posix only functions */

/*
 * l_kill == kill(pid, signal)
 *
 * pid = process id
 * signal = signal number or string
*/  

static int l_kill (lua_State *L)
{
  lua_pushnumber(L, kill((pid_t) luaL_checknumber(L, 1), get_signal(L, 2)));
  return 1;
}

#endif

static int interrupted (lua_State *L)
{
  return luaL_error(L, "interrupted!");
}

int luaopen_signal (lua_State *L)
{
  static const struct luaL_Reg lib[] = {
    {"signal", l_signal},
    {"raise", l_raise},
#if INCLUDE_KILL
    {"kill", l_kill},
#endif
    {NULL, NULL}
  };

  int i = 0;

  /* environment */
  lua_newtable(L);
  lua_replace(L, LUA_ENVIRONINDEX);

  /* Set the thread for our library, we hope this is the main thread.
   * This hook will propagate into other new threads.
   * We set a reasonable number of calls via byte code count.
   */
  lua_pushthread(L);
  lua_pushboolean(L, 1);
  lua_rawset(L, LUA_ENVIRONINDEX); /* prevent GC */
  lua_sethook(L, hook, LUA_MASKCOUNT, 1e5);

  /* add the library */
  luaL_register(L, LIB_NAME, lib);

  while (lua_signals[i].name != NULL)
  {
    /* environment table */
    lua_pushstring(L, lua_signals[i].name);
    lua_pushnumber(L, lua_signals[i].sig);
    lua_rawset(L, LUA_ENVIRONINDEX);
    /* signal table */
    lua_pushstring(L, lua_signals[i].name);
    lua_pushnumber(L, lua_signals[i].sig);
    lua_settable(L, -3);
    i++;
  }

  /* set default interrupt handler */
  lua_pushnumber(L, SIGINT);
  lua_pushcfunction(L, interrupted);
  lua_rawset(L, LUA_ENVIRONINDEX);
  signal(SIGINT, handle);

  return 1;
}
