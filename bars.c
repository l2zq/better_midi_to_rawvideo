#include <errno.h>
#include "bars.h"
// clang-format off
bar_global    bars;
static size_t pool_left;
static Bar   *pool_beg, *pool_end, **pool_items;
// clang-format on
static inline Bar *bar_new() {
  if (pool_left > 0)
    return pool_items[--pool_left];
  else
    return malloc(sizeof(Bar));
}
static inline void bar_del(Bar *bar) {
  if (pool_beg <= bar && bar < pool_end)
    pool_items[pool_left++] = bar;
  else
    free(bar);
}
int bars_free(void) {
  for (int i = 0; i < 128; i++) {
    Bar *bar;
    Bar *next = bars.keys[i].head.next;
    while ((bar = next)) {
      next = bar->next;
      if (bar < pool_beg || bar >= pool_end)
        free(bar);
    }
  }
  free(pool_beg);
  free(pool_items);
  return 0;
}
int bars_init(size_t pool_size) {
  int tmp_errno;
  if ((pool_beg = malloc(sizeof(Bar) * pool_size)) == NULL && pool_size > 0)
    return -1;
  if ((pool_items = malloc(sizeof(Bar *) * pool_size)) == NULL && pool_size > 0) {
    tmp_errno = errno;
    free(pool_beg);
    errno = tmp_errno;
    return -1;
  }
  pool_end = pool_beg;
  pool_left = pool_size;
  for (size_t i = 0; i < pool_size; i++)
    pool_items[i] = pool_end++;
  KBarList *kbls = bars.keys;
  for (int i = 0; i < 128; i++) {
    kbls->head.next = NULL;
    kbls->tail = &(kbls->head);
    kbls++;
  }
  return 0;
}
Bar *bars_add_bar(byte key, tk_t time, bool nend_old, bool nbeg_new) {
  KBarList *kbls = bars.keys + key;
  Bar *nbar = bar_new(), *tail = kbls->tail;
  tail->next = nbar;
  nbar->next = NULL;
  kbls->tail = nbar;
  tail->nend = nend_old ? time : TICK_INF;
  nbar->nbeg = nbeg_new ? time : 0;
  tail->bend = time;
  nbar->bend = TICK_INF;
  nbar->nend = TICK_INF; // new note bar, not yet end
  return nbar;
}
Bar *bars_del_bef(KBarList *kbls) {
  Bar *ptr;
  Bar *nex = kbls->head.next;
  while ((ptr = nex))
    if (ptr->bend <= bars.screen_bot) {
      nex = ptr->next;
      bar_del(ptr);
    } else
      break;
  if ((kbls->head.next = nex) == NULL)
    kbls->tail = &(kbls->head);
  return nex;
}