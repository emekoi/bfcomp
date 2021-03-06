#pragma once

#ifdef __GNUC__
/* if this is getting optimized away, try adding volatile to the int */
#define defer_if_(cond, sym)                                                   \
  auto void _dtor1_##sym();                                                    \
  auto void _dtor2_##sym();                                                    \
  int __attribute__((cleanup(_dtor2_##sym))) _dtorV_##sym;                     \
  void _dtor2_##sym() {                                                        \
    if (cond)                                                                  \
      _dtor1_##sym();                                                          \
  };                                                                           \
  void _dtor1_##sym()
#define defer_if__(cond, sym) defer_if_(cond, sym)
#define defer defer_if__(1, __COUNTER__)
#define defer_if(cond) defer_if__(cond, __COUNTER__)
#else
#warning defer and defer_if are unsupported
#define defer_if(x)
#define defer
#endif

#include <stdio.h>
#include <stdlib.h>

#define defer_var(fn) __attribute__((cleanup(fn)))
#define auto_fn_decl(fn) void auto_##fn(void *data)
#define auto_fn(fn)                                                            \
  void auto_##fn(void *data) {                                                 \
    if (*(void **)data)                                                        \
      fn(*(void **)data);                                                      \
  }

auto_fn_decl(free);
auto_fn_decl(fclose);

#ifdef DEFER_IMPL
auto_fn(free);
auto_fn(fclose);
#endif
