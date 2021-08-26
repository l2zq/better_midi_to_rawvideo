#include "util.h"
#define TICK_INF 0x7fffffffffffffff
typedef struct Bar Bar;
struct Bar {
  Bar *next;
  tk_t bend;       //  bar end (beg = prev end, first beg = screen bottom)
  tk_t nbeg, nend; // note begin, end
  no_t n_id;
  ui16 trak;
  byte chan;
};
// nend > bend : no end border, nbeg < bbeg
typedef struct KBarList {
  Bar head, *tail;
} KBarList;

typedef struct bar_global {
  KBarList keys[128];
  tk_t screen_top, screen_bot;
} bar_global;
extern bar_global bars;
// clang-format off
int  bars_free(void);
int  bars_init(size_t pool_size);
Bar *bars_add_bar(byte key, tk_t time, bool nend_old, bool nbeg_new);
Bar *bars_del_bef(KBarList *kbls);